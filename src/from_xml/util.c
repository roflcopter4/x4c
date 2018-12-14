#include "Common.h"

#define CHECK_BOUND(n)                                                   \
        ((i == 0 || orig->data[i-1] == ' ' || orig->data[i-1] == '(') && \
         orig->data[i + (n)] == ' ')

#define DATA(n) (((char *)orig->data)[n])

#define CHECK_PAREN(val)                       \
        do {                                   \
                unsigned x = (val);            \
                while (isspace(DATA(x)))       \
                        ++x;                   \
                if (DATA(x) == '(') {          \
                        i             = x + 1; \
                        *need_close++ = false; \
                } else {                       \
                        i = x;                 \
                        *need_close++ = true;  \
                        b_catchar(repl, '(');  \
                }                              \
        } while (0)

void
de_verbosify(bstring **origp)
{
#if 0
        bool     need_close_a[2048 * 3];
        bool    *need_close = need_close_a;
        uint8_t  tern_status_a[2048];
        uint8_t *tern_status = tern_status_a;
#endif
        bstring *orig = *origp;
        bstring *repl = b_create(orig->slen + 1);
        /* int      in_tern = 0; */
        unsigned i       = 0;

        while (isspace(DATA(i))) {
                b_catchar(repl, DATA(i));
                ++i;
        }
        if (strncmp(&DATA(i), "if", 2) == 0) {
                b_catlit(repl, "if");
                i += 2;
        }
        for (; i < orig->slen; ++i) {
        start:
                switch (DATA(i)) {
                case 'a':
                        /* and */
                        if (strncmp(&DATA(i+1), "nd", 2) == 0 && CHECK_BOUND(3)) {
                                b_catlit(repl, "&&");
                                i += 2;
                        } else {
                                b_catchar(repl, DATA(i));
                        }
                        break;
                case 'n':
                        /* not */
                        if (strncmp(&DATA(i+1), "ot", 2) == 0 && CHECK_BOUND(3)) {
                                b_catchar(repl, '!');
                                i += 3;
                        } else {
                                b_catchar(repl, DATA(i));
                        }
                        break;
                case 'g':
                        /* ge */
                        if (DATA(i+1) == 'e' && CHECK_BOUND(2)) {
                                b_catlit(repl, ">=");
                                i += 1;
                        }
                        /* lt */
                        else if (DATA(i+1) == 't' && CHECK_BOUND(2)) {
                                b_catchar(repl, '>');
                                i += 1;
                        } else {
                                b_catchar(repl, DATA(i));
                        }
                        break;
                case 'l':
                        /* le */
                        if (DATA(i+1) == 'e' && CHECK_BOUND(2)) {
                                b_catlit(repl, "<=");
                                i += 1;
                        }
                        /* gt */
                        else if (DATA(i+1) == 't' && CHECK_BOUND(2)) {
                                b_catchar(repl, '<');
                                i += 1;
                        } else {
                                b_catchar(repl, DATA(i));
                        }
                        break;
                case 'o':
                        /* or */
                        if (DATA(i+1) == 'r' && CHECK_BOUND(2)) {
                                b_catlit(repl, "||");
                                i += 1;
                        } else {
                                b_catchar(repl, DATA(i));
                        }
                        break;
#if 0
                case 'i':
                        /* if else then */
                        if (DATA(i+1) == 'f' && CHECK_BOUND(2)) {
#if 0
                                unsigned x = i + 2;
                                while (isspace(DATA(x)))
                                        ++x;
#endif
                                b_catlit(repl, "if ");
                                CHECK_PAREN(i + 2);
#if 0
                                if (DATA(x) == '(') {
                                        i = x+1;
                                        *need_close++ = false;
                                } else {
                                        i += 3;
                                        *need_close++ = true;
                                        b_catchar(repl, '(');
                                }
#endif
                                *++tern_status = 1;
                                goto start;
                        } else {
                                b_catchar(repl, 'i');
                        }
                        break;
                case 't':
                        if (*tern_status == 1 && strncmp(&DATA(i+1), "hen", 3) == 0 && CHECK_BOUND(4)) {
                                --repl->slen;
                                if (*--need_close)
                                        b_catlit(repl, ") then");
                                else
                                        b_catlit(repl, " then");
                                CHECK_PAREN(i + 4);
                                *tern_status = 2;
                                goto start;
                        } else {
                                b_catchar(repl, 't');
                        }
                        break;
                case 'e':
                        if (*tern_status == 1 && strncmp(&DATA(i+1), "lse", 3) == 0 && CHECK_BOUND(4)) {
                                --repl->slen;
                                if (*--need_close)
                                        b_catlit(repl, ") else");
                                else
                                        b_catlit(repl, " else");
                                CHECK_PAREN(i + 4);
                                *tern_status-- = 0;
                                goto start;
                        } else {
                                b_catchar(repl, 'e');
                        }
                        break;
#endif
                case '\'':
                        /* Don't replace anything in string literals. */
                        b_catchar(repl, DATA(i++));
                        for (; i < orig->slen; ++i) {
                                if (DATA(i) == '\'' && (i > 0 && DATA(i-1) != '\\')) {
                                        b_catchar(repl, DATA(i++));
                                        goto start;
                                }
                                b_catchar(repl, DATA(i));
                        }
                        break;
                default:
                        b_catchar(repl, DATA(i));
                        break;
                }
        }

        const void *ctx = talloc_parent(*origp);
        b_free(*origp);
        *origp = repl;
        talloc_steal(ctx, *origp);
}
