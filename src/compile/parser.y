%define api.pure full
%lex-param {void *scanner}
%parse-param {void *scanner}{ast_data *data}
%define parse.trace
%define parse.error verbose

%{
#include "Common.h"
#include "ast.h"
#include "my_p99_common.h"
#include "parser.tab.h"
#include "lexer.h"
#include <talloc.h>

extern void yyerror(yyscan_t scanner, ast_data *data, char const *msg);
extern void cat_relop(bstring *str, int op);
static void fix_line_comment(bstring *bstr);

#define B_CONCAT(b, s)                                 \
        do {                                           \
                b_catchar((b), ' ');                   \
                _Generic((s), bstring *: b_concat,     \
                              char *: b_catcstr,       \
                              const char *: b_catcstr, \
                              int: b_catchar           \
                        )((b), (s));                   \
        } while (0)

#define RESET_CUR() (data->cur = data->cur->parent)
%}

%code requires
{
#include "Common.h"
#include "ast.h"
}
%token TOK_EOF 0 "EOF"

%token OP_FILTER    ">>"
%token OP_ARROW     "=>"
%token TOK_ADD      "add"
%token TOK_BREAK    "break"
%token TOK_CHANCE   "chance"
%token TOK_DEBUG    "debug"
%token TOK_ELSE     "else"
%token TOK_ELSIF    "elsif"
%token TOK_FOR      "for"
%token TOK_IF       "if"
%token TOK_IN       "in"
%token TOK_LET      "let"
%token TOK_NULL     "NULL"
%token TOK_RETURN   "return"
%token TOK_THEN     "then"
%token TOK_UNDEF    "undef"
%token TOK_WHILE    "while"
%token TOK_REVERSED "reversed"
%token TOK_TABLE    "table"
%token BLANK_LINE


%token <bstring *> BARE_LINE_COMMENT "lone_line_comment"
%token <bstring *> BLOCK_COMMENT     "block_comment"
%token <bstring *> DISTANCE          "distance value"
%token <bstring *> LINE_COMMENT      "line_comment"
%token <bstring *> TIME_VAL          "time value"
%token <bstring *> TOK_CONST         "global_constant"
%token <bstring *> TOK_EMPTY_ARRAY   "[]"
%token <bstring *> TOK_SQRT          "sqrt"
%token <bstring *> VARIABLE          "$variable"

%token <int> '+' '-' '*' '/' '%' '^' '$' '!' '(' ')' '{' '}' ';' '.' '@' '[' ']' '?' '=' ',' ':'

%type <bstring *>    assignment_statement literal identifier_clash
                     additive_expression multiplicative_expression
                     unary_expression assignment_expression identifier_terminal
                     debug_print_statement relational_expression terminal
                     expression primary_expression builtin_function 
                     unary_expression2 relational_expression2 primary_expression2
                     identifier additive_expression2 multiplicative_expression2
                     struct_assignment table_assignment
%type <int>          unary_op multiplicative_op additive_op reversed inexplicable_f
%type <const char *> relational_op logical_op

/* These associations are required to avoid conflicts. */
%left ','
%precedence ':'
%precedence '='

%token OP_AND OP_OR OP_EQ OP_NE OP_GE OP_LE '<' '>'
%token <bstring *> TOK_MAX TOK_MIN

%define api.value.type union
%token <bstring *> INTEGER        "number"
%token <bstring *> FLOAT          "floating point number"
%token <bstring *> STRING_LITERAL "string literal"
%token <bstring *> STRING_UNIMPL  "string"
%token <bstring *> IDENTIFIER     "identifier"
%token <bstring *> XML_IDENTIFIER "xml identifier"
<bstring *> BSTRING
<char *>    CSTR
<int>       CHAR

%start statement_list;

/*======================================================================================*/
%%
/*======================================================================================*/

statement_list
	: statement { RESET_CUR(); }
	| statement { RESET_CUR(); } statement_list
	;

statement
	: BLOCK_COMMENT               { new_block_comment(data, $1); }
	| BARE_LINE_COMMENT           { fix_line_comment($1); new_block_comment(data, $1); }
	| BLANK_LINE                  { new_blank_line(data); }
	| statement_type LINE_COMMENT { append_line_comment(data, $2, 0); }
	| statement_type              { }
	;

statement_type
	: compound_statement
	| simple_statement        chance ';'
	| conditional_statement   chance { RESET_CUR(); } compound_statement
	| for_statement           chance { RESET_CUR(); } compound_statement
	| unimplemented_statement chance ';'
	| assignment_statement    chance ';'
	| debug_print_statement   chance ';'
	;

compound_statement
	: '{' line_comment { new_block(data); } statement_list '}'
	| '{' line_comment { new_block(data); } '}'
	;

chance
	: "chance" '(' expression ')' { append_chance(data, $3); }
	| %empty
	;

line_comment
	: LINE_COMMENT { append_line_comment(data, $1, 1); }
	| %empty
	;

/*======================================================================================*/
/* Unimplemented statements */

unimplemented_statement
	: IDENTIFIER { new_unimpl_statement(data, $1); } '<' unimplemented_expression '>'
	;

unimplemented_expression
	: unimplemented_subexpr
	| unimplemented_expression ',' unimplemented_subexpr
	| %empty
	;

unimplemented_subexpr 
	: IDENTIFIER     '=' STRING_UNIMPL  { new_unimpl_subexpr(data, $1, $3); }
	| XML_IDENTIFIER '=' STRING_UNIMPL  { new_unimpl_subexpr(data, $1, $3); }
	| TOK_CHANCE     '=' STRING_UNIMPL  { new_unimpl_subexpr(data, b_fromlit("chance"), $3); }
	| identifier_clash '=' STRING_UNIMPL  { new_unimpl_subexpr(data, $1, $3); }
	;

/*======================================================================================*/
/* Assignment */

assignment_statement
	: "let" identifier                                { new_assignment_statement(data, $2, NULL, ASSIGNMENT_NORMAL); }
	| "let" identifier '=' assignment_expression      { new_assignment_statement(data, $2, $4, ASSIGNMENT_NORMAL); }
	| "let" identifier "=>" '{' struct_assignment '}' { new_assignment_statement(data, $2, $5, ASSIGNMENT_SPECIAL); }
	| "let" identifier '=' "table" '[' table_assignment ']' { new_assignment_statement(data, $2, b_sprintf("table[ %s ]", $6), ASSIGNMENT_NORMAL); b_free($6); }
	| "add" identifier                                { new_assignment_statement(data, $2, NULL, ASSIGNMENT_ADD); }
	;

assignment_expression
	: "if" '(' expression ')' "then" expression "else" expression
		{ $$ = b_sprintf("if (%s) then %s else %s", $3, $6, $8); b_destroy_all($3, $6, $8); }
	/* | "table" '[' expression ']' { $$ = b_sprintf("table[%s]", $3); b_free($3); } */
	| expression { $$ = $1; }
	;

table_assignment
	: table_assignment ',' table_assignment { $$ = $1; B_CONCAT($$, $2); B_CONCAT($$, $3); b_free($3); }
	| identifier '=' expression             { $$ = $1; b_sprintfa($$, " = %s", $3); b_free($3); }
	| %empty                                { $$ = b_fromlit(""); }
	;

struct_assignment
	: struct_assignment ',' struct_assignment { $$ = $1; B_CONCAT($$, $3); b_free($3); }
	| identifier ':' expression               { $$ = $1; b_sprintfa($$, "=\"%s\"", $3); b_free($3); }
	;

/*======================================================================================*/
/* Conditionals */

conditional_statement
	: "if"    '(' expression ')' { new_conditional_statement(data, $3,   NODE_ST_IF); }
	| "elsif" '(' expression ')' { new_conditional_statement(data, $3,   NODE_ST_ELSIF); }
	| "else"                     { new_conditional_statement(data, NULL, NODE_ST_ELSE); }
	| "while" '(' expression ')' { new_conditional_statement(data, $3,   NODE_ST_WHILE); }
	;

for_statement
	: "for" '(' identifier "in" reversed expression ')'
		{ new_for_statement(data, $6, $3, $5); }
	;

reversed
	: "reversed" { $$ = 1; }
	| %empty     { $$ = 0; }
	;

/*======================================================================================*/
/* Other */

debug_print_statement
	: "debug" expression                     { new_debug_statement(data, $2, NULL); }
	| "debug" ">>" identifier ',' expression { new_debug_statement(data, $5, $3); }
	;

simple_statement
	: "return"           { new_simple_statement(data, b_fromlit("return"), NODE_ST_RETURN); }
	| "break"            { new_simple_statement(data, b_fromlit("break"),  NODE_ST_BREAK); }
	| "undef" identifier { new_undef_statement(data, $2); }
	;

/*======================================================================================*/
/* Expressions */

expression
	: builtin_function '(' expression ')' { $$ = $1; b_sprintfa($$, "(%s)", $3); }
	/* | expression ',' primary_expression { $$ = $1; if (add_comma) b_catchar($$, ','); B_CONCAT($$, $3); b_free($3); } */
	| expression ',' primary_expression { $$ = $1; b_catchar($$, ','); B_CONCAT($$, $3); b_free($3); }
	| expression '.' primary_expression { $$ = $1; b_catchar($$, '.'); b_concat($$, $3); b_free($3); }
	| primary_expression                { $$ = $1; }
	;

primary_expression
	: primary_expression logical_op relational_expression
		{ $$ = $1; B_CONCAT($$, $2); B_CONCAT($$, $3); b_free($3); }
	| relational_expression { $$ = $1; }
	;

relational_expression
	: relational_expression relational_op additive_expression
		{ $$ = $1; B_CONCAT($$, $2); B_CONCAT($$, $3); b_free($3); }
	| additive_expression { $$ = $1; }
	;

additive_expression 
	: additive_expression additive_op multiplicative_expression
		{ $$ = $1; B_CONCAT($$, $2); B_CONCAT($$, $3); b_free($3); }
	| multiplicative_expression { $$ = $1; }
	;

multiplicative_expression
	: multiplicative_expression multiplicative_op unary_expression 
		{ $$ = $1; B_CONCAT($$, $2); B_CONCAT($$, $3); b_free($3); }
	| unary_expression { $$ = $1; }
	;

unary_expression
	: unary_op unary_expression
		{ if ($1 == '!') { $$ = b_sprintf("not %s", $2); b_free($2); } else { b_insert_char($2, 0, $1); $$ = $2; } }
	| terminal { $$ = $1; }
	| literal  { $$ = $1; }
	;

/*======================================================================================*/

identifier
	: identifier '.' primary_expression2 { $$ = $1; b_catchar($$, '.'); b_concat($$, $3); b_free($3); }
	| primary_expression
	;

primary_expression2
	: primary_expression2 logical_op relational_expression2
		{ $$ = $1; B_CONCAT($$, $2); B_CONCAT($$, $3); b_free($3); }
	| relational_expression2 { $$ = $1; }
	;

relational_expression2
	: relational_expression2 relational_op additive_expression2
		{ $$ = $1; B_CONCAT($$, $2); B_CONCAT($$, $3); b_free($3); }
	| additive_expression2 { $$ = $1; }
	;

additive_expression2
	: additive_expression2 additive_op multiplicative_expression2
		{ $$ = $1; B_CONCAT($$, $2); B_CONCAT($$, $3); b_free($3); }
	| multiplicative_expression2 { $$ = $1; }
	;

multiplicative_expression2
	: multiplicative_expression2 multiplicative_op unary_expression2
		{ $$ = $1; B_CONCAT($$, $2); B_CONCAT($$, $3); b_free($3); }
	| unary_expression2 { $$ = $1; }
	;

unary_expression2
	: unary_op unary_expression2
		{ if ($1 == '!') { $$ = b_sprintf("not %s", $2); b_free($2); } else { b_insert_char($2, 0, $1); $$ = $2; } }
	| terminal { $$ = $1; }
	;

/*======================================================================================*/
/* Tokens */

terminal
	: '(' expression ')' inexplicable_f  { $$ = $2; b_insert_char($$, 0, $1); b_catchar($$, $3); if ($4) b_catchar($$, 'f'); }
	| '[' expression ']'  { $$ = $2; b_insert_char($$, 0, $1); b_catchar($$, $3); }
	| '{' expression '}'  { $$ = $2; b_insert_char($$, 0, $1); b_catchar($$, $3); }
	| identifier_terminal { $$ = $1; }
	| terminal '?'        { $$ = $1; b_catchar($$, '?'); }
	;

literal
	: STRING_LITERAL     { $$ = $1; }
	| FLOAT              { $$ = $1; }
	| INTEGER            { $$ = $1; }
	| TOK_NULL           { $$ = NULL; }
	| DISTANCE
	| TIME_VAL
	| TOK_EMPTY_ARRAY
	;

identifier_terminal
	: IDENTIFIER
	| TOK_CONST
	| VARIABLE
	| identifier_clash
	;

inexplicable_f : 'f' { $$ = 1; } | %empty { $$ = 0; } ;
identifier_clash: TOK_MIN | TOK_MAX ; 

builtin_function  : TOK_SQRT ;
additive_op       : '+' | '-' ;
unary_op          : '+' | '-' | '@' | '!' { $$ = '!'; } ;
multiplicative_op : '^' | '*' | '/' | '%' ;

relational_op     : OP_EQ  { $$ = "=="; }  | OP_NE { $$ = "!="; }
                  | OP_LE  { $$ = "le"; }  | OP_GE { $$ = "ge"; }
                  | '<'    { $$ = "lt"; }  | '>'   { $$ = "gt"; } ;
logical_op        : OP_AND { $$ = "and"; } | OP_OR { $$ = "or"; } ;


/*======================================================================================*/
%%
/*======================================================================================*/

static void
fix_line_comment(bstring *bstr)
{
        unsigned i;
        for (i = 0; i < bstr->slen; ++i)
                if (!isspace(bstr->data[i]))
                        break;
        i += 2;
        memmove(bstr->data, bstr->data + i, bstr->slen + 1 - i);
        bstr->slen -= i - 1;
        bstr->data[bstr->slen - 1] = ' ';
        bstr->data[bstr->slen]     = '\0';
}

// vim: noexpandtab tw=0
