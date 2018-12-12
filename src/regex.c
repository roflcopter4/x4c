#include "Common.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#define PATSIZ 512
#define _substr(INDEX, SUBJECT, OVECTOR) \
        ((char *)((SUBJECT) + (OVECTOR)[(INDEX)*2]))
#define _substrlen(INDEX, OVECTOR) \
        ((int)((OVECTOR)[(2 * (INDEX)) + 1] - (OVECTOR)[2 * (INDEX)]))

#define SUB_BUFSIZ (8192)

extern b_list * get_pcre2_matches(const bstring *pattern, const bstring *subject, uint32_t flags);

#define HAX(CHAR_)                          \
        __extension__ ({                    \
                char *buf;                  \
                int   ch = (CHAR_);         \
                if (ch) {                   \
                        buf    = alloca(2); \
                        buf[0] = ch;        \
                        buf[1] = '\0';      \
                } else {                    \
                        buf    = alloca(3); \
                        buf[0] = '\\';      \
                        buf[1] = '0';       \
                        buf[2] = '\0';      \
                }                           \
                buf;                        \
        })


b_list *
get_pcre2_matches(const bstring *pattern, const bstring *subject, const uint32_t flags)
{
        int                 errornumber;
        size_t              erroroffset;
        pcre2_match_data_8 *match_data;
        PCRE2_SPTR8         pat_data = (PCRE2_SPTR8)(pattern->data);
        pcre2_code_8       *cre = pcre2_compile_8(pat_data, pattern->slen, flags,
                                                  &errornumber, &erroroffset, NULL);

        if (!cre) {
                uint8_t buf[8192];
                pcre2_get_error_message_8(errornumber, buf, 8192);
                errx(1, "PCRE2 compilation failed at offset %zu: %s\n",
                     erroroffset, buf);
        }

        size_t      offset = 0;
        size_t      slen   = subject->slen;
        PCRE2_SPTR8 subp   = (PCRE2_SPTR8)subject->data;

        for (;;) {
                match_data = pcre2_match_data_create_from_pattern_8(cre, NULL);
                int rcnt   = pcre2_match_8(cre, subp, slen, offset,
                                           flags, match_data, NULL);

                if (rcnt <= 0) {
                        pcre2_match_data_free(match_data);
                        break;
                }

                echo("Found %d matches!", rcnt);
                size_t *ovector = pcre2_get_ovector_pointer_8(match_data);
                /* offset              += *ovector; */

                for (int i = 0; i < rcnt; ++i) {
                        char         tmp[2048];
                        const size_t start = ovector[i*2];
                        const size_t end   = ovector[(i*2)+1];
                        const size_t len   = end - start;

                        memcpy(tmp, (subp + start), len);
                        tmp[len] = '\0';

                        echo("At %ld, found: \"%s\"", start, tmp);
                        echo("Also, ovector: %zu and %zu", ovector[i], ovector[i+1]);
                        echo("Furthermore: \"%s\" - '%s'", (const char *)(subp + end), HAX(((const char *)subp)[end]));
                }

                offset = ovector[1];
                pcre2_match_data_free_8(match_data);
        }

        pcre2_code_free_8(cre);
        return NULL;
}

bstring *
call_pcre2_substitute(const bstring *src, const bstring *pattern, const bstring *replace, const uint32_t flags)
{
        int                 errornumber;
        size_t              erroroffset;
        PCRE2_SPTR8         src_data  = (PCRE2_SPTR8)(src->data);
        PCRE2_SPTR8         pat_data  = (PCRE2_SPTR8)(pattern->data);
        PCRE2_SPTR8         repl_data = (PCRE2_SPTR8)(replace->data);
        pcre2_code_8       *cre = pcre2_compile_8(pat_data, pattern->slen, flags,
                                                  &errornumber, &erroroffset, NULL);

        if (!cre) {
                uint8_t buf[8192];
                pcre2_get_error_message_8(errornumber, buf, 8192);
                errx(1, "PCRE2 compilation failed at offset %zu: %s\n",
                     erroroffset, buf);
        }

        uint8_t buf[8192];
        size_t buflen = 8192;
        pcre2_substitute_8(cre, src_data, src->slen, 0, flags, NULL, NULL,
                           repl_data, replace->slen, buf, &buflen);
        pcre2_code_free_8(cre);

        return b_fromblk(buf, buflen);
}

static void
report_error(const int errornumber, const size_t erroroffset)
{
        uint8_t buf[SUB_BUFSIZ];
        pcre2_get_error_message_8(errornumber, buf, SUB_BUFSIZ);
        errx(1, "PCRE2 compilation failed at offset %zu: %s\n", erroroffset, buf);
}

/*======================================================================================*/

struct v2punct {
        const bstring  find;
        const bstring  replacement;
        pcre2_code_8  *cre;
};

/* This started life as a series of very simple regular expressions. Then I realized
 * that they were replacing things inside of string literals. Thus they ceased to be
 * simple regular expressions. */
static struct v2punct verboten[] = {
        { BT("\\band\\b"),   BT("&&"), NULL },
        { BT("\\bnot\\b ?"), BT("!"),  NULL },
        { BT("\\bor\\b"),    BT("||"), NULL },
        { BT("\\bgt\\b"),    BT(">"),  NULL },
        { BT("\\bge\\b"),    BT(">="), NULL },
        { BT("\\blt\\b"),    BT("<"),  NULL },
        { BT("\\ble\\b"),    BT("<="), NULL },
};

#if 0
        { BT("^(([^']*)|((.*?'.*?')+[^']*))\\band\\b"),   BT("$1&&"), NULL },
        { BT("^(([^']*)|((.*?'.*?')+[^']*))\\bnot\\b ?"), BT("$1!"),  NULL },
        { BT("^(([^']*)|((.*?'.*?')+[^']*))\\bor\\b"),    BT("$1||"), NULL },
        { BT("^(([^']*)|((.*?'.*?')+[^']*))\\bgt\\b"),    BT("$1>"),  NULL },
        { BT("^(([^']*)|((.*?'.*?')+[^']*))\\bge\\b"),    BT("$1>="), NULL },
        { BT("^(([^']*)|((.*?'.*?')+[^']*))\\blt\\b"),    BT("$1<"),  NULL },
        { BT("^(([^']*)|((.*?'.*?')+[^']*))\\ble\\b"),    BT("$1<="), NULL },
#endif

#if 0
static struct v2punct str_method = { BT("'\\.\\[(.*)\\]"), BT("'.($1)"), NULL };
static struct v2punct method = { BT("(?<!\\$)\\b([a-zA-Z_]\\w*?)\\.\\{([^{]+?)\\}"), BT("$1.($2)"), NULL };
static struct v2punct ind = { BT("\\.\\{([^{]+?)\\}"), BT("[$1]"), NULL };
#endif

/*======================================================================================*/

static inline pcre2_code_8 *
init_cre(const uint8_t *find, const size_t len)
{
        int           errornumber;
        size_t        erroroffset;
        pcre2_code_8 *cre = pcre2_compile_8(find, len, 0, &errornumber, &erroroffset, NULL);
        if (!cre)
                report_error(errornumber, erroroffset);
        return cre;
}

static void __attribute__((__constructor__))
init_verboten(void)
{
        for (size_t i = 0; i < ARRSIZ(verboten); ++i) {
                struct v2punct *p = &verboten[i];
                p->cre = init_cre(p->find.data, p->find.slen);
        }
#if 0
        str_method.cre = init_cre(str_method.find.data, str_method.find.slen); 
        method.cre = init_cre(method.find.data, method.find.slen);
        ind.cre = init_cre(ind.find.data, ind.find.slen);
#endif
}

static void __attribute__((__destructor__)) 
clean_verboten(void)
{
        for (size_t i = 0; i < ARRSIZ(verboten); ++i)
                if (verboten[i].cre)
                        pcre2_code_free_8(verboten[i].cre);
#if 0
        pcre2_code_free_8(str_method.cre);
        pcre2_code_free_8(method.cre);
        pcre2_code_free_8(ind.cre);
#endif
}

static int
do_replace(struct v2punct *p, bstring **origp)
{
        assert(origp != NULL && *origp != NULL);
        bstring *orig  = *origp;
        size_t   buflen = SUB_BUFSIZ;
        uint8_t  buf[SUB_BUFSIZ];
        int nr = pcre2_substitute_8(p->cre, orig->data, orig->slen, 0,
                                    PCRE2_SUBSTITUTE_GLOBAL, NULL, NULL,
                                    p->replacement.data, p->replacement.slen, buf, &buflen);
        /* fwrite(buf, 1, buflen, stderr); */
        /* putc('\n', stderr); */
        if (nr > 0) {
                *origp = b_fromblk(buf, buflen);
                b_free(orig);
        }
        return nr;
}

/*======================================================================================*/

#define STRCHR(s, c) ((uint8_t *)strchrnul((char *)(s), (c)))
#define CHECK_BOUND(n) ((i == 0 || orig->data[i-1] == ' ' || orig->data[i-1] == '(') && orig->data[i+(n)] == ' ')

void
de_verbosify(bstring **origp)
{
        bstring *orig = *origp;
        bstring *repl = b_create(orig->slen + 1);

        for (unsigned i = 0; i < orig->slen; ++i) {
                int ch = orig->data[i];
        start:
                switch (ch) {
                case 'a':
                        if (strncmp((char *)(orig->data + i + 1), "nd", 2) == 0 && CHECK_BOUND(3)) {
                                b_catlit(repl, "&&");
                                i += 2;
                        } else {
                                b_catchar(repl, ch);
                        }
                        break;
                case 'n':
                        if (strncmp((char *)(orig->data + i + 1), "ot", 2) == 0 && CHECK_BOUND(3)) {
                                b_catchar(repl, '!');
                                i += 3;
                        } else {
                                b_catchar(repl, ch);
                        }
                        break;
                case 'g':
                        if (orig->data[i+1] == 'e' && CHECK_BOUND(2)) {
                                b_catlit(repl, ">=");
                                i += 1;
                        } else if (orig->data[i+1] == 't' && CHECK_BOUND(2)) {
                                b_catchar(repl, '>');
                                i += 1;
                        } else {
                                b_catchar(repl, ch);
                        }
                        break;
                case 'l':
                        if (orig->data[i+1] == 'e' && CHECK_BOUND(2)) {
                                b_catlit(repl, "<=");
                                i += 1;
                        } else if (orig->data[i+1] == 't' && CHECK_BOUND(2)) {
                                b_catchar(repl, '<');
                                i += 1;
                        } else {
                                b_catchar(repl, ch);
                        }
                        break;
                case 'o':
                        if (orig->data[i+1] == 'r' && CHECK_BOUND(2)) {
                                b_catlit(repl, "||");
                                i += 1;
                        } else {
                                b_catchar(repl, ch);
                        }
                        break;
                case '\'':
                        b_catchar(repl, ch);
                        ++i;
                        for (; i < orig->slen; ++i) {
                                if (orig->data[i] == '\'' && (i > 0 && orig->data[i-1] != '\\')) {
                                        b_catchar(repl, '\'');
                                        ch = orig->data[++i];
                                        goto start;
                                }
                                b_catchar(repl, orig->data[i]);
                        }
                        break;
                default:
                        b_catchar(repl, ch);
                        break;
                }
        }

        const void *ctx = talloc_parent(*origp);
        b_free(*origp);
        *origp = repl;
        talloc_steal(ctx, *origp);
}

#if 0
void
de_verbosify(bstring **orig)
{
        const void *ctx  = talloc_parent(*orig);
        bstring    *repl = *orig;

        for (size_t i = 0; i < ARRSIZ(verboten); ++i) {
                struct v2punct *p = &verboten[i];
                size_t          buflen = SUB_BUFSIZ;
                int64_t loc = b_strchr(repl, '\'');
                uint8_t buf[SUB_BUFSIZ];

                if (loc > 0) {
                        pcre2_substitute_8(p->cre, repl->data, loc, 0, PCRE2_SUBSTITUTE_GLOBAL,
                                           NULL, NULL, p->replacement.data,
                                           p->replacement.slen, buf, &buflen);
                        while (loc > 0) {
                                eprintf("loc is %ld (%s)\n", loc, BS(repl));
                                int64_t end = b_strchrp(repl, '\'', loc+1);
                                while (repl->data[end-1] == '\\')
                                        end = b_strchrp(repl, '\'', end+1) + 1;
                                if (end == repl->slen)
                                        break;

                                size_t len = buflen;
                                buflen = SUB_BUFSIZ - len;

                                pcre2_substitute_8(p->cre, repl->data + end, loc, 0, PCRE2_SUBSTITUTE_GLOBAL,
                                                   NULL, NULL, p->replacement.data,
                                                   p->replacement.slen, buf + len, &buflen);

                                loc = b_strchrp(repl, '\'', end+1);
                        }


#if 0
                        uint8_t *s = STRCHR(buf, '\'');
                        uint8_t *e = STRCHR(s+1, '\'');
                        while (*(e - 1) == '\\')
                                e = STRCHR(s+1, '\'');

                        s = e + 1;
                        e = STRCHR(s, '\'');
                        size_t len = (e) ? (e - s - 1) : (e - s);
#endif

                        /* pcre2_substitute_8(p->cre, s, len, 0, PCRE2_SUBSTITUTE_GLOBAL,
                                           NULL, NULL, p->replacement.data,
                                           p->replacement.slen, buf, &buflen); */

                } else {
                        pcre2_substitute_8(p->cre, repl->data, repl->slen, 0, PCRE2_SUBSTITUTE_GLOBAL,
                                           NULL, NULL, p->replacement.data,
                                           p->replacement.slen, buf, &buflen);
                }

                /* do { */
                        /* uint8_t         buf[SUB_BUFSIZ]; */
                        /* nr = do_replace(p, orig); */
#if 0
                        nr = pcre2_substitute_8(p->cre, repl->data, repl->slen, 0, PCRE2_SUBSTITUTE_GLOBAL,
                                           NULL, NULL, p->replacement.data,
                                           p->replacement.slen, buf, &buflen);

                        fprintf(stderr, "have %d - %s\n", nr, BS(repl));
                        /* nr = do_replace(p, &repl); */
#endif
                /* } while (nr > 0); */

                bstring *tmp = b_fromblk(buf, buflen+1);
                talloc_steal(ctx, tmp);
                b_free(repl);
                repl = tmp;
        }

        *orig = repl;
}
#endif

#if 0
void
convert_indeces(bstring **orig)
{
        int         nr;
        const void *ctx  = talloc_parent(*orig);

        do {
                nr = do_replace(&method, orig);
                fprintf(stderr, "have %d - %s\n", nr, BS(*orig));
        } while (nr > 0);
        do {
                nr = do_replace(&str_method, orig);
                fprintf(stderr, "have %d - %s\n", nr, BS(*orig));
        } while (nr > 0);
        do {
                nr = do_replace(&ind, orig);
                fprintf(stderr, "have %d - %s\n", nr, BS(*orig));
        } while (nr > 0);

        talloc_steal(ctx, *orig);
}
#endif
