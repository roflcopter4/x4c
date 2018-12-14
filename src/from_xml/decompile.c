#include "Common.h"
#include "decls.h"
#include "my_p99_common.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#define INDENT_WIDTH 4

#define PRAGMA_GCC_PUSH_AND(str)   _Pragma("GCC diagnostic push") _Pragma(str)
#define P01_DECLARE_BSTRING_(NAME) bstring *NAME = NULL
#define DECLARE_BSTRINGS(...)      P99_SEP(P01_DECLARE_BSTRING_, __VA_ARGS__)
#define P01_BSTRING_PARAM_(NAME)   bstring *NAME
#define B_PARAMS(...)              P99_SEQ(P01_BSTRING_PARAM_, __VA_ARGS__)
#define UNKNOWN_ATTR_ERR()                                        \
        errx(1, "Error: Unexpected attribute \"%s : %s\" in %s.", \
             BS(ent->key), BS(ent->val), __func__);

#define SETUP_CHANCE()               \
        bstring *chance      = NULL; \
        bstring *comment     = NULL; \
        int      chance_type = 0
#define HANDLE_CHANCE()                                                               \
        case XS_ATTR_CHANCE:  chance  = ent->val; chance_type = CHANCE_NORMAL; break; \
        case XS_ATTR_WEIGHT:  chance  = ent->val; chance_type = CHANCE_WEIGHT; break; \
        case XS_ATTR_COMMENT: comment = ent->val; break

#define convert_indeces(s)

P99_DEFINE_ENUM(xs_state);

static void decompile_context(xs_data *data);

#ifdef ASSERT
#  undef ASSERT
#endif
#define ASSERT(...)                                                                    \
        do {                                                                           \
                if (!(P99_CHS(0, __VA_ARGS__))) {                                      \
                        P99_IF_EQ_3(P99_NARG(__VA_ARGS__))                             \
                        (warnx(                                                        \
                            "Error (%s - %d): condition failed -> %s -> %s", __func__, \
                            __LINE__, P99_STRINGIFY(P99_CHS(0, __VA_ARGS__)),          \
                            P99_CHS(2, __VA_ARGS__)))                                  \
                        (warnx("Error (%s - %d): condition failed -> %s", __func__,    \
                               __LINE__, P99_STRINGIFY(P99_CHS(0, __VA_ARGS__))));     \
                        P99_IF_GE(P99_NARG(__VA_ARGS__), 2)(b_free(str))();            \
                        return 1;                                                      \
                }                                                                      \
        } while (0)

enum chance_mask {
        CHANCE_NORMAL    = 0x00,
        CHANCE_WEIGHT    = 0x01,
        CHANCE_DEBUG     = 0x02,
        CHANCE_SEMICOLON = 0x04,
        CHANCE_BRACE     = 0x08,
};

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
        XS_ATTR_DEBUGCHANCE,
        XS_ATTR_REVERSE,
        XS_ATTR_COUNTER,
        XS_ATTR_COMMENT,
        XS_ATTR_NEGATE,
        XS_ATTR_MAX,
        XS_ATTR_MIN,
        XS_ATTR_OPERATION,
        XS_ATTR_TEXT,
        XS_ATTR_FILTER,
        XS_ATTR_PROFILE,
        XS_ATTR_WEIGHT,
        XS_ATTR_LIST,
        XS_ATTR_SEED,
};

static struct attribute_identity {
        bstring           attr;
        enum xs_attr_type id;
} attr_ids[] = {
        { BT("name"),        XS_ATTR_NAME        },
        { BT("value"),       XS_ATTR_VALUE       },
        { BT("exact"),       XS_ATTR_EXACT       },
        { BT("chance"),      XS_ATTR_CHANCE      },
        { BT("reverse"),     XS_ATTR_REVERSE     },
        { BT("counter"),     XS_ATTR_COUNTER     },
        { BT("comment"),     XS_ATTR_COMMENT     },
        { BT("negate"),      XS_ATTR_NEGATE      },
        { BT("max"),         XS_ATTR_MAX         },
        { BT("min"),         XS_ATTR_MIN         },
        { BT("operation"),   XS_ATTR_OPERATION   },
        { BT("text"),        XS_ATTR_TEXT        },
        { BT("filter"),      XS_ATTR_FILTER      },
        { BT("debugchance"), XS_ATTR_DEBUGCHANCE },
        { BT("profile"),     XS_ATTR_PROFILE     },
        { BT("weight"),      XS_ATTR_WEIGHT      },
        { BT("list"),        XS_ATTR_LIST        },
        { BT("seed"),        XS_ATTR_SEED        },
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

static int handle_unknown(xs_data *data);
static int handle_blank_line(xs_data *data);
static int handle_comment(xs_data *data);
static int handle_while_statement(xs_data *data, genlist *dict);
static int handle_for_statement(xs_data *data, genlist *dict);
static int handle_conditional(xs_data *data, genlist *dict, const xs_state state);
static int handle_assignment(xs_data *data, genlist *dict);
static int handle_remove_value(xs_data *data, genlist *dict);
static int handle_debug_text(xs_data *data, genlist *dict);
static int handle_keywords(xs_data *data, genlist *dict, xs_context_type type);
static int fallback_handler(xs_data *data, genlist *dict);

static genlist *parse_attributes(xs_context *ctx);
static void     add_with_chance(bstring *strp, bstring *chance, int mask);
static void     add_comment(bstring *str, bstring *comment);

static void
decompile_context(xs_data *data)
{
        xs_context *ctx  = data->cur_ctx;
        genlist *   dict = ((P44_EQ_ANY(ctx->xtype, XS_UNKNOWN_CONTENT, XS_COMMENT, XS_BLANK_LINE))
                             ? NULL
                             : parse_attributes(ctx));
        int         ret  = 0;

        switch (ctx->xtype) {
        case XS_UNKNOWN_CONTENT: ret = handle_unknown(data);                            break;
        case XS_BLANK_LINE:      ret = handle_blank_line(data);                         break;
        case XS_COMMENT:         ret = handle_comment(data);                            break;
        case XS_IF_STMT:         ret = handle_conditional(data, dict, XS_STATE_IF);     break;
        case XS_ELSEIF_STMT:     ret = handle_conditional(data, dict, XS_STATE_ELSEIF); break;
        case XS_ELSE_STMT:       ret = handle_conditional(data, dict, XS_STATE_ELSE);   break;
        case XS_WHILE_STMT:      ret = handle_conditional(data, dict, XS_STATE_WHILE);  break;
        /* case XS_WHILE_STMT:      ret = handle_while_statement(data, dict);              break; */
        case XS_FOR_STMT:        ret = handle_for_statement(data, dict);                break;
        case XS_SET_VALUE:       ret = handle_assignment(data, dict);                   break;
        case XS_REMOVE_VALUE:    ret = handle_remove_value(data, dict);                 break;
        case XS_DEBUG_TEXT:      ret = handle_debug_text(data, dict);                   break;
        case XS_RETURN:          ret = handle_keywords(data, dict, XS_RETURN);          break;
        case XS_BREAK:           ret = handle_keywords(data, dict, XS_BREAK);           break;

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

        bool negate = false;
        SETUP_CHANCE();
        DECLARE_BSTRINGS(val);
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                HANDLE_CHANCE();
                case XS_ATTR_VALUE:   val     = ent->val; break;
                case XS_ATTR_NEGATE:  negate  = b_iseq(ent->val, B("true")); break;
                case XS_ATTR_UNKNOWN:
                default:
                        UNKNOWN_ATTR_ERR();
                }
        }

        ASSERT(val, str);
        if (negate)
                b_sprintfa(str, "while (!(%s)) ", val);
        else
                b_sprintfa(str, "while (%s) ", val);
        add_with_chance(str, chance, CHANCE_BRACE | chance_type);
        de_verbosify(&str);
        convert_indeces(&str);
        add_comment(str, comment);

        b_list_append(data->fmt, str);
        return 0;
}

static void
handle_foreach_statement(bstring *str, bstring *counter, bstring *value, bool reverse)
{
        if (counter) {
                if (reverse)
                        b_sprintfa(str, "for (%s in reversed %s) ", counter, value);
                else
                        b_sprintfa(str, "for (%s in %s) ", counter, value);
        } else {
                if (reverse)
                        b_sprintfa(str, "for (reversed %s) ", value);
                else
                        b_sprintfa(str, "for (%s) ", value);
        }
}

static void
handle_for_range_statement(bstring *str, bstring *counter, bstring *min, bstring *max, bstring *prof)
{
        b_catlit(str, "for (");
        if (counter)
                b_sprintfa(str, "%s in ", counter);
        b_sprintfa(str, "range(%s, %s", min, max);
        if (prof)
                b_sprintfa(str, ", \"%s\"))", prof);
        else
                b_catlit(str, "))");
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

        bool reverse = false;
        SETUP_CHANCE();
        DECLARE_BSTRINGS(value, counter, min, max, prof);
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                HANDLE_CHANCE();
                case XS_ATTR_REVERSE: reverse = b_iseq(ent->val, B("true")); break;
                case XS_ATTR_EXACT:   value   = ent->val; break;
                case XS_ATTR_COUNTER: counter = ent->val; break;
                case XS_ATTR_MIN:     min     = ent->val; break;
                case XS_ATTR_MAX:     max     = ent->val; break;
                case XS_ATTR_PROFILE: prof    = ent->val; break;
                case XS_ATTR_UNKNOWN:
                default:
                        UNKNOWN_ATTR_ERR();
                }
        }

        if (!value && !(min && max)) {
                b_free(str);
                return 1;
        }
        ASSERT(str);

        if (value)
                handle_foreach_statement(str, counter, value, reverse);
        else if (min && max)
                handle_for_range_statement(str, counter, min, max, prof);

        add_with_chance(str, chance, CHANCE_BRACE | chance_type);
        de_verbosify(&str);
        convert_indeces(&str);
        add_comment(str, comment);

        b_list_append(data->fmt, str);
        return 0;
}

/*======================================================================================*/
/* Conditionals */

static void handle_conditional_range_stmt(bstring *str, bstring *maxi, bstring *mini);
static void handle_positive_conditional(B_PARAMS(str, val, exact, mini, maxi, list), bool range);
static void handle_negative_conditional(B_PARAMS(str, val, exact, mini, maxi, list), bool range);

static inline void
do_handle_conditional(B_PARAMS(str, exact, mini, maxi, list), const bool range)
{
        if (exact)
                b_sprintfa(str, " %s)", exact);
        else if (range)
                handle_conditional_range_stmt(str, mini, maxi);
        else if (list)
                b_sprintfa(str, " list%s)", list);
}

static void
handle_positive_conditional(B_PARAMS(str, val, exact, mini, maxi, list), const bool range)
{
        if (exact || range || list)  {
                b_sprintfa(str, "(%s is", val);
                do_handle_conditional(str, exact, mini, maxi, list, range);
        } else {
                b_sprintfa(str, "(%s)", val);
        }
}

static void
handle_negative_conditional(B_PARAMS(str, val, exact, mini, maxi, list), const bool range)
{
        if (exact || range || list)  {
                b_sprintfa(str, "(%s isnot", val);
                do_handle_conditional(str, exact, mini, maxi, list, range);
        } else {
                b_sprintfa(str, "(!(%s))", val);
        }
}

static void
handle_conditional_range_stmt(bstring *str, bstring *maxi, bstring *mini)
{
        bool chain = false;
        b_catlit(str, " {");
        if (mini) {
                b_sprintfa(str, "min: %s", mini);
                chain = true;
        }
        if (maxi) {
                if (chain)
                        b_catlit(str, ", ");
                b_sprintfa(str, "max: %s", maxi);
                /* chain = true; */
        }
        b_catlit(str, "})");
}

static int
handle_conditional(xs_data *data, genlist *dict, const xs_state state)
{
        bool        range = false;
        xs_context *ctx   = data->cur_ctx;
        ctx->state        = state;
        if (state == XS_STATE_WHILE)
                ctx->mask = XS_MASK_LOOP | XS_MASK_BLOCK;
        else
                ctx->mask = XS_MASK_COND | XS_MASK_BLOCK;

        bool negate = false;
        SETUP_CHANCE();
        DECLARE_BSTRINGS(val, exact, mini, maxi, list);
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                HANDLE_CHANCE();
                case XS_ATTR_VALUE:   val     = ent->val; break;
                case XS_ATTR_NEGATE:  negate  = b_iseq(ent->val, B("true")); break;
                case XS_ATTR_EXACT:   exact   = ent->val; break;
                case XS_ATTR_MAX:     mini    = ent->val; range= true; break;
                case XS_ATTR_MIN:     maxi    = ent->val; range= true; break;
                case XS_ATTR_LIST:    list    = ent->val; break;
                case XS_ATTR_UNKNOWN:
                default:
                        UNKNOWN_ATTR_ERR();
                }
        }

        switch (state) {
        case XS_STATE_WHILE: {
                bstring *str = init_line(ctx->depth);
                b_list_append(data->fmt, str);
                b_catlit(PREV_STR, "while ");
                goto condition;
        }
        case XS_STATE_IF: {
                ASSERT(val);
                bstring *str = init_line(ctx->depth);
                b_list_append(data->fmt, str);
                b_catlit(PREV_STR, "if ");
                goto condition;
        }
        case XS_STATE_ELSEIF:
                ASSERT(val);
                if (!(ctx->parent->prev_child_mask & XS_MASK_COND)) {
                        bstring *str = init_line(ctx->depth);
                        b_list_append(data->fmt, str);
                } else {
                        b_catchar(PREV_STR, ' ');
                }
                b_catlit(PREV_STR, "elsif ");

        condition:
                if (negate)
                        handle_negative_conditional(PREV_STR, val, exact, mini, maxi, list, range);
                else
                        handle_positive_conditional(PREV_STR, val, exact, mini, maxi, list, range);
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

        add_with_chance(PREV_STR, chance, CHANCE_BRACE | chance_type);
        de_verbosify(&PREV_STR);
        convert_indeces(&PREV_STR);
        add_comment(PREV_STR, comment);

        return 0;
}

/*======================================================================================*/
/* Variable Assignment */
/*======================================================================================*/

static void
handle_struct_assignment(B_PARAMS(str, mini, maxi, prof, seed))
{
        bool chain = false;
        b_catlit(str, " = {");
        if (mini) {
                b_sprintfa(str, "min: %s", mini);
                chain = true;
        }
        if (maxi) {
                if (chain)
                        b_catlit(str, ", ");
                b_sprintfa(str, "max: %s", maxi);
                chain = true;
        }
        if (prof) {
                if (chain)
                        b_catlit(str, ", ");
                b_sprintfa(str, "profile: %s", prof);
                chain = true;
        }
        if (seed) {
                if (chain)
                        b_catlit(str, ", ");
                b_sprintfa(str, "seed: %s", seed);
                /* chain = true; */
        }
        b_catchar(str, '}');
}

static int
handle_assignment(xs_data *data, genlist *dict)
{
        bool        special = false;
        xs_context *ctx = data->cur_ctx;
        bstring    *str = init_line(ctx->depth);
        ctx->state      = XS_STATE_STATEMENT;
        ctx->mask       = XS_MASK_STATEMENT;

        SETUP_CHANCE();
        DECLARE_BSTRINGS(name, val, min, max, prof, operation, seed);
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                HANDLE_CHANCE();
                case XS_ATTR_NAME:      name      = ent->val; break;
                case XS_ATTR_EXACT:     val       = ent->val; break;
                case XS_ATTR_OPERATION: operation = ent->val; break;
                case XS_ATTR_MAX:       max       = ent->val; special = true; break;
                case XS_ATTR_MIN:       min       = ent->val; special = true; break;
                case XS_ATTR_PROFILE:   prof      = ent->val; special = true; break;
                case XS_ATTR_SEED:      seed      = ent->val; special = true; break;
                case XS_ATTR_LIST:
                        if (ent->val->data[0] == '[')
                                val = b_sprintf("list%s", ent->val);
                        else
                                val = b_sprintf("list(%s)", ent->val);
                        talloc_steal(ent, val);
                        break;
                case XS_ATTR_UNKNOWN:
                default:
                        UNKNOWN_ATTR_ERR();
                }
        }

        ASSERT(name, str);
        if (operation)
                b_sprintfa(str, "%s %s", operation, name);
        else
                b_sprintfa(str, "let %s", name);

        if (special)
                handle_struct_assignment(str, min, max, prof, seed);
        else if (val)
                b_sprintfa(str, " = %s", val);

        add_with_chance(str, chance, CHANCE_SEMICOLON | chance_type);
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

        SETUP_CHANCE();
        DECLARE_BSTRINGS(name);
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                HANDLE_CHANCE();
                case XS_ATTR_NAME:    name    = ent->val; break;
                default:
                case XS_ATTR_UNKNOWN:
                        UNKNOWN_ATTR_ERR();
                }
        }

        ASSERT(name, str);
        b_sprintfa(str, "undef %s", name);

        add_with_chance(str, chance, CHANCE_SEMICOLON | chance_type);
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
        xs_context *ctx  = data->cur_ctx;
        bstring    *str  = init_line(ctx->depth);
        ctx->state       = XS_STATE_STATEMENT;
        ctx->mask        = XS_MASK_STATEMENT;

        SETUP_CHANCE();
        DECLARE_BSTRINGS(text, filter);
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                HANDLE_CHANCE();
                case XS_ATTR_DEBUGCHANCE:
                        chance      = ent->val;
                        chance_type = CHANCE_DEBUG;
                        break;
                case XS_ATTR_TEXT:    text    = ent->val; break;
                case XS_ATTR_FILTER:  filter  = ent->val; break;
                case XS_ATTR_UNKNOWN:
                default:
                        UNKNOWN_ATTR_ERR();
                }
        }

        ASSERT(text, str);
        if (filter)
                b_sprintfa(str, "debug >> %s, %s", filter, text);
        else
                b_sprintfa(str, "debug %s", text);

        add_with_chance(str, chance, CHANCE_SEMICOLON | chance_type);
        de_verbosify(&str);
        convert_indeces(&str);
        add_comment(str, comment);

        b_list_append(data->fmt, str);
        return 0;
}

static int
handle_keywords(xs_data *data, genlist *dict, xs_context_type type)
{
        xs_context *ctx = data->cur_ctx;
        bstring    *str = init_line(ctx->depth);
        ctx->state      = XS_STATE_STATEMENT;
        ctx->mask       = XS_MASK_STATEMENT;
        int ch_mask;

        if (ctx->list->qty > 0) {
                ch_mask    = CHANCE_BRACE;
                ctx->mask |= XS_MASK_BLOCK;
        } else {
                ch_mask = CHANCE_SEMICOLON;
        }

        SETUP_CHANCE();
        DECLARE_BSTRINGS(value);
        GENLIST_FOREACH (dict, struct kv_pair *, ent) {
                switch (ent->attr_type) {
                HANDLE_CHANCE();
                case XS_ATTR_VALUE: value = ent->val; break;
                case XS_ATTR_UNKNOWN:
                default:
                        UNKNOWN_ATTR_ERR();
                }
        }

        switch (type) {
        case XS_RETURN:
                b_catlit(str, "return");
                if (value)
                        b_sprintfa(str, " (%s)", value);
                break;
        case XS_BREAK:
                b_catlit(str, "break");
                break;
        default:
                abort();
        }

        add_with_chance(str, chance, ch_mask | chance_type);
        de_verbosify(&str);
        add_comment(str, comment);
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
/* Blanks and comments */

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
handle_unknown(xs_data *data)
{
        xs_context *ctx = data->cur_ctx;
        bstring    *str = b_sprintf("** %s", ctx->unknown_text);
        b_list_append(data->fmt, str);
        return 0;
}

static int
handle_comment(xs_data *data)
{
        xs_context *ctx = data->cur_ctx;
        bstring    *str = init_line(ctx->depth);
        ctx->state      = XS_STATE_COMMENT;
        ctx->mask       = XS_MASK_COMMENT;

        ASSERT(ctx->comment && ctx->comment->data, str);
        b_sprintfa(str, "/*%s*/", ctx->comment);
#if 0
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
#endif
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
add_with_chance(bstring *strp, bstring *chance, const int mask)
{
        if (chance) {
                bstring *name;
                if (mask & CHANCE_DEBUG)
                        name = B("debugchance");
                else if (mask & CHANCE_WEIGHT)
                        name = B("weight");
                else
                        name = B("chance");

                if (mask & CHANCE_SEMICOLON) {
                        if (mask & CHANCE_BRACE)
                                b_sprintfa(strp, " %s (%s); {", name, chance);
                        else
                                b_sprintfa(strp, " %s (%s);", name, chance);
                } else {
                        if (mask & CHANCE_BRACE)
                                b_sprintfa(strp, " %s (%s) {", name, chance);
                        else
                                b_sprintfa(strp, " %s (%s)", name, chance);
                }
        } else {
                if (mask & CHANCE_SEMICOLON) {
                        if (mask & CHANCE_BRACE)
                                b_catlit(strp, "; {");
                        else
                                b_catlit(strp, ";");
                } else {
                        if (mask & CHANCE_BRACE)
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
