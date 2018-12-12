#include "Common.h"
#include "decls.h"
#include "my_p99_common.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#define INDENT_WIDTH 4

#define PRAGMA_GCC_PUSH_AND(str)   _Pragma("GCC diagnostic push") _Pragma(str)
#define P01_DECLARE_BSTRING_(NAME) bstring *NAME = NULL
#define DECLARE_BSTRINGS(...)      P99_SEP(P01_DECLARE_BSTRING_, __VA_ARGS__)
#define UNKNOWN_ATTR_ERR()                                        \
        errx(1, "Error: Unexpected attribute \"%s : %s\" in %s.", \
             BS(ent->key), BS(ent->val), __func__);

#define convert_indeces(s)

P99_DEFINE_ENUM(xs_state);

/* static void decompile_context(linked_list *ll, ll_node *node, xs_context *ctx, unsigned level); */
extern bstring * call_pcre2_substitute(const bstring *src, const bstring *pattern, const bstring *replace, uint32_t flags);
static void decompile_context(xs_data *data);

#ifdef ASSERT
#  undef ASSERT
#endif
#define ASSERT(cond) do { if (!(cond)) return 1; } while (0)

enum xs_mask {
        XS_MASK_NONE,
        XS_MASK_COND      = 0x01,
        XS_MASK_LOOP      = 0x02,
        XS_MASK_BLOCK     = 0x04,
        XS_MASK_COMMENT   = 0x08,
        XS_MASK_STATEMENT = 0x10,
};

enum xs_attr_type {
        XS_ATTR_UNKNOWN,
        XS_ATTR_NAME,
        XS_ATTR_VALUE,
        XS_ATTR_EXACT,
        XS_ATTR_CHANCE,
        XS_ATTR_REVERSE,
        XS_ATTR_COUNTER,
        XS_ATTR_COMMENT,
        XS_ATTR_NEGATE,
        XS_ATTR_MAX,
        XS_ATTR_MIN,
        XS_ATTR_OPERATION,
        XS_ATTR_TEXT,
        XS_ATTR_FILTER,
};

static struct attribute_identity {
        bstring           attr;
        enum xs_attr_type id;
} attr_ids[] = {
        { BT("name"),      XS_ATTR_NAME      },
        { BT("value"),     XS_ATTR_VALUE     },
        { BT("exact"),     XS_ATTR_EXACT     },
        { BT("chance"),    XS_ATTR_CHANCE    },
        { BT("reverse"),   XS_ATTR_REVERSE   },
        { BT("counter"),   XS_ATTR_COUNTER   },
        { BT("comment"),   XS_ATTR_COMMENT   },
        { BT("negate"),    XS_ATTR_NEGATE    },
        { BT("max"),       XS_ATTR_MAX       },
        { BT("min"),       XS_ATTR_MIN       },
        { BT("operation"), XS_ATTR_OPERATION },
        { BT("text"),      XS_ATTR_TEXT },
        { BT("filter"),    XS_ATTR_FILTER },
};

struct kv_pair {
        bstring *key;
        bstring *val;
        enum xs_attr_type attr_type;
};

static int attr_ids_cb(const void *a, const void *b)
{
        return b_strcmp_fast(&((struct attribute_identity *)a)->attr,
                             &((struct attribute_identity *)b)->attr);
}
__attribute__((__constructor__)) static void sort_attr_ids(void)
{
        qsort(attr_ids, ARRSIZ(attr_ids), sizeof(struct attribute_identity), attr_ids_cb);
}

/*======================================================================================*/

void
decompile_file(xs_data *data)
{
        data->cur_ctx = data->top_ctx;
        LL_FOREACH_F (data->top_ctx->list, node) {
                data->cur_ctx = node->data;
                decompile_context(data);
        }
}

static inline void
add_indentation(bstring *str, unsigned depth)
{
        depth = (depth) ? depth - 1 : 0;
        for (unsigned i = 0; i < depth * INDENT_WIDTH; ++i)
                b_catchar(str, ' ');
}

static inline bstring *
init_line(unsigned depth)
{
        bstring *str = b_alloc_null(80);
        add_indentation(str, depth);
        return str;
}

/*======================================================================================*/
/* Function Prototypes */
/*======================================================================================*/

static int handle_blank_line(xs_data *data);
static int handle_comment(xs_data *data);
static int handle_while_statement(xs_data *data, genlist *dict);
static int handle_for_statement(xs_data *data, genlist *dict);
static int handle_conditional(xs_data *data, genlist *dict, const xs_state state);
static int handle_assignment(xs_data *data, genlist *dict);
static int handle_remove_value(xs_data *data, genlist *dict);
static int handle_debug_text(xs_data *data, genlist *dict);
static int handle_keywords(xs_data *data, xs_context_type type);
static int fallback_handler(xs_data *data, genlist *dict);

static genlist *parse_attributes(xs_context *ctx);
static void     add_with_chance(bstring *strp, bstring *chance, bool add_brace, bool stmt);
static void     add_comment(bstring *str, bstring *comment);

static void
decompile_context(xs_data *data)
{
        xs_context *ctx  = data->cur_ctx;
        genlist *   dict = ((P44_EQ_ANY(ctx->xtype, XS_COMMENT, XS_BLANK_LINE))
                             ? NULL
                             : parse_attributes(ctx));
        int         ret  = 0;

        switch (ctx->xtype) {
        case XS_BLANK_LINE:   ret = handle_blank_line(data);                         break;
        case XS_COMMENT:      ret = handle_comment(data);                            break;
        case XS_IF_STMT:      ret = handle_conditional(data, dict, XS_STATE_IF);     break;
        case XS_ELSEIF_STMT:  ret = handle_conditional(data, dict, XS_STATE_ELSEIF); break;
        case XS_ELSE_STMT:    ret = handle_conditional(data, dict, XS_STATE_ELSE);   break;
        case XS_WHILE_STMT:   ret = handle_while_statement(data, dict);              break;
        case XS_FOR_STMT:     ret = handle_for_statement(data, dict);                break;
        case XS_SET_VALUE:    ret = handle_assignment(data, dict);                   break;
        case XS_REMOVE_VALUE: ret = handle_remove_value(data, dict);                 break;
        case XS_DEBUG_TEXT:   ret = handle_debug_text(data, dict);                   break;
        case XS_RETURN:       ret = handle_keywords(data, XS_RETURN);                break;
        case XS_BREAK:        ret = handle_keywords(data, XS_BREAK);                 break;

        default: fallback_handler(data, dict); break;
        }

        if (ret)
                fallback_handler(data, dict);
        genlist_destroy(dict);
        ctx->parent->prev_child_mask = ctx->mask;

        LL_FOREACH_F (ctx->list, node) {
                data->cur_ctx = node->data;
                decompile_context(data);
        }
        data->cur_ctx = ctx;

        if (ctx->mask & XS_MASK_BLOCK) {
                bstring *line = init_line(ctx->depth);
                b_catchar(line, '}');
                b_list_append(data->fmt, line);
                ctx->mask &= ~(XS_MASK_BLOCK);
        }
}

/*======================================================================================*/
/* Basic syntax */
/*======================================================================================*/

extern void (de_verbosify)(bstring **orig);
extern void (convert_indeces)(bstring **orig);
#define PREV_STR (data->fmt->lst[data->fmt->qty-1])

static int
handle_while_statement(xs_data *data, genlist *dict)
{
        xs_context *ctx = data->cur_ctx;
        bstring    *str = init_line(ctx->depth);
        ctx->state      = XS_STATE_WHILE;
        ctx->mask       = XS_MASK_LOOP | XS_MASK_BLOCK;

        DECLARE_BSTRINGS(val, comment, chance);
        bool negate = false;
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                case XS_ATTR_VALUE:
                        val = ent->val;
                        break;
                case XS_ATTR_CHANCE:
                        chance = ent->val;
                        break;
                case XS_ATTR_COMMENT:
                        comment = ent->val;
                        break;
                case XS_ATTR_NEGATE:
                        negate = b_iseq(ent->val, B("true"));
                        break;
                case XS_ATTR_UNKNOWN:
                        UNKNOWN_ATTR_ERR();
                default:;
                }
        }

        ASSERT(val);
        if (negate)
                b_sprintfa(str, "while (!(%s)) ", val);
        else
                b_sprintfa(str, "while (%s) ", val);
        add_with_chance(str, chance, true, false);
        de_verbosify(&str);
        convert_indeces(&str);
        add_comment(str, comment);

        b_list_append(data->fmt, str);
        return 0;
}

static int
handle_for_statement(xs_data *data, genlist *dict)
{
        xs_context *ctx = data->cur_ctx;
        if (ctx->parent->xtype == XS_DO_ANY)
                return 1;

        bstring    *str = init_line(ctx->depth);
        ctx->state      = XS_STATE_FOR;
        ctx->mask       = XS_MASK_LOOP | XS_MASK_BLOCK;

        DECLARE_BSTRINGS(value, counter, chance, comment);
        bool reverse = false;
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                case XS_ATTR_EXACT:
                        value = ent->val;
                        break;
                case XS_ATTR_COUNTER:
                        counter = ent->val;
                        break;
                case XS_ATTR_CHANCE:
                        chance = ent->val;
                        break;
                case XS_ATTR_REVERSE:
                        reverse = b_iseq(ent->val, B("true"));
                        break;
                case XS_ATTR_COMMENT:
                        comment = ent->val;
                        break;
                case XS_ATTR_UNKNOWN:
                        UNKNOWN_ATTR_ERR();
                default:;
                }
        }

        ASSERT(value && counter);
        if (reverse)
                b_sprintfa(str, "for (%s in reversed %s) ", counter, value);
        else
                b_sprintfa(str, "for (%s in %s) ", counter, value);

        add_with_chance(str, chance, true, false);
        de_verbosify(&str);
        convert_indeces(&str);
        add_comment(str, comment);

        b_list_append(data->fmt, str);
        return 0;
}

static int
handle_conditional(xs_data *data, genlist *dict, const xs_state state)
{
        xs_context *ctx = data->cur_ctx;
        ctx->state      = state;
        ctx->mask       = XS_MASK_COND | XS_MASK_BLOCK;

        DECLARE_BSTRINGS(val, chance, comment);
        bool negate = false;
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                case XS_ATTR_VALUE:
                        val = ent->val;
                        break;
                case XS_ATTR_CHANCE:
                        chance = ent->val;
                        break;
                case XS_ATTR_COMMENT:
                        comment = ent->val;
                        break;
                case XS_ATTR_NEGATE:
                        negate = b_iseq(ent->val, B("true"));
                        break;
                case XS_ATTR_UNKNOWN:
                        UNKNOWN_ATTR_ERR();
                default:;
                }
        }

        switch (state) {
        case XS_STATE_IF: {
                ASSERT(val);
                bstring *str = init_line(ctx->depth);
                if (negate)
                        b_sprintfa(str, "if (!(%s))", val);
                else
                        b_sprintfa(str, "if (%s)", val);
                b_list_append(data->fmt, str);
                break;
        }
        case XS_STATE_ELSEIF:
                ASSERT(val);
                if (!(ctx->parent->prev_child_mask & XS_MASK_COND)) {
                        bstring *str = init_line(ctx->depth);
                        b_list_append(data->fmt, str);
                } else {
                        b_catchar(PREV_STR, ' ');
                }

                if (negate)
                        b_sprintfa(PREV_STR, "elsif (!(%s))", val);
                else
                        b_sprintfa(PREV_STR, "elsif (%s)", val);
                break;
        case XS_STATE_ELSE:
                if (!(ctx->parent->prev_child_mask & XS_MASK_COND)) {
                        bstring *str = init_line(ctx->depth);
                        b_catlit(str, "else ");
                        b_list_append(data->fmt, str);
                } else {
                        b_catlit(PREV_STR, " else");
                }
                break;
        default:;
        }

        add_with_chance(PREV_STR, chance, true, false);
        de_verbosify(&PREV_STR);
        convert_indeces(&PREV_STR);
        add_comment(PREV_STR, comment);

        return 0;
}

static int
handle_blank_line(xs_data *data)
{
        xs_context *ctx = data->cur_ctx;
        ctx->state      = 0;
        ctx->mask       = 0;
        b_list_append(data->fmt, b_alloc_null(0));
        return 0;
}

static int
handle_comment(xs_data *data)
{
        xs_context *ctx = data->cur_ctx;
        bstring    *str = init_line(ctx->depth);
        ctx->state      = XS_STATE_COMMENT;
        ctx->mask       = XS_MASK_COMMENT;

        ASSERT(ctx->comment && ctx->comment->data);
        b_catlit(str, "/*");

        if (b_strchr(ctx->comment, '\n') > 0) {
                unsigned len = ((ctx->depth) ? (ctx->depth - 1) : 0) * INDENT_WIDTH;

                b_catchar(str, '\n');
                add_indentation(str, ctx->depth);
                b_catlit(str, "  ");

                for (unsigned i = 0; i < ctx->comment->slen; ++i) {
                        if (ctx->comment->data[i] == '\n') {
                                b_catchar(str, ctx->comment->data[i++]);
                                unsigned x;
                                for (x = 0; x < len && i < ctx->comment->slen; ++x, ++i) {
                                        if (!isblank(ctx->comment->data[i]))
                                                break;
                                        b_catchar(str, ' ');
                                }
                                for (; x < len; ++x)
                                        b_catchar(str, ' ');
                                b_catlit(str, "  ");
                        }
                        b_catchar(str, ctx->comment->data[i]);
                }
                b_catchar(str, '\n');
                add_indentation(str, ctx->depth);
        } else {
                b_concat(str, ctx->comment);
                /* b_catchar(str, ' '); */
        }

        b_catlit(str, "*/");
        b_list_append(data->fmt, str);

        return 0;
}

/*======================================================================================*/
/* Variable Assignment */
/*======================================================================================*/

static int
handle_assignment(xs_data *data, genlist *dict)
{
        xs_context *ctx = data->cur_ctx;
        bstring    *str = init_line(ctx->depth);
        ctx->state      = XS_STATE_STATEMENT;
        ctx->mask       = XS_MASK_STATEMENT;

        DECLARE_BSTRINGS(name, val, chance, comment, min, max, operation);
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                case XS_ATTR_NAME:
                        name = ent->val;
                        break;
                case XS_ATTR_EXACT:
                        val = ent->val;
                        break;
                case XS_ATTR_CHANCE:
                        chance = ent->val;
                        break;
                case XS_ATTR_COMMENT:
                        comment = ent->val;
                        break;
                case XS_ATTR_MAX:
                        max = ent->val;
                        break;
                case XS_ATTR_MIN:
                        min = ent->val;
                        break;
                case XS_ATTR_OPERATION:
                        operation = ent->val;
                        break;
                case XS_ATTR_UNKNOWN:
                        UNKNOWN_ATTR_ERR();
                default:;
                }
        }

        ASSERT(name);
        if (operation) {
                if (val)
                        b_sprintfa(str, "%s %s = %s", operation, name, val);
                else if (min && max)
                        b_sprintfa(str, "%s %s => {min: %s, max: %s}", operation, name, min, max);
                else
                        b_sprintfa(str, "%s %s", operation, name);
        } else {
                if (val)
                        b_sprintfa(str, "let %s = %s", name, val);
                else if (min && max)
                        b_sprintfa(str, "let %s => {min: %s, max: %s}", name, min, max);
                else
                        b_sprintfa(str, "let %s", name);
        }

        add_with_chance(str, chance, false, true);
        de_verbosify(&str);
        convert_indeces(&str);
        add_comment(str, comment);

        b_list_append(data->fmt, str);
        return 0;
}

static int
handle_remove_value(xs_data *data, genlist *dict)
{
        xs_context *ctx = data->cur_ctx;
        bstring    *str = init_line(ctx->depth);
        ctx->state      = XS_STATE_STATEMENT;
        ctx->mask       = XS_MASK_STATEMENT;

        DECLARE_BSTRINGS(name, comment, chance);
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                case XS_ATTR_NAME:
                        name = ent->val;
                        break;
                case XS_ATTR_COMMENT:
                        comment = ent->val;
                        break;
                case XS_ATTR_CHANCE:
                        chance = ent->val;
                        break;
                case XS_ATTR_UNKNOWN:
                        UNKNOWN_ATTR_ERR();
                default:;
                }
        }

        ASSERT(name);
        b_sprintfa(str, "undef %s", name);

        add_with_chance(str, chance, false, true);
        de_verbosify(&str);
        convert_indeces(&str);
        add_comment(str, comment);

        b_list_append(data->fmt, str);
        return 0;
}

/*======================================================================================*/
/* Misc */
/*======================================================================================*/


static int
handle_debug_text(xs_data *data, genlist *dict)
{
        xs_context *ctx = data->cur_ctx;
        bstring    *str = init_line(ctx->depth);
        ctx->state      = XS_STATE_STATEMENT;
        ctx->mask       = XS_MASK_STATEMENT;

        DECLARE_BSTRINGS(text, chance, filter, comment);
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                case XS_ATTR_TEXT:
                        text = ent->val;
                        break;
                case XS_ATTR_FILTER:
                        filter = ent->val;
                        break;
                case XS_ATTR_CHANCE:
                        chance = ent->val;
                        break;
                case XS_ATTR_COMMENT:
                        comment = ent->val;
                        break;
                case XS_ATTR_UNKNOWN:
                        UNKNOWN_ATTR_ERR();
                default:;
                }
        }

        ASSERT(text);
        if (filter)
                b_sprintfa(str, "debug >> %s, %s", filter, text);
        else
                b_sprintfa(str, "debug %s", text);
        /* else */
                /* b_catchar(str, ')'); */
        /* if (chance) */
                /* b_sprintfa(str, " with chance (%s)", chance); */

        add_with_chance(str, chance, false, true);
        de_verbosify(&str);
        convert_indeces(&str);
        add_comment(str, comment);

        b_list_append(data->fmt, str);
        return 0;
}

static int
handle_keywords(xs_data *data, xs_context_type type)
{
        xs_context *ctx = data->cur_ctx;
        bstring    *str = init_line(ctx->depth);
        ctx->state      = XS_STATE_STATEMENT;
        ctx->mask       = XS_MASK_STATEMENT;

        switch (type) {
        case XS_RETURN:
                b_catlit(str, "return");
                break;
        case XS_BREAK:
                b_catlit(str, "break");
                break;
        default:
                abort();
        }

        b_catchar(str, ';');
        b_list_append(data->fmt, str);
        return 0;
}

static int
fallback_handler(xs_data *data, genlist *dict)
{
        xs_context *ctx = data->cur_ctx;
        bstring    *str = init_line(ctx->depth);
        ctx->state      = XS_STATE_NONE;
        ctx->mask       = 0;

        b_sprintfa(str, "%s<", ctx->raw_name);
        GENLIST_FOREACH (dict, struct kv_pair *, ent)
                b_sprintfa(str, "%s=\"%s\", ", ent->key, ent->val);
        if (dict && dict->qty > 0)
                str->data[str->slen -= 2] = '\0';

        if (ctx->list->qty > 0) {
                ctx->mask |= XS_MASK_BLOCK;
                b_catlit(str, ">; {");
        } else {
                b_catlit(str, ">;");
        }

        b_list_append(data->fmt, str);
        return 0;
}

/*======================================================================================*/
/* Util */
/*======================================================================================*/

static genlist *
parse_attributes(xs_context *ctx)
{
        if (ctx->attributes->qty == 0)
                return NULL;
        genlist *dict = genlist_create(NULL);

        for (unsigned i = 0; i < ctx->attributes->qty; i += 2) {
                struct kv_pair *ent = talloc(dict, struct kv_pair);
                ent->key            = ctx->attributes->lst[i];
                ent->val            = ((i+1) < ctx->attributes->qty)
                                      ? ctx->attributes->lst[i+1] : NULL;

                struct attribute_identity tmp = { *ent->key, 0 };
                struct attribute_identity *id = bsearch(
                    &tmp, attr_ids, ARRSIZ(attr_ids),
                    sizeof(struct attribute_identity), attr_ids_cb);

                ent->attr_type = (id) ? id->id : XS_ATTR_UNKNOWN;
                genlist_append(dict, ent);
        }

        return dict;
}

static void
add_with_chance(bstring *strp, bstring *chance, const bool add_brace, const bool stmt)
{
        if (chance) {
                if (stmt) {
                        if (add_brace)
                                b_sprintfa(strp, " chance (%s); {", chance);
                        else
                                b_sprintfa(strp, " chance (%s);", chance);
                } else {
                        if (add_brace)
                                b_sprintfa(strp, " chance (%s) {", chance);
                        else
                                b_sprintfa(strp, " chance (%s)", chance);
                }
        } else {
                if (stmt) {
                        if (add_brace)
                                b_catlit(strp, "; {");
                        else
                                b_catlit(strp, ";");
                } else {
                        if (add_brace)
                                b_catlit(strp, " {");
                }

        }
}


static void
add_comment(bstring *str, bstring *comment)
{
        if (comment)
                b_sprintfa(str, " // %s", comment);
}
