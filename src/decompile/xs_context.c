#include "Common.h"
#include "decls.h"


xs_context *
xs_context_create(xs_context     *parent,
                  xs_context_type xtype,
                  const bstring  *debug_name,
                  bstring        *name,
                  const unsigned  depth)
{
        xs_context *ctx = talloc(parent, xs_context);
        ctx->list       = ll_make_new();
        ctx->xtype      = xtype;
        ctx->parent     = parent;
        ctx->attributes = b_list_create();
        ctx->depth      = depth;
        ctx->raw_name   = name;
        ctx->debug_typename = debug_name;
        ctx->state = ctx->mask = ctx->prev_child_mask = 0;
        talloc_steal(ctx, ctx->list);
        talloc_steal(ctx, ctx->attributes);
        talloc_steal(ctx, ctx->raw_name);
        return ctx;
}

/*======================================================================================*/

void
xs_context_dump(xs_context *ctx, unsigned level)
{
        switch (ctx->xtype) {
        case XS_UNKNOWN: case XS_TOP_DUMMY:
                break;
        case XS_COMMENT:
                printf("Comment: \"%s\"\n\n", BS(ctx->comment));
                break;
        default:
                printf("%2u: Node of type \"%s\"\n", level, BS(ctx->debug_typename));
                printf("Attributes: (%u): ", ctx->attributes->qty);
                if (ctx->attributes->qty > 0)
                        B_LIST_FOREACH (ctx->attributes, str)
                                printf("\"%s\", ", BS(str));
                fputs("\n\n", stdout);
                break;
        }

        LL_FOREACH_F (ctx->list, node)
                xs_context_dump(node->data, level + 1);
}
