#include "Common.h"

#include "ast.h"
P99_DEFINE_ENUM(ast_node_types);

/*======================================================================================*/

static int fp_wrap_free(struct fp_wrap_s *wrap) {
        if (wrap->fp != stdin)
                fclose(wrap->fp);
        return 0;
}

ast_data *
ast_data_create_(const void *src, const enum ast_data_types type)
{
        ast_data *data = talloc_zero(NULL, ast_data);
        data->fp_wrap  = talloc(data, struct fp_wrap_s);
        talloc_set_destructor(data->fp_wrap, fp_wrap_free);

        switch (type) {
        case COMPDATA_FILE:
                data->fp_wrap->fp = (FILE *)src;
                data->filename    = b_fromlit("<STDIN>");
                break;
        case COMPDATA_FILENAME:
                data->fp_wrap->fp = fopen((const char *)src, "rb");
                data->filename    = b_fromcstr(src);
                break;
        default:
                abort();
        }
        talloc_steal(data, data->filename);

        if (!data->fp_wrap->fp) {
                warn("Error: Null file");
                talloc_free(data);
                return NULL;
        }

        data->cur             = ast_node_create(data, NODE_BLOCK);
        data->cur->block.list = genlist_create(data->cur);
        data->cur->depth      = 0;
        data->top             = data->cur;
        talloc_steal(data, data->cur);
        return data;
}

ast_node *
ast_node_create(ast_data *data, enum ast_node_types type)
{
        ast_node *node = talloc_zero(data->cur, ast_node);
        if (data->cur) {
                node->parent = data->cur;
                node->depth  = data->cur->depth + 1;
                genlist_append(data->cur->block.list, node);
        } else {
                node->parent = node;
        }
        node->type = type;
        data->cur  = node;
        return node;
}

/*======================================================================================*/

void
new_unimpl_statement(ast_data *data, bstring *id)
{
        ast_node *node    = ast_node_create(data, NODE_ST_UNIMPL);
        node->unimpl.id   = id;
        node->unimpl.list = genlist_create(node);
        talloc_steal(node, node->unimpl.id);
}

void
new_unimpl_subexpr(ast_data *data, bstring *id, bstring *statement)
{
        ast_atom *atom    = talloc(data->cur->unimpl.list, ast_atom);
        atom->type        = AT_UNIMPL_ARG;
        atom->unimpl.id   = id;
        atom->unimpl.text = statement;
        talloc_steal(atom, id);
        talloc_steal(atom, statement);
        genlist_append(data->cur->unimpl.list, atom);
}

void
new_unknown_statement(ast_data *data, bstring *statement)
{
        ast_node *node = ast_node_create(data, NODE_UNKNOWN);
        node->string   = statement;
        talloc_steal(node, node->string);
}

/*======================================================================================*/

void
new_assignment_statement(ast_data *data, bstring *var, bstring *expr, enum ast_assignment_type type)
{
        ast_node *node           = ast_node_create(data, NODE_ST_ASSIGN);
        node->assignment.var     = var;
        node->assignment.type    = type;
        talloc_steal(node, node->assignment.var);
        if (expr) {
                node->assignment.expr = expr;
                talloc_steal(node, node->assignment.expr);
        }
}

void
new_conditional_statement(ast_data *data, bstring *expr, int type)
{
        ast_node *node  = ast_node_create(data, 0);
        node->type      = type;
        node->condition = expr;
        talloc_steal(node, node->condition);
}

void
new_for_statement(ast_data *data, bstring *var, bstring *ident, int reversed)
{
        ast_node *node         = ast_node_create(data, NODE_ST_FOR);
        node->forstmt.var      = var;
        node->forstmt.ident    = ident;
        node->forstmt.reversed = reversed;
        talloc_steal(node, node->forstmt.var);
        talloc_steal(node, node->forstmt.ident);
}

/*======================================================================================*/

void
new_simple_statement(ast_data *data, bstring *keyword, int type)
{
        ast_node *node = ast_node_create(data, 0);
        node->type     = type;
        node->keyword  = keyword;
        talloc_steal(node, node->keyword);
}

void
new_undef_statement(ast_data *data, bstring *var)
{
        ast_node *node = ast_node_create(data, NODE_ST_UNDEF);
        node->string   = var;
        talloc_steal(node, node->string);
}

/*======================================================================================*/

void
new_block(ast_data *data)
{
        /* eprintf("The parent node is presently of type %s\n", ast_node_types_getname(data->cur->type)); */
        ast_node *node     = ast_node_create(data, NODE_BLOCK);
        node->block.list   = genlist_create(node);
        unsigned  pnum     = node->parent->block.list->qty - 2;
        ast_node *prev     = node->parent->block.list->lst[pnum];

retry:
        switch (prev->type) {
        case NODE_BLANK_LINE: prev = node->parent->block.list->lst[--pnum]; goto retry;
        case NODE_ST_UNIMPL: node->block.name = prev->unimpl.id;        break;
        case NODE_ST_IF:     node->block.name = b_fromlit("do_if");     break;
        case NODE_ST_ELSIF:  node->block.name = b_fromlit("do_elseif"); break;
        case NODE_ST_ELSE:   node->block.name = b_fromlit("do_else");   break;
        case NODE_ST_WHILE:  node->block.name = b_fromlit("do_while");  break;
        case NODE_ST_FOR:    node->block.name = b_fromlit("do_all");    break;
        default:
                warnx("Invalid block node: %s", ast_node_types_getname(prev->type));
                abort();
        }

        prev->block_parent = true;
        talloc_steal(node, node->block.name);
}

void
new_block_comment(ast_data *data, bstring *text)
{
        ast_node *node = ast_node_create(data, NODE_COMMENT);
        b_strip_leading_ws(text);
        b_strip_trailing_ws(text);
        node->comment  = text;
        talloc_steal(node, node->comment);
}

void
new_blank_line(ast_data *data)
{
        ast_node_create(data, NODE_BLANK_LINE);
}

/*======================================================================================*/

void
new_debug_statement(ast_data *data, bstring *text, bstring *filter)
{
        ast_node *node   = ast_node_create(data, NODE_ST_DEBUG_TEXT);
        node->debug.text = text;
        talloc_steal(node, node->debug.text);
        if (filter) {
                node->debug.filter = filter;
                talloc_steal(node, node->debug.filter);
        }
}

/*======================================================================================*/

void
append_chance(ast_data *data, bstring *expr)
{
        data->cur->chance = expr;
        talloc_steal(data->cur, data->cur->chance);
}

void
append_line_comment(ast_data *data, bstring *text, const bool prev)
{
        b_strip_trailing_ws(text);
        b_strip_leading_ws(text);
        ast_node *node = (prev)
                             ? data->cur->block.list->lst[data->cur->block.list->qty - 1]
                             : data->cur;
        node->line_comment = text;
        talloc_steal(node, node->line_comment);
}
