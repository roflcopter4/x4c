#include "Common.h"
#include "decls.h"
#include <getopt.h>

#define PRINTL(str) fwrite(("" str ""), 1, sizeof(str) - 1, stdout)

/* P99_DEFINE_ENUM(xs_context_type); */

/*======================================================================================*/

extern void xs_context_dump(xs_context *ctx, unsigned level);
#define xs_context_dump(...) P99_CALL_DEFARG(xs_context_dump, 2, __VA_ARGS__)
#define xs_context_dump_defarg_1() 0

extern void decompile_file(xs_data *data);

/*======================================================================================*/

void
decompile_main(const char *fname, const char *out_fname)
{
        setlocale(LC_ALL, "en_US.UTF-8");

        XML_Parser ps   = XML_ParserCreate("utf-8");
        bstring   *file = (fname) ? b_quickread("%s", fname) : b_read_stdin();
        if (!file)
                errx(1, "read error");

        xs_data *data    = talloc_named_const(NULL, sizeof(xs_data), "Main user data structure");
        data->fmt        = b_list_create();
        data->top_ctx    = xs_context_create(NULL, XS_TOP_DUMMY, b_fromlit("top"), b_fromlit("tmp"), 0);
        data->cur_ctx    = data->top_ctx->parent = data->top_ctx;
        data->mask       = 0;
        data->last_blank = false;
        data->out        = (!out_fname || strcmp(out_fname, "-") == 0)
                              ? stdout
                              : safe_fopen(out_fname, "wb");
        talloc_steal(data, data->fmt);
        talloc_steal(data, data->top_ctx);
        talloc_steal(data->top_ctx, data->top_ctx->debug_typename);

        XML_SetUserData(ps, data);
        XML_SetDefaultHandler(ps, my_XML_DefaultHandler);
        XML_SetCommentHandler(ps, my_XML_CommentHandler);
        XML_SetElementHandler(ps, my_XML_StartElementHandler, my_XML_EndElementHandler);
        XML_SetEntityDeclHandler(ps, my_XML_EntityDeclHandler);
        XML_SetAttlistDeclHandler(ps, my_XML_AttlistDeclHandler);
        XML_SetDoctypeDeclHandler(ps, my_XML_StartDoctypeDeclHandler, my_XML_EndDoctypeDeclHandler);

        if (XML_Parse(ps, BS(file), (int)file->slen, true) != XML_STATUS_OK)
                errx(1, "XML Error.");

        decompile_file(data);
        B_LIST_FOREACH (data->fmt, str, i)
                b_printf("%s\n", str);

        b_destroy(file);
        XML_ParserFree(ps);
        if (data->out != stdout)
                fclose(data->out);
        talloc_free(data);
}

/*======================================================================================*/

void
my_XML_DefaultHandler(void *vdata, const XML_Char *s, int len)
{
        static const bstring default_debug_name = bt_init("default");
        xs_data *data = vdata;

        if ((len == 1 && s[0] == '\n') || (len == 2 && s[0] == '\r' && s[1] == '\n')) {
                if (!data->last_blank) {
                        data->last_blank = true;
                        return;
                }
                xs_context *ctx  = xs_context_create(data->cur_ctx, XS_BLANK_LINE,
                                                     &default_debug_name, NULL,
                                                     data->cur_ctx->depth + 1);
                ll_append(ctx->parent->list, ctx);
        } else {
                bool graph = false;
                for (int i = 0; i < len; ++i) {
                        if (!isprint(s[i])) {
                                /* fprintf(stderr, "Got printable chars in default handler: \""); */
                                /* fwrite(s+i, 1, len-i, stderr); */
                                /* fputs("\"\n", stderr); */
                                return;
                        }
                        if (isgraph(s[i]))
                                graph = true;
                }
                if (!graph)
                        return;
                xs_context *ctx = xs_context_create(data->cur_ctx, XS_UNKNOWN_CONTENT,
                                                    &default_debug_name, NULL,
                                                    data->cur_ctx->depth + 1);
                ctx->unknown_text = b_fromblk(s, len);
                talloc_steal(ctx, ctx->unknown_text);
                ll_append(ctx->parent->list, ctx);
        }
}

void
my_XML_CommentHandler(void *vdata, const XML_Char *s)
{
        static const bstring comment_debug_name = bt_init("comment");
        xs_data    *data = vdata;
        xs_context *ctx  = xs_context_create(data->cur_ctx, XS_COMMENT,
                                             &comment_debug_name, b_fromlit("comment"),
                                             data->cur_ctx->depth + 1);
        data->last_blank = false;
        ctx->comment     = b_fromcstr(s);
        talloc_steal(ctx, ctx->comment);
        ll_append(ctx->parent->list, ctx);
}

void
my_XML_EntityDeclHandler(UNUSED void    *data,
                         const XML_Char *entityName,
                         int             is_parameter_entity,
                         const XML_Char *value,
                         int             value_length,
                         const XML_Char *base,
                         const XML_Char *systemId,
                         const XML_Char *publicId,
                         const XML_Char *notationName)
{
        printf("Got an entitydecl named \"%s\" ... which is %sa paramater\n",
               entityName, (is_parameter_entity ? "" : "not "));

        if (value) {
                fwrite(value, 1, value_length, stdout);
                putc('\n', stdout);
        } else {
                printf("No value: %s - %s - %s - %s\n", base, systemId, publicId, notationName);
        }
}

void
my_XML_AttlistDeclHandler(UNUSED void    *data,
                          const XML_Char *elname,
                          const XML_Char *attname,
                          const XML_Char *att_type,
                          const XML_Char *dflt,
                          int             isrequired)
{
        printf("Got an AttlistDecl. elname: \"%s\", attname: \"%s\", att_type: "
               "\"%s\", dflt: \"%s\", isrequired: %d\n",
               elname, attname, att_type, dflt, isrequired);
}


void
my_XML_StartDoctypeDeclHandler(UNUSED void    *data,
                               const XML_Char *doctypeName,
                               const XML_Char *sysid,
                               const XML_Char *pubid,
                               int             has_internal_subset)
{
        printf("Got DoctypeDecl, name \"%s\", sysid \"%s\", pubid \"%s\", "
               "with%s internal subset.\n",
               doctypeName, sysid, pubid, (has_internal_subset ? "" : "out"));
}

void
my_XML_EndDoctypeDeclHandler(UNUSED void *data)
{
        PRINTL("Got end of doctypeDecl.\n");
}

