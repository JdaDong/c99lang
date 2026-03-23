/*
 * c99lang - Preprocessor macro expansion
 */
#include "preprocessor/macro.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

static void buf_append(char **out, size_t *len, size_t *cap,
                       const char *s, size_t slen) {
    while (*len + slen + 1 > *cap) {
        *cap = *cap == 0 ? 256 : *cap * 2;
        *out = realloc(*out, *cap);
    }
    memcpy(*out + *len, s, slen);
    *len += slen;
    (*out)[*len] = '\0';
}

static void buf_append_cstr(char **out, size_t *len, size_t *cap,
                            const char *s) {
    buf_append(out, len, cap, s, strlen(s));
}

void macro_expand(Preprocessor *pp, Macro *m, const char *args[],
                  int nargs, char **out, size_t *out_len, size_t *out_cap) {
    if (!m->body) return;

    const char *body = m->body;
    size_t body_len = strlen(body);

    for (size_t i = 0; i < body_len; i++) {
        /* Check for parameter substitution */
        if (m->is_function_like && (isalpha(body[i]) || body[i] == '_')) {
            size_t start = i;
            while (i < body_len && (isalnum(body[i]) || body[i] == '_')) i++;
            size_t plen = i - start;
            i--;

            /* Check if it's a parameter name */
            bool found = false;
            int pidx = 0;
            for (MacroParam *p = m->params; p; p = p->next, pidx++) {
                if (strlen(p->name) == plen &&
                    memcmp(p->name, body + start, plen) == 0) {
                    if (pidx < nargs && args[pidx]) {
                        buf_append_cstr(out, out_len, out_cap, args[pidx]);
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                buf_append(out, out_len, out_cap, body + start, plen);
            }
        } else {
            buf_append(out, out_len, out_cap, body + i, 1);
        }
    }
}

void macro_init_builtins(Preprocessor *pp) {
    /* __LINE__ and __FILE__ are handled dynamically during expansion */

    /* __DATE__ */
    {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char buf[16];
        strftime(buf, sizeof(buf), "\"%b %d %Y\"", tm);
        pp_define(pp, "__DATE__", buf);
    }

    /* __TIME__ */
    {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char buf[16];
        strftime(buf, sizeof(buf), "\"%H:%M:%S\"", tm);
        pp_define(pp, "__TIME__", buf);
    }

    /* Standard C version */
    pp_define(pp, "__STDC__", "1");
    pp_define(pp, "__STDC_VERSION__", "199901L");
    pp_define(pp, "__STDC_HOSTED__", "1");

    /* Compiler identification */
    pp_define(pp, "__c99c__", "1");
}
