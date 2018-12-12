#ifndef SRC_LIST_H_
#define SRC_LIST_H_

#include "contrib/bstring/bstring/bstring.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#if !defined __GNUC__ && !defined __attribute__
#  define __attribute__(...)
#endif
#ifndef __aWUR
#  define __aWUR __attribute__((__warn_unused_result__))
#endif
#ifndef __aFMT
#  ifdef __clang__
#    define __aFMT(A1, A2) __attribute__((__format__(__printf__, A1, A2)))
#  else
#    define __aFMT(A1, A2) __attribute__((__format__(__gnu_printf__, A1, A2)))
#  endif
#endif
#if defined __MINGW__ || defined __MINGW32 || defined __MINGW64__
#  define LLDECL extern
#  define INTERN extern /* That's defininitely not confusing at all... */
#else
#  define LLDECL __attribute__((__visibility__("default"))) extern
#  define INTERN __attribute__((__visibility__("hidden"))) extern
#endif

__BEGIN_DECLS
/*======================================================================================================*/

typedef struct linked_list_s linked_list;
typedef struct ll_node_s     ll_node;

struct linked_list_s {
        ll_node *head;
        ll_node *tail;
        int qty;
        pthread_mutex_t lock;
};

struct ll_node_s {
        void *data;
        ll_node *prev;
        ll_node *next;
};

#define ll_insert_before_at(LIST, IND, DATA) \
        ll_insert_before((LIST), ll_at((LIST), (IND)), (DATA))

#define ll_insert_after_at(LIST, IND, DATA) \
        ll_insert_after((LIST), ll_at((LIST), (IND)), (DATA))

#if 0
#define ll_insert_blist_before_at(LIST, IND, DATA, S_IND, E_IND) \
        ll_insert_blist_before((LIST), ll_at((LIST), (IND)), (DATA), (S_IND), (E_IND))

#define ll_insert_blist_after_at(LIST, IND, DATA, S_IND, E_IND) \
        ll_insert_blist_after((LIST), ll_at((LIST), (IND)), (DATA), (S_IND), (E_IND))
#endif

#define ll_delete_at(LIST, IND) \
        ll_delete_node((LIST), ll_at((LIST), (IND)))

#define ll_delete_range_at(LIST, IND, RANGE) \
        ll_delete_range((LIST), ll_at((LIST), (IND)), (RANGE))

#define LL_FOREACH_F(LIST, VAR) \
        for (ll_node * VAR = (LIST)->head; (VAR) != NULL; (VAR) = (VAR)->next)

#define LL_FOREACH_B(LIST, VAR) \
        for (ll_node * VAR = (LIST)->tail; (VAR) != NULL; (VAR) = (VAR)->prev)

LLDECL linked_list *ll_make_new    (void) __aWUR;
LLDECL ll_node     *ll_at          (linked_list *list, int index);
LLDECL void ll_prepend             (linked_list *list, void *data);
LLDECL void ll_append              (linked_list *list, void *data);
LLDECL void ll_delete_node         (linked_list *list, ll_node *node);
LLDECL void ll_delete_range        (linked_list *list, ll_node *at, int range);
LLDECL void ll_destroy             (linked_list *list);
LLDECL void ll_insert_after        (linked_list *list, ll_node *at, void *data);
LLDECL void ll_insert_before       (linked_list *list, ll_node *at, void *data);
/* LLDECL void ll_insert_blist_after  (linked_list *list, ll_node *at, b_list *blist, int start, int end); */
/* LLDECL void ll_insert_blist_before (linked_list *list, ll_node *at, b_list *blist, int start, int end); */
LLDECL bool ll_verify_size         (linked_list *list);
LLDECL bstring *ll_join            (linked_list *list, int sepchar) __aWUR;

/*======================================================================================================*/
/* Generic list */

typedef struct generic_list {
        void **  lst;
        unsigned qty;
        unsigned mlen;
        /* pthread_rwlock_t lock; */
        pthread_mutex_t mut;
} genlist;

typedef int (*genlist_copy_func)(void **dest, void *item);

LLDECL genlist *genlist_create       (void *talloc_ctx) __aWUR;
LLDECL genlist *genlist_create_alloc (const unsigned msz);
LLDECL int      genlist_destroy      (genlist *list);
LLDECL int      genlist_alloc        (genlist *list, const unsigned msz);
LLDECL int      genlist_append       (genlist *list, void *item);
LLDECL int      genlist_remove_index (genlist *list, const unsigned index);
LLDECL int      genlist_remove       (genlist *list, const void *obj);
LLDECL void    *genlist_pop          (genlist *list);
LLDECL void    *genlist_dequeue      (genlist *list);
LLDECL genlist *genlist_copy         (genlist *list, const genlist_copy_func cpy) __aWUR;

/* genlist *genlist_copy            (const genlist *list); */

/* #define genlist_append(...) P99_CALL_DEFARG(genlist_append, 4, __VA_ARGS__)
#define genlist_append_defarg_2() (false)
#define genlist_append_defarg_3() (0llu) */

/* #define genlist_append(lst, item, cp) */
#if 0
#define genlist_append(...)                                          \
        __extension__({                                              \
                auto_type m_var_01_ = P99_CHS(1, __VA_ARGS__);       \
                (genlist_append)(P99_CHS(0, __VA_ARGS__), m_var_01_, \
                                 P99_IF_EQ_3(P99_NARG(__VA_ARGS__))  \
                                     (P99_CHS(2, __VA_ARGS__))       \
                                     (false),                        \
                                 sizeof(*m_var_01_));                \
        })
#endif
/* #define genlist_append(...) P99_CALL_DEFARG(genlist_append, 3, __VA_ARGS__) */

#define GENLIST_FOREACH(LIST, TYPE, VAR, ...)                                            \
        GENLIST_FOREACH_EXPLICIT_(LIST, TYPE, VAR,                                       \
                                  P99_IF_EMPTY(__VA_ARGS__)(P99_UNIQ(cnt))(__VA_ARGS__), \
                                  P99_UNIQ(genlist_b))

#define GENLIST_FOREACH_EXPLICIT_(LIST, TYPE, VAR, CTR, BL)                      \
        for (unsigned CTR, BL = true; (BL) && (LIST) != NULL; (BL) = false)      \
                for (TYPE VAR = ((LIST)->lst[((CTR) = 0)]);                      \
                     (CTR) < (LIST)->qty && (((VAR) = (LIST)->lst[(CTR)]) || 1); \
                     ++(CTR))

/*======================================================================================================*/
/* Simple char * list. */

typedef struct argument_vector {
        char   **lst;
        unsigned qty;
        unsigned mlen;
} str_vector;

LLDECL str_vector *argv_create(const unsigned len) __aWUR;
LLDECL void argv_append       (str_vector *argv, const char *str, const bool cpy);
LLDECL void argv_destroy      (str_vector *argv);
LLDECL void argv_fmt          (str_vector *argv, const char *const __restrict fmt, ...) __aFMT(2, 3);

INTERN void argv_dump__(FILE *fp, const struct argument_vector *argv, const char *listname, const char *, int);
INTERN void argv_dump_fd__(int fd, const struct argument_vector *argv, const char *listname, const char *, int);

#define argv_dump(FP, ARGV)    (argv_dump__((FP), (ARGV), #ARGV, __FILE__, __LINE__))
#define argv_dump_fd(FD, ARGV) (argv_dump_fd__((FD), (ARGV), #ARGV, __FILE__, __LINE__))

#undef INTERN

/*======================================================================================================*/
__END_DECLS
#endif /* list.h */
