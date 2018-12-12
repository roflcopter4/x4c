#include "Common.h"

#define CHECK_BOUND(n)                                                   \
        ((i == 0 || orig->data[i-1] == ' ' || orig->data[i-1] == '(') && \
         orig->data[i + (n)] == ' ')

void
de_verbosify(bstring **origp)
{
        bstring *orig = *origp;
        bstring *repl = b_create(orig->slen + 1);

        for (unsigned i = 0; i < orig->slen; ++i) {
        start:
                switch (orig->data[i]) {
                case 'a':
                        /* and */
                        if (strncmp((char *)&orig->data[i+1], "nd", 2) == 0 && CHECK_BOUND(3)) {
                                b_catlit(repl, "&&");
                                i += 2;
                        } else {
                                b_catchar(repl, orig->data[i]);
                        }
                        break;
                case 'n':
                        /* not */
                        if (strncmp((char *)&orig->data[i+1], "ot", 2) == 0 && CHECK_BOUND(3)) {
                                b_catchar(repl, '!');
                                i += 3;
                        } else {
                                b_catchar(repl, orig->data[i]);
                        }
                        break;
                case 'g':
                        /* ge */
                        if (orig->data[i+1] == 'e' && CHECK_BOUND(2)) {
                                b_catlit(repl, ">=");
                                i += 1;
                        }
                        /* lt */
                        else if (orig->data[i+1] == 't' && CHECK_BOUND(2)) {
                                b_catchar(repl, '>');
                                i += 1;
                        } else {
                                b_catchar(repl, orig->data[i]);
                        }
                        break;
                case 'l':
                        /* le */
                        if (orig->data[i+1] == 'e' && CHECK_BOUND(2)) {
                                b_catlit(repl, "<=");
                                i += 1;
                        }
                        /* gt */
                        else if (orig->data[i+1] == 't' && CHECK_BOUND(2)) {
                                b_catchar(repl, '<');
                                i += 1;
                        } else {
                                b_catchar(repl, orig->data[i]);
                        }
                        break;
                case 'o':
                        /* or */
                        if (orig->data[i+1] == 'r' && CHECK_BOUND(2)) {
                                b_catlit(repl, "||");
                                i += 1;
                        } else {
                                b_catchar(repl, orig->data[i]);
                        }
                        break;
                case '\'':
                        /* Don't replace anything in string literals. */
                        b_catchar(repl, orig->data[i++]);
                        for (; i < orig->slen; ++i) {
                                if (orig->data[i] == '\'' && (i > 0 && orig->data[i-1] != '\\')) {
                                        b_catchar(repl, orig->data[i++]);
                                        goto start;
                                }
                                b_catchar(repl, orig->data[i]);
                        }
                        break;
                default:
                        b_catchar(repl, orig->data[i]);
                        break;
                }
        }

        const void *ctx = talloc_parent(*origp);
        b_free(*origp);
        *origp = repl;
        talloc_steal(ctx, *origp);
}
