%{
#include "parser.tab.h"
#include "contrib/bstring/bstring/bstring.h"
#include "util/util.h"

#ifndef __cplusplus
#  define yyinput input
#endif

#define MK_BSTRING      b_fromblk(yytext, yyleng)
#define SETSTR          (yylval->TOK_CSTR = yytext)
#define SETCHAR         (yylval->TOK_CHAR = yytext[0])
#define UPDATE_COLUMN() (yyextra->column += yyleng)

#define SHUT_UP 1
#ifdef SHUT_UP
#  define ECHON UPDATE_COLUMN()
#else
#  define ECHON do { ECHO; putchar('\n'); UPDATE_COLUMN(); } while (0)
#endif

static bstring *handle_block_comment(yyscan_t scanner, ast_data *data);
static bstring *handle_unknown_content(const char *txt, unsigned len);
%}

%option reentrant bison-bridge noyywrap yylineno
%option extra-type="ast_data *"

id     [a-zA-Z_][a-zA-Z_0-9]*
xmlid  [a-zA-Z_][a-zA-Z_0-9:]*

int     [0-9]+
ws      [[:blank:]]
Dstring \"(\\\"|[^\"])*\"
Sstring \'(\\\'|[^\'])*\'
nl      (\n|\r\n)

blkc "/*"()

%%

^{ws}*{nl}              { ECHON; return BLANK_LINE; }
^{ws}*"//".*"\r\n"	{ ECHON; yylval->BSTRING = b_fromblk(yytext, yyleng-2); return BARE_LINE_COMMENT; }
^{ws}*"//".*"\n"	{ ECHON; yylval->BSTRING = b_fromblk(yytext, yyleng-1); return BARE_LINE_COMMENT; }
^{ws}*"**".*		{ ECHON; yylval->BSTRING = handle_unknown_content(yytext, yyleng); return UNKNOWN_XML_CONTENT; }
"//".*"\r\n"		{ ECHON; yylval->BSTRING = b_fromblk(yytext+2, yyleng-4); return LINE_COMMENT; }
"//".*"\n"		{ ECHON; yylval->BSTRING = b_fromblk(yytext+2, yyleng-3); return LINE_COMMENT; }
"/*"			{ ECHON; yylval->BSTRING = handle_block_comment(yyscanner, yyextra); return BLOCK_COMMENT; }

{ws}+			{ UPDATE_COLUMN(); }

{int}"min"		{ ECHON; yylval->BSTRING        = MK_BSTRING; return TIME_VAL; }
{int}"ms"		{ ECHON; yylval->BSTRING        = MK_BSTRING; return TIME_VAL; }
{int}"s"		{ ECHON; yylval->BSTRING        = MK_BSTRING; return TIME_VAL; }
{int}"m"		{ ECHON; yylval->BSTRING        = MK_BSTRING; return TOK_DISTANCE; }
{int}"km"		{ ECHON; yylval->BSTRING        = MK_BSTRING; return TOK_DISTANCE; }
{int}"Cr"		{ ECHON; yylval->BSTRING        = MK_BSTRING; return TOK_CREDITS; }
{int}"Deg"		{ ECHON; yylval->BSTRING        = MK_BSTRING; return TOK_DEGREES; }
{int}+"."{int}*"f"?	{ ECHON; yylval->TOK_FLOAT      = MK_BSTRING; return TOK_FLOAT; }
{int}f			{ ECHON; yylval->TOK_FLOAT      = MK_BSTRING; return TOK_FLOAT; }
{int}			{ ECHON; yylval->TOK_INTEGER    = MK_BSTRING; return TOK_INTEGER; }
{Dstring}		{ ECHON; yylval->STRING_UNIMPL  = MK_BSTRING; return STRING_UNIMPL; }
{Sstring}		{ ECHON; yylval->STRING_LITERAL = MK_BSTRING; return STRING_LITERAL; }

"min"			{ ECHON; SETSTR;  return TOK_MIN; }
"ms"			{ ECHON; SETSTR;  return TOK_MS; }
"km"			{ ECHON; SETSTR;  return TOK_KM; }
"Cr"			{ ECHON; SETSTR;  return TOK_CR; }
"Deg"			{ ECHON; SETSTR;  return TOK_DEG; }
"ct"			{ ECHON; SETSTR;  return TOK_CT; }
"m"			{ ECHON; SETSTR;  return 'm'; }
"f"			{ ECHON; SETCHAR; return 'f'; }
"i"			{ ECHON; SETCHAR; return 'i'; }
"s"			{ ECHON; SETCHAR; return 's'; }

"null"			{ ECHON; return TOK_NULL; }
"add"			{ ECHON; return TOK_ADD; }
"break"			{ ECHON; return TOK_BREAK; }
"chance"		{ ECHON; return TOK_CHANCE; }
"debug"			{ ECHON; return TOK_DEBUG; }
"else"			{ ECHON; return TOK_ELSE; }
"elsif"			{ ECHON; return TOK_ELSIF; }
"for"			{ ECHON; return TOK_FOR; }
"if"			{ ECHON; return TOK_IF; }
"in"			{ ECHON; return TOK_IN; }
"let"			{ ECHON; return TOK_LET; }
"return"		{ ECHON; return TOK_RETURN; }
"reversed"		{ ECHON; return TOK_REVERSED; }
"table"			{ ECHON; return TOK_TABLE; }
"then"			{ ECHON; return TOK_THEN; }
"undef"			{ ECHON; return TOK_UNDEF; }
"while"			{ ECHON; return TOK_WHILE; }

"[]"			{ ECHON; yylval->BSTRING    = MK_BSTRING; return TOK_EMPTY_ARRAY; }
"event"			{ ECHON; yylval->IDENTIFIER = MK_BSTRING; return TOK_CONST; }
"this"			{ ECHON; yylval->IDENTIFIER = MK_BSTRING; return TOK_CONST; }
"error"			{ ECHON; yylval->IDENTIFIER = MK_BSTRING; return TOK_CONST; }
"sqrt"			{ ECHON; yylval->IDENTIFIER = MK_BSTRING; return TOK_SQRT; }
"typeof"		{ ECHON; yylval->TOK_VAL    = TOK_TYPEOF; return TOK_TYPEOF; }

"$"{id}			{ ECHON; yylval->IDENTIFIER = MK_BSTRING; return VARIABLE; }
{xmlid}			{ 
    ECHON;
    yylval->IDENTIFIER = b_fromblk(yytext, yyleng);
    int64_t loc = b_strchr(yylval->IDENTIFIER, ':');
    if (loc > 0) {
        if (loc == yylval->IDENTIFIER->slen - 1) {
            yylval->IDENTIFIER->data[--yylval->IDENTIFIER->slen] = '\0';
            yyless(yylval->IDENTIFIER->slen);
        } else
            return XML_IDENTIFIER;
    }

    return IDENTIFIER;
}

">>"			{ ECHON; SETSTR; return OP_FILTER; }
"=="			{ ECHON; SETSTR; return OP_EQ; }
"!="			{ ECHON; SETSTR; return OP_NE; }
"<="			{ ECHON; SETSTR; return OP_LE; }
">="			{ ECHON; SETSTR; return OP_GE; }
"&&"			{ ECHON; SETSTR; return OP_AND; }
"||"			{ ECHON; SETSTR; return OP_OR; }
"=>"			{ ECHON; SETSTR; return OP_ARROW; }

"!"			{ ECHON; SETCHAR; return '!'; }
"$"			{ ECHON; SETCHAR; return '$'; }
"%"			{ ECHON; SETCHAR; return '%'; }
"("			{ ECHON; SETCHAR; return '('; }
")"			{ ECHON; SETCHAR; return ')'; }
"*"			{ ECHON; SETCHAR; return '*'; }
"+"			{ ECHON; SETCHAR; return '+'; }
","			{ ECHON; SETCHAR; return ','; }
"-"			{ ECHON; SETCHAR; return '-'; }
"."			{ ECHON; SETCHAR; return '.'; }
"/"			{ ECHON; SETCHAR; return '/'; }
":"			{ ECHON; SETCHAR; return ':'; }
";"			{ ECHON; SETCHAR; return ';'; }
"<"			{ ECHON; SETCHAR; return '<'; }
"="			{ ECHON; SETCHAR; return '='; }
">"			{ ECHON; SETCHAR; return '>'; }
"?"			{ ECHON; SETCHAR; return '?'; }
"@"			{ ECHON; SETCHAR; return '@'; }
"["			{ ECHON; SETCHAR; return '['; }
"]"			{ ECHON; SETCHAR; return ']'; }
"{"			{ ECHON; SETCHAR; return '{'; }
"}"			{ ECHON; SETCHAR; return '}'; }

{nl}			{ yyextra->column = 0; }
<<EOF>>			{ return TOK_EOF; }
.			{ ECHON; SETCHAR; return yytext[0]; }

%%

/*
"max"			{ ECHON; yylval->IDENTIFIER = MK_BSTRING; return TOK_MAX; }
"min"			{ ECHON; yylval->IDENTIFIER = MK_BSTRING; return TOK_MIN; }
"max:"			{ ECHON; yyunput(1, yytext, yyscanner); yylval->IDENTIFIER = b_fromlit("max"); return TOK_MAX; }
"min:"			{ ECHON; yyunput(1, yytext, yyscanner); yylval->IDENTIFIER = b_fromlit("min"); return TOK_MIN; }
.          { ((void)0); }
{int}"km"		{ ECHON; yylval->DISTANCE = (struct distance){ xatoi(yytext), DIST_KILO }; return DISTANCE; }
"@$"{id}		{ ECHON; yylval->IDENTIFIER = b_fromblk(yytext, yyleng); return VARIABLE_WPREFIX; }
"$"{id}"?"		{ ECHON; yylval->IDENTIFIER = b_fromblk(yytext, yyleng); return VARIABLE_WSUFFIX; }
*/

#include <termcap.h>
static char termbuf[2048];

static inline int
make_sep_line(int lengths[3], char *buf)
{
        int len  = MAX(lengths[0], lengths[1]);
        len      = MAX(len, lengths[2]);
        len      = MIN(len, 200);
        buf[0]   = 033;
        buf[1]   = '[';
        buf[2]   = '1';
        buf[3]   = 'm';

        for (size_t i = 0; i < len; ++i)
                buf[i+4] = '=';

        buf[len]   = 033;
        buf[len+1] = '[';
        buf[len+2] = '0';
        buf[len+3] = 'm';
        buf[len+4] = '\n';
        buf[len+5] = '\0';

        return len+5;
}

void
yyerror(yyscan_t scanner, ast_data *data, const char *msg)
{
        const size_t buflen = 16384;
        char buf[8192], out[buflen];
        int  num = 0, ch;

        while ((ch = yyinput(scanner)) != '\n')
                buf[num++] = ch;
        buf[num] = '\0';

        int lengths[3];
        int tot_len = 0;
        int column  = yyget_extra(scanner)->column;
        lengths[0]  = snprintf(out, buflen,
                               "\033[1;31mParsing Error:\033[37m at '\033[36m<HERE>\033[37m'"
                               " on line \033[32m%d\033[37m col \033[32m%d\033[0m '%s'\n",
                               yyget_lineno(scanner), column, BS(data->filename));
        tot_len    += lengths[0];
        lengths[1]  = snprintf(out+tot_len, buflen-tot_len,
                               "\033[0;33m%s\033[1;36m<HERE>\033[0;33m%s\033[0m\n",
                               yyget_text(scanner)-column+1, buf);
        tot_len    += lengths[1];
        lengths[2]  = snprintf(out+tot_len, buflen-tot_len,
                               "\033[1m--> \033[1;31m%s\033[0m\n", msg);
        tot_len    += lengths[2];
        lengths[0] -= 38;
        lengths[1] -= 22;
        lengths[2] -= 12;
        int len     = make_sep_line(lengths, buf);

        fputc('\n', stderr);
        fwrite(buf, 1, len, stderr);
        fwrite(out, 1, tot_len, stderr);
        fwrite(buf, 1, len, stderr);
        fputc('\n', stderr);
        fflush(stderr);

        yyunput(num, yyget_text(scanner), scanner);
}

static inline void
yyerror_fatal(yyscan_t scanner, ast_data *data, const char *msg)
{
        yyerror(scanner, NULL, msg);
        yy_fatal_error("Cannot continue: aborting.", scanner);
}

static bstring *
handle_block_comment(yyscan_t scanner, ast_data *data)
{
        int      ch;
        bool     nul  = false;
        bstring *text = b_create(80);

        for (;;) {
                ch = yyinput(scanner);
                if (!ch) {
                        if (nul)
                                break;
                        nul = true;
                        continue;
                }
                nul = false;
        /* while ((ch = input(scanner))) { */
                b_catchar(text, ch);
                if (ch == '*') {
                        while ((ch = yyinput(scanner)) == '*')
                                b_catchar(text, ch);
                        if (ch == '/') {
                                text->data[--text->slen] = '\0';
                                return text;
                        }
                        if (ch == '\0')
                                break;
                        b_catchar(text, ch);
                }
        }
        fprintf(stderr, "Got char %#02x\n%s\n", ch, yyget_text(scanner));

        b_destroy(text);
        yyerror_fatal(scanner, data, "Unterminated comment");
        return NULL;
}

static bstring *
handle_unknown_content(const char *txt, unsigned len)
{
        char buf[len+1];
        memcpy(buf, txt, len);
        buf[len] = 0;

        char *s = memchr(buf, '*', len) + 3;
        size_t loc = s - buf;
        return b_fromblk(s, len - loc);
}
