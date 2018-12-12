#ifndef X4C_DECLS_H_
#define X4C_DECLS_H_

#include "Common.h"
#include "util/list.h"

#include "contrib/p99/p99.h"
__BEGIN_DECLS
/*======================================================================================*/

#include <expat.h>
#define c_ucharp const XML_Char *

/*
 * Expat callbacks. I don't really care whether these paramaters are clear to read because
 * they are standard typedefs for the expat library. Check the library header for details.
 */
void my_XML_DefaultHandler(void *, c_ucharp, int);
void my_XML_CommentHandler(void *, c_ucharp);
void my_XML_StartElementHandler(void *, c_ucharp, c_ucharp *);
void my_XML_EndElementHandler(void *, c_ucharp);
void my_XML_AttlistDeclHandler(void *, c_ucharp, c_ucharp, c_ucharp, c_ucharp, int);
void my_XML_EntityDeclHandler(void *, c_ucharp, int, c_ucharp, int, c_ucharp, c_ucharp, c_ucharp, c_ucharp);
void my_XML_StartDoctypeDeclHandler(void *, c_ucharp, c_ucharp, c_ucharp, int);
void my_XML_EndDoctypeDeclHandler(void *);

#undef c_ucharp

/*======================================================================================*/

enum xs_context_type {
        XS_TOP_DUMMY,
        XS_UNKNOWN,
        XS_BLANK_LINE,
        XS_COMMENT,

        XS_IF_STMT,
        XS_ELSE_STMT,
        XS_ELSEIF_STMT,
        XS_WHILE_STMT,
        XS_FOR_STMT,
        XS_DO_ANY,
        
        XS_ATTENTION,
        XS_CONDITIONS,
        XS_INIT,
        XS_INTERRUPTS,
        XS_SINGLE_INTERRUPT,
        XS_ON_ABORT,
        XS_ORDER,
        XS_ACTIONS,
        XS_CREATE_ORDER,

        XS_SET_VALUE,
        XS_REMOVE_VALUE,
        XS_SET_COMMAND,
        XS_SET_COMMAND_ACTION,
        XS_CREATE_LIST,
        XS_APPEND_TO_LIST,
        XS_SHUFFLE_LIST,

        XS_PARAM,
        XS_INPUT_PARAM,
        XS_HANDLER,
        XS_CHECK_ANY,
        XS_CHECK_VALUE,
        XS_EDIT_ORDER_PARAM,
        XS_SUBSTITUTE_TEXT,

        XS_LABEL,
        XS_RESUME,
        XS_ASSERTION,
        XS_RETURN,
        XS_BREAK,
        XS_DEBUG_TEXT,
        XS_RUN_SCRIPT,
        XS_SAVE_RETVAL,

        XS_ADD_RELATION_BOOST,
        XS_MATCH_CONTENT,
        XS_MATCH_DISTANCE,
        XS_SKILL,
        XS_WAIT,
        XS_INTERRUPT_AFTER_TIME,
        XS_ADD_FACTION_RELATION,
        XS_SET_ORDER_SYNCPOINT_REACHED,
        XS_SIGNAL_OBJECTS,
        XS_MOVE_STRAFE,
        XS_POSITION,
        XS_ROTATION,

        XS_EVENT_MASSTRAFFIC_SHIP_REMOVED,
        XS_EVENT_OBJECT_ATTACKED,
        XS_EVENT_OBJECT_CHANGED_ZONE,
        XS_EVENT_OBJECT_DESTROYED,
        XS_EVENT_OBJECT_DOCKED,
        XS_EVENT_OBJECT_SIGNALLED,

        XS_FIND_CLOSEST_UNDISCOVERED_POSITION,
        XS_FIND_ZONE,
        XS_FIND_OBJECT,

        XS_REPLACE,
        XS_SHOW_NOTIFICATION,
        XS_WRITE_TO_LOGBOOK
};

P99_DECLARE_ENUM(xs_state,
        XS_STATE_NONE,
        XS_STATE_IF,
        XS_STATE_ELSEIF,
        XS_STATE_ELSE,
        XS_STATE_WHILE,
        XS_STATE_FOR,
        XS_STATE_COMMENT,
        XS_STATE_STATEMENT
);

typedef enum xs_context_type xs_context_type;
P99_DECLARE_STRUCT(xs_data);
P99_DECLARE_STRUCT(xs_context);

struct xs_data {
        FILE       *out;
        xs_context *top_ctx;
        xs_context *cur_ctx;
        b_list     *fmt;
        uint32_t    mask;
        bool        last_blank;
};

struct xs_context {
        linked_list     *list;
        xs_context      *parent;
        union {
                b_list  *attributes;
                bstring *comment;
        };
        const bstring   *debug_typename;
        bstring         *raw_name;
        xs_context_type  xtype;
        enum xs_state    state;
        uint16_t         depth;
        uint16_t         mask;
        uint16_t         prev_child_mask;
};

xs_context *xs_context_create(xs_context     *parent,
                              xs_context_type xtype,
                              const bstring  *debug_name,
                              bstring        *name,
                              unsigned        depth);
#if 0
void ll_free_data_xs_context(void **ptr);
void xs_context_destroy(xs_context *ctx);
#endif

/*======================================================================================*/
__END_DECLS
#endif /* decls.h */
