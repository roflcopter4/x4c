#include "Common.h"
#include "decls.h"

#define BT bt_init

/*======================================================================================*/

static const struct element_id *id_element_type(const bstring *name);

/*======================================================================================*/

static struct element_id {
        const bstring     name;
        const xs_context_type type;
} xs_eids[] = {
    { BT("actions"),                            XS_ACTIONS                            },
    { BT("add_faction_relation"),               XS_ADD_FACTION_RELATION               },
    { BT("add_relation_boost"),                 XS_ADD_RELATION_BOOST                 },
    { BT("append_to_list"),                     XS_APPEND_TO_LIST                     },
    { BT("assert"),                             XS_ASSERTION                          },
    { BT("attention"),                          XS_ATTENTION                          },
    { BT("break"),                              XS_BREAK                              },
    { BT("check_any"),                          XS_CHECK_ANY                          },
    { BT("check_value"),                        XS_CHECK_VALUE                        },
    { BT("conditions"),                         XS_CONDITIONS                         },
    { BT("create_list"),                        XS_CREATE_LIST                        },
    { BT("create_order"),                       XS_CREATE_ORDER                       },
    { BT("debug_text"),                         XS_DEBUG_TEXT                         },
    { BT("do_all"),                             XS_FOR_STMT                           },
    { BT("do_any"),                             XS_DO_ANY                             },
    { BT("do_else"),                            XS_ELSE_STMT                          },
    { BT("do_elseif"),                          XS_ELSEIF_STMT                        },
    { BT("do_if"),                              XS_IF_STMT                            },
    { BT("do_while"),                           XS_WHILE_STMT                         },
    { BT("edit_order_param"),                   XS_EDIT_ORDER_PARAM                   },
    { BT("event_masstraffic_ship_removed"),     XS_EVENT_MASSTRAFFIC_SHIP_REMOVED     },
    { BT("event_object_attacked"),              XS_EVENT_OBJECT_ATTACKED              },
    { BT("event_object_changed_zone"),          XS_EVENT_OBJECT_CHANGED_ZONE          },
    { BT("event_object_destroyed"),             XS_EVENT_OBJECT_DESTROYED             },
    { BT("event_object_docked"),                XS_EVENT_OBJECT_DOCKED                },
    { BT("event_object_signalled"),             XS_EVENT_OBJECT_SIGNALLED             },
    { BT("find_closest_undiscovered_position"), XS_FIND_CLOSEST_UNDISCOVERED_POSITION },
    { BT("find_object"),                        XS_FIND_OBJECT                        },
    { BT("find_zone"),                          XS_FIND_ZONE                          },
    { BT("handler"),                            XS_HANDLER                            },
    { BT("init"),                               XS_INIT                               },
    { BT("input_param"),                        XS_INPUT_PARAM                        },
    { BT("interrupt"),                          XS_SINGLE_INTERRUPT                   },
    { BT("interrupt_after_time"),               XS_INTERRUPT_AFTER_TIME               },
    { BT("interrupts"),                         XS_INTERRUPTS                         },
    { BT("label"),                              XS_LABEL                              },
    { BT("match_content"),                      XS_MATCH_CONTENT                      },
    { BT("match_distance"),                     XS_MATCH_DISTANCE                     },
    { BT("move_strafe"),                        XS_MOVE_STRAFE                        },
    { BT("on_abort"),                           XS_ON_ABORT                           },
    { BT("order"),                              XS_ORDER                              },
    { BT("param"),                              XS_PARAM                              },
    { BT("position"),                           XS_POSITION                           },
    { BT("remove_value"),                       XS_REMOVE_VALUE                       },
    { BT("replace"),                            XS_REPLACE                            },
    { BT("resume"),                             XS_RESUME                             },
    { BT("return"),                             XS_RETURN                             },
    { BT("rotation"),                           XS_ROTATION                           },
    { BT("run_script"),                         XS_RUN_SCRIPT                         },
    { BT("save_retval"),                        XS_SAVE_RETVAL                        },
    { BT("set_command"),                        XS_SET_COMMAND                        },
    { BT("set_command_action"),                 XS_SET_COMMAND_ACTION                 },
    { BT("set_order_syncpoint_reached"),        XS_SET_ORDER_SYNCPOINT_REACHED        },
    { BT("set_value"),                          XS_SET_VALUE                          },
    { BT("show_notification"),                  XS_SHOW_NOTIFICATION                  },
    { BT("shuffle_list"),                       XS_SHUFFLE_LIST                       },
    { BT("signal_objects"),                     XS_SIGNAL_OBJECTS                     },
    { BT("skill"),                              XS_SKILL                              },
    { BT("substitute_text"),                    XS_SUBSTITUTE_TEXT                    },
    { BT("unknown"),                            XS_UNKNOWN                            },
    { BT("wait"),                               XS_WAIT                               },
    { BT("write_to_logbook"),                   XS_WRITE_TO_LOGBOOK                   },
};

/* 
 * Sort the array with b_strcmp_fast before doing anything. This routine is a tiny bit
 * faster than a normal strcmp (it sorts by size first, only doing a memcmp if the sizes
 * of the two strings are identical).
 */
static int eids_cb(const void *a, const void *b)
{
        return b_strcmp_fast(&((struct element_id *)a)->name,
                             &((struct element_id *)b)->name);
}

__attribute__((__constructor__)) static void sort_struct_eids(void)
{
        qsort(xs_eids, ARRSIZ(xs_eids), sizeof(struct element_id), eids_cb);
}

/*======================================================================================*/

void
my_XML_StartElementHandler(void *vdata, const XML_Char *name, const XML_Char **attributes)
{
        xs_data                 *data  = vdata;
        bstring                 *bname = b_fromcstr(name);
        const struct element_id *id    = id_element_type(bname);
        xs_context *ctx = xs_context_create(data->cur_ctx, id->type, &id->name, bname,
                                            data->cur_ctx->depth + 1);

        for (unsigned i = 0; attributes[i]; ++i)
                b_list_append(ctx->attributes, b_fromcstr(attributes[i]));

        ll_append(data->cur_ctx->list, ctx);
        data->cur_ctx    = ctx;
        data->last_blank = false;
}

void
my_XML_EndElementHandler(void *vdata, UNUSED const XML_Char *name)
{
        xs_data *data    = vdata;
        data->cur_ctx    = data->cur_ctx->parent;
        data->last_blank = false;
}

/*======================================================================================*/

static const struct element_id *
id_element_type(const bstring *name)
{
        static const struct element_id unknown = { BT("unknown"), XS_UNKNOWN };

        const struct element_id tmp = { *name, 0 };
        const struct element_id *id = bsearch(&tmp, xs_eids, ARRSIZ(xs_eids),
                                              sizeof(struct element_id), eids_cb);
        return (id) ? id : &unknown;
}
