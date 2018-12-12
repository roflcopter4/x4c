#ifndef MYAST_H_
#define MYAST_H_

#include "Common.h"
#include "contrib/p99/p99.h"
#include "contrib/p99/p99_enum.h"
#include "util/list.h"

__BEGIN_DECLS
/*======================================================================================*/

enum ast_data_types { COMPDATA_FILE, COMPDATA_FILENAME, COMPDATA_STRING };

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
        NODE_ST_UNDEF
);

enum ast_assignment_type {
        ASSIGNMENT_NORMAL,
        ASSIGNMENT_SPECIAL,
        ASSIGNMENT_NIL,
        ASSIGNMENT_ADD,
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

struct ast_node {
        ast_node *parent;
        bstring  *chance;
        bstring  *line_comment;
        union {
                bstring *string; /* Generic */
                bstring *comment;
                bstring *condition;
                bstring *keyword;
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
                        bstring *var;
                        bstring *ident;
                        bool     reversed;
                } forstmt;
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

#define ast_data_create(...)                                            \
        (P99_IF_EMPTY(__VA_ARGS__)                                       \
                (ast_data_create_(stdin, COMPDATA_FILE))                \
                (ast_data_create_((__VA_ARGS__),                        \
                                   _Generic((__VA_ARGS__),               \
                                            char *: COMPDATA_FILENAME, const char *: COMPDATA_FILENAME,   \
                                            FILE *: COMPDATA_FILE,       \
                                            bstring *:COMPDATA_STRING, const bstring *: COMPDATA_STRING))) \
        )

ast_data * ast_data_create_(const void *src, const enum ast_data_types type);
ast_node * ast_node_create(ast_data *data, enum ast_node_types type);

/*======================================================================================*/

extern void new_blank_line(ast_data *data);
extern void new_unimpl_statement(ast_data *data, bstring *id);
extern void new_unimpl_subexpr(ast_data *data, bstring *id, bstring *statement);
extern void new_unknown_statement(ast_data *data, bstring *statement);
extern void new_block(ast_data *data);
extern void new_block_comment(ast_data *data, bstring *text);
extern void new_assignment_statement(ast_data *data, bstring *var, bstring *expr, enum ast_assignment_type type);
extern void new_conditional_statement(ast_data *data, bstring *expr, int type);
extern void new_debug_statement(ast_data *data, bstring *text, bstring *filter);
extern void new_simple_statement(ast_data *data, bstring *keyword, int type);
extern void new_undef_statement(ast_data *data, bstring *var);
extern void new_for_statement(ast_data *data, bstring *var, bstring *ident, int reversed);

extern void append_chance(ast_data *data, bstring *expr);
extern void append_line_comment(ast_data *data, bstring *text, bool prev);

/*======================================================================================*/
__END_DECLS
#endif /* MyAst.h */
