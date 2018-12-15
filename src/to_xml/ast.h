#ifndef MYAST_H_
#define MYAST_H_

#include "Common.h"
#include "contrib/p99/p99.h"
#include "contrib/p99/p99_enum.h"
#include "util/list.h"

__BEGIN_DECLS
/*======================================================================================*/

/* typedef enum xs_context_type xs_context_type; */
P99_DECLARE_STRUCT(ast_data);
P99_DECLARE_STRUCT(ast_node);
P99_DECLARE_STRUCT(ast_atom);

P99_DECLARE_ENUM(ast_node_types,
        NODE_UNKNOWN,
        NODE_BLANK_LINE,
        NODE_BLOCK,
        NODE_COMMENT,
        NODE_ST_UNIMPL,
        NODE_ST_ASSIGN,
        NODE_ST_ASSIGN_SPECIAL,
        NODE_ST_IF,
        NODE_ST_ELSIF,
        NODE_ST_ELSE,
        NODE_ST_WHILE,
        NODE_ST_FOR,
        NODE_ST_DEBUG_TEXT,
        NODE_ST_RETURN,
        NODE_ST_BREAK,
        NODE_ST_UNDEF,
        NODE_ST_FOR_RANGE
);

enum ast_assignment_type {
        ASSIGNMENT_NORMAL,
        ASSIGNMENT_SPECIAL,
        ASSIGNMENT_LIST,
        ASSIGNMENT_NIL,
        ASSIGNMENT_ADD,
        ASSIGNMENT_SUBTRACT,
        ASSIGNMENT_INSERT,
};

struct ast_data {
        ast_node *cur;
        ast_node *top;
        struct fp_wrap_s {
                FILE *fp;
        } *fp_wrap;

        bstring  *filename;
        uint32_t      mask;
        uint32_t      column;
};

struct xs_range {
        bstring *min;
        bstring *max;
        bstring *prof;
};

struct ast_node {
        ast_node *parent;
        bstring  *line_comment;
        bstring  *chance;
        bstring  *chance_name;
        union {
                bstring *string; /* Generic */
                bstring *comment;
                struct {
                        bstring *keyword;
                        bstring *extra;
                } simple;
                struct {
                        uint8_t type;
                        union {
                                struct {
                                        bstring *expr;
                                        bstring *exact;
                                        bool     negate;
                                };
                                struct {
                                        bstring         *var;
                                        struct xs_range *rng;
                                };
                        };
                } condition;
                struct {
                        bstring *name;
                        genlist *list;
                } block;
                struct {
                        genlist *list;
                        bstring *id;
                } unimpl;
                struct {
                        bstring *var;
                        bstring *expr;
                        enum ast_assignment_type type;
                } assignment;
                struct {
                        bstring *text;
                        bstring *filter;
                } debug;
                struct {
                        bstring *counter;
                        bstring *expression;
                        bool     reversed;
                } forstmt;
                struct {
                        bstring         *counter;
                        struct xs_range *rng;
                        bool             reversed;
                } for_range_stmt;
        };
        enum ast_node_types type;
        uint16_t            depth;
        bool                block_parent;
};

enum ast_atom_types {
        AT_UNKNOWN,
        AT_UNIMPL_ID,
        AT_UNIMPL_ARG,
        AT_KEYWORD,
        AT_ASSIGNMENT_LVAL,
        AT_ASSIGNMENT_RVAL,
};

struct ast_atom {
        enum ast_atom_types type;
        union {
                int      none;
                bstring *identifier;
                struct unimplemented_subexpr {
                        bstring *id;
                        bstring *text;
                } unimpl;
        };
};

enum distance_type { DIST_METER, DIST_KILO };
struct distance {
        int64_t            n;
        enum distance_type type;
};

/*======================================================================================*/

ast_data * ast_data_create(const char *src);
ast_node * ast_node_create(ast_data *data, enum ast_node_types type);

/*======================================================================================*/

struct if_expression {
        bstring         *id1;
        bstring         *id2;
        bool     negate;
        int type;
};

extern void new_blank_line(ast_data *data);
extern void new_unimpl_statement(ast_data *data, bstring *id);
extern void new_unimpl_subexpr(ast_data *data, bstring *id, bstring *statement);
extern void new_unknown_statement(ast_data *data, bstring *statement);
extern void new_block(ast_data *data);
extern void new_block_comment(ast_data *data, bstring *text);
extern void new_assignment_statement(ast_data *data, bstring *var, bstring *expr, enum ast_assignment_type type);
extern void new_conditional_statement(ast_data *data, struct if_expression *expr, int type);
extern void new_debug_statement(ast_data *data, bstring *text, bstring *filter);
extern void new_simple_statement(ast_data *data, bstring *keyword, bstring *extra, int type);
extern void new_undef_statement(ast_data *data, bstring *var);
extern void new_for_statement(ast_data *data, bstring *var, bstring *ident, int reversed);
extern void new_range_statement(ast_data *data, bstring *counter, bstring *min, bstring *max, bstring *prof, bool reversed);

extern void append_chance(ast_data *data, bstring *expr, bstring *name);
extern void append_line_comment(ast_data *data, bstring *text, bool prev);

/*======================================================================================*/
__END_DECLS
#endif /* MyAst.h */
