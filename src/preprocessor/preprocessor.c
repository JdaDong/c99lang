/*
 * c99lang - C99 Preprocessor
 * Handles: #include, #define, #undef, #if/#ifdef/#ifndef/#elif/#else/#endif,
 *          #line, #error, #pragma
 */
#include "preprocessor/preprocessor.h"
#include "preprocessor/macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static unsigned pp_hash(const char *name) {
    unsigned h = 5381;
    for (; *name; name++)
        h = ((h << 5) + h) + (unsigned char)*name;
    return h;
}

void pp_init(Preprocessor *pp, Strtab *strtab, ErrorCtx *err) {
    arena_init(&pp->arena);
    pp->strtab = strtab;
    pp->err = err;
    memset(pp->macros, 0, sizeof(pp->macros));
    vec_init(&pp->include_paths);
    pp->filename = NULL;
    pp->line = 1;

    macro_init_builtins(pp);
}

void pp_free(Preprocessor *pp) {
    arena_free(&pp->arena);
    vec_free(&pp->include_paths);
}

void pp_add_include_path(Preprocessor *pp, const char *path) {
    vec_push(&pp->include_paths, (void *)path);
}

void pp_define(Preprocessor *pp, const char *name, const char *body) {
    unsigned idx = pp_hash(name) % PP_MACRO_BUCKETS;

    /* Check for redefinition */
    for (Macro *m = pp->macros[idx]; m; m = m->next) {
        if (strcmp(m->name, name) == 0) {
            m->body = body ? arena_strdup(&pp->arena, body) : NULL;
            return;
        }
    }

    Macro *m = (Macro *)arena_calloc(&pp->arena, sizeof(Macro));
    m->name = arena_strdup(&pp->arena, name);
    m->body = body ? arena_strdup(&pp->arena, body) : NULL;
    m->params = NULL;
    m->param_count = 0;
    m->is_function_like = false;
    m->is_builtin = false;
    m->next = pp->macros[idx];
    pp->macros[idx] = m;
}

void pp_undef(Preprocessor *pp, const char *name) {
    unsigned idx = pp_hash(name) % PP_MACRO_BUCKETS;
    Macro **prev = &pp->macros[idx];
    for (Macro *m = *prev; m; prev = &m->next, m = m->next) {
        if (strcmp(m->name, name) == 0) {
            *prev = m->next;
            return;
        }
    }
}

Macro *pp_lookup(Preprocessor *pp, const char *name) {
    unsigned idx = pp_hash(name) % PP_MACRO_BUCKETS;
    for (Macro *m = pp->macros[idx]; m; m = m->next) {
        if (strcmp(m->name, name) == 0) return m;
    }
    return NULL;
}

/* ---- Internal helpers ---- */

typedef struct PPCtx {
    Preprocessor *pp;
    const char *src;
    size_t src_len;
    size_t pos;
    int line;
    const char *filename;

    /* Output buffer */
    char *out;
    size_t out_len;
    size_t out_cap;

    /* Conditional compilation stack */
    int if_depth;
    bool if_active[64];    /* whether current branch is active */
    bool if_seen_true[64]; /* whether any branch was true */
} PPCtx;

static void pp_buf_append(PPCtx *ctx, const char *s, size_t len) {
    while (ctx->out_len + len + 1 > ctx->out_cap) {
        ctx->out_cap = ctx->out_cap == 0 ? 4096 : ctx->out_cap * 2;
        ctx->out = realloc(ctx->out, ctx->out_cap);
    }
    memcpy(ctx->out + ctx->out_len, s, len);
    ctx->out_len += len;
    ctx->out[ctx->out_len] = '\0';
}

static void pp_buf_putc(PPCtx *ctx, char c) {
    pp_buf_append(ctx, &c, 1);
}

static inline int pp_ch(PPCtx *ctx) {
    return ctx->pos < ctx->src_len ? (unsigned char)ctx->src[ctx->pos] : '\0';
}

static inline void pp_adv(PPCtx *ctx) {
    if (ctx->pos < ctx->src_len) {
        if (ctx->src[ctx->pos] == '\n') ctx->line++;
        ctx->pos++;
    }
}

static void pp_skip_line(PPCtx *ctx) {
    while (ctx->pos < ctx->src_len && ctx->src[ctx->pos] != '\n') {
        ctx->pos++;
    }
}

static void pp_skip_spaces(PPCtx *ctx) {
    while (ctx->pos < ctx->src_len &&
           (ctx->src[ctx->pos] == ' ' || ctx->src[ctx->pos] == '\t')) {
        ctx->pos++;
    }
}

static char *pp_read_ident(PPCtx *ctx) {
    size_t start = ctx->pos;
    while (ctx->pos < ctx->src_len &&
           (isalnum(ctx->src[ctx->pos]) || ctx->src[ctx->pos] == '_')) {
        ctx->pos++;
    }
    size_t len = ctx->pos - start;
    if (len == 0) return NULL;
    return arena_strndup(&ctx->pp->arena, ctx->src + start, len);
}

static char *pp_read_rest_of_line(PPCtx *ctx) {
    pp_skip_spaces(ctx);
    size_t start = ctx->pos;
    /* Handle line continuation */
    size_t end = start;
    while (ctx->pos < ctx->src_len && ctx->src[ctx->pos] != '\n') {
        if (ctx->src[ctx->pos] == '\\' && ctx->pos + 1 < ctx->src_len &&
            ctx->src[ctx->pos + 1] == '\n') {
            ctx->pos += 2;
            ctx->line++;
            continue;
        }
        ctx->pos++;
    }
    end = ctx->pos;
    /* Trim trailing whitespace */
    while (end > start && (ctx->src[end-1] == ' ' || ctx->src[end-1] == '\t'))
        end--;
    return arena_strndup(&ctx->pp->arena, ctx->src + start, end - start);
}

static bool pp_is_active(PPCtx *ctx) {
    if (ctx->if_depth <= 0) return true;
    return ctx->if_active[ctx->if_depth];
}

static void pp_handle_define(PPCtx *ctx) {
    pp_skip_spaces(ctx);
    char *name = pp_read_ident(ctx);
    if (!name) {
        SrcLoc loc = { ctx->filename, ctx->line, 1 };
        diag_error(ctx->pp->err, loc, "expected macro name after #define");
        pp_skip_line(ctx);
        return;
    }

    /* Check for function-like macro: #define FOO( */
    if (pp_ch(ctx) == '(' && ctx->pos > 0 &&
        ctx->src[ctx->pos - 1] != ' ' && ctx->src[ctx->pos - 1] != '\t') {
        /* Function-like macro */
        pp_adv(ctx); /* skip ( */

        unsigned idx = pp_hash(name) % PP_MACRO_BUCKETS;
        Macro *m = (Macro *)arena_calloc(&ctx->pp->arena, sizeof(Macro));
        m->name = name;
        m->is_function_like = true;
        m->param_count = 0;

        MacroParam *last_param = NULL;
        pp_skip_spaces(ctx);
        while (pp_ch(ctx) != ')' && pp_ch(ctx) != '\0' && pp_ch(ctx) != '\n') {
            char *pname = pp_read_ident(ctx);
            if (pname) {
                MacroParam *p = (MacroParam *)arena_calloc(&ctx->pp->arena,
                                                            sizeof(MacroParam));
                p->name = pname;
                p->next = NULL;
                if (last_param) last_param->next = p;
                else m->params = p;
                last_param = p;
                m->param_count++;
            }
            pp_skip_spaces(ctx);
            if (pp_ch(ctx) == ',') { pp_adv(ctx); pp_skip_spaces(ctx); }
        }
        if (pp_ch(ctx) == ')') pp_adv(ctx);

        char *body = pp_read_rest_of_line(ctx);
        m->body = body;
        m->next = ctx->pp->macros[idx];
        ctx->pp->macros[idx] = m;
    } else {
        /* Object-like macro */
        char *body = pp_read_rest_of_line(ctx);
        pp_define(ctx->pp, name, body);
    }
}

static void pp_handle_include(PPCtx *ctx) {
    pp_skip_spaces(ctx);
    char delim_end = 0;

    if (pp_ch(ctx) == '"') {
        delim_end = '"';
    } else if (pp_ch(ctx) == '<') {
        delim_end = '>';
    } else {
        SrcLoc loc = { ctx->filename, ctx->line, 1 };
        diag_error(ctx->pp->err, loc, "expected filename after #include");
        pp_skip_line(ctx);
        return;
    }
    pp_adv(ctx);

    size_t start = ctx->pos;
    while (ctx->pos < ctx->src_len && ctx->src[ctx->pos] != delim_end &&
           ctx->src[ctx->pos] != '\n') {
        ctx->pos++;
    }
    char *filename = arena_strndup(&ctx->pp->arena, ctx->src + start,
                                    ctx->pos - start);
    if (pp_ch(ctx) == delim_end) pp_adv(ctx);
    pp_skip_line(ctx);

    /* Try to read the include file */
    FILE *f = NULL;

    /* Try current directory first (for "..." includes) */
    if (delim_end == '"') {
        f = fopen(filename, "rb");
    }

    /* Search include paths */
    if (!f) {
        for (size_t i = 0; i < ctx->pp->include_paths.size; i++) {
            const char *dir = (const char *)vec_get(&ctx->pp->include_paths, i);
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir, filename);
            f = fopen(path, "rb");
            if (f) break;
        }
    }

    if (!f) {
        SrcLoc loc = { ctx->filename, ctx->line, 1 };
        diag_error(ctx->pp->err, loc, "cannot open include file '%s'", filename);
        return;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(fsize + 1);
    fread(content, 1, fsize, f);
    content[fsize] = '\0';
    fclose(f);

    /* Recursively preprocess included file */
    char *processed = pp_process(ctx->pp, content, fsize, filename);
    if (processed) {
        pp_buf_append(ctx, processed, strlen(processed));
        free(processed);
    }
    free(content);
}

static long pp_eval_constant(PPCtx *ctx, const char *expr) {
    /* Simplified constant expression evaluator for #if */
    /* Handles: integer literals, defined(X), basic arithmetic, !, &&, || */
    while (*expr == ' ' || *expr == '\t') expr++;

    if (*expr == '\0') return 0;

    /* defined(MACRO) or defined MACRO */
    if (strncmp(expr, "defined", 7) == 0) {
        expr += 7;
        while (*expr == ' ' || *expr == '\t') expr++;
        bool has_paren = false;
        if (*expr == '(') { has_paren = true; expr++; }
        while (*expr == ' ' || *expr == '\t') expr++;
        const char *name_start = expr;
        while (isalnum(*expr) || *expr == '_') expr++;
        char name[256];
        size_t nlen = expr - name_start;
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, name_start, nlen);
        name[nlen] = '\0';
        if (has_paren) { while (*expr != ')' && *expr) expr++; }
        return pp_lookup(ctx->pp, name) ? 1 : 0;
    }

    /* Negation */
    if (*expr == '!') {
        return !pp_eval_constant(ctx, expr + 1);
    }

    /* Simple integer */
    if (isdigit(*expr)) {
        return strtol(expr, NULL, 0);
    }

    /* Identifier: check if macro is defined and non-zero */
    if (isalpha(*expr) || *expr == '_') {
        const char *start = expr;
        while (isalnum(*expr) || *expr == '_') expr++;
        char name[256];
        size_t nlen = expr - start;
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, start, nlen);
        name[nlen] = '\0';
        Macro *m = pp_lookup(ctx->pp, name);
        if (m && m->body) return strtol(m->body, NULL, 0);
        return 0;
    }

    return 0;
}

static void pp_handle_directive(PPCtx *ctx) {
    pp_skip_spaces(ctx);
    char *directive = pp_read_ident(ctx);
    if (!directive) {
        pp_skip_line(ctx);
        return;
    }

    /* Conditional compilation directives always processed */
    if (strcmp(directive, "ifdef") == 0) {
        bool parent_active = pp_is_active(ctx);
        ctx->if_depth++;
        pp_skip_spaces(ctx);
        char *name = pp_read_ident(ctx);
        bool cond = name && pp_lookup(ctx->pp, name) != NULL;
        ctx->if_active[ctx->if_depth] = parent_active ? cond : false;
        ctx->if_seen_true[ctx->if_depth] = cond;
        pp_skip_line(ctx);
        return;
    }
    if (strcmp(directive, "ifndef") == 0) {
        ctx->if_depth++;
        pp_skip_spaces(ctx);
        char *name = pp_read_ident(ctx);
        bool cond = !name || pp_lookup(ctx->pp, name) == NULL;
        /* Only active if parent is active */
        bool parent_active = ctx->if_depth > 1 ? ctx->if_active[ctx->if_depth - 1] : true;
        ctx->if_active[ctx->if_depth] = parent_active ? cond : false;
        ctx->if_seen_true[ctx->if_depth] = cond;
        pp_skip_line(ctx);
        return;
    }
    if (strcmp(directive, "if") == 0) {
        ctx->if_depth++;
        char *expr = pp_read_rest_of_line(ctx);
        bool parent_active = ctx->if_depth > 1 ? ctx->if_active[ctx->if_depth - 1] : true;
        bool cond = parent_active ? (pp_eval_constant(ctx, expr) != 0) : false;
        ctx->if_active[ctx->if_depth] = cond;
        ctx->if_seen_true[ctx->if_depth] = cond;
        return;
    }
    if (strcmp(directive, "elif") == 0) {
        if (ctx->if_depth <= 0) {
            SrcLoc loc = { ctx->filename, ctx->line, 1 };
            diag_error(ctx->pp->err, loc, "#elif without #if");
        } else {
            char *expr = pp_read_rest_of_line(ctx);
            bool parent_active = ctx->if_depth > 1 ? ctx->if_active[ctx->if_depth - 1] : true;
            if (ctx->if_seen_true[ctx->if_depth] || !parent_active) {
                ctx->if_active[ctx->if_depth] = false;
            } else {
                bool cond = pp_eval_constant(ctx, expr) != 0;
                ctx->if_active[ctx->if_depth] = cond;
                if (cond) ctx->if_seen_true[ctx->if_depth] = true;
            }
        }
        return;
    }
    if (strcmp(directive, "else") == 0) {
        if (ctx->if_depth <= 0) {
            SrcLoc loc = { ctx->filename, ctx->line, 1 };
            diag_error(ctx->pp->err, loc, "#else without #if");
        } else {
            bool parent_active = ctx->if_depth > 1 ? ctx->if_active[ctx->if_depth - 1] : true;
            ctx->if_active[ctx->if_depth] =
                parent_active && !ctx->if_seen_true[ctx->if_depth];
        }
        pp_skip_line(ctx);
        return;
    }
    if (strcmp(directive, "endif") == 0) {
        if (ctx->if_depth <= 0) {
            SrcLoc loc = { ctx->filename, ctx->line, 1 };
            diag_error(ctx->pp->err, loc, "#endif without #if");
        } else {
            ctx->if_depth--;
        }
        pp_skip_line(ctx);
        return;
    }

    /* Other directives: only process if active */
    if (!pp_is_active(ctx)) {
        pp_skip_line(ctx);
        return;
    }

    if (strcmp(directive, "define") == 0) {
        pp_handle_define(ctx);
    } else if (strcmp(directive, "undef") == 0) {
        pp_skip_spaces(ctx);
        char *name = pp_read_ident(ctx);
        if (name) pp_undef(ctx->pp, name);
        pp_skip_line(ctx);
    } else if (strcmp(directive, "include") == 0) {
        pp_handle_include(ctx);
    } else if (strcmp(directive, "error") == 0) {
        char *msg = pp_read_rest_of_line(ctx);
        SrcLoc loc = { ctx->filename, ctx->line, 1 };
        diag_error(ctx->pp->err, loc, "#error %s", msg);
    } else if (strcmp(directive, "warning") == 0) {
        char *msg = pp_read_rest_of_line(ctx);
        SrcLoc loc = { ctx->filename, ctx->line, 1 };
        diag_warn(ctx->pp->err, loc, "#warning %s", msg);
    } else if (strcmp(directive, "line") == 0) {
        char *rest = pp_read_rest_of_line(ctx);
        ctx->line = atoi(rest);
    } else if (strcmp(directive, "pragma") == 0) {
        /* Ignore pragmas for now */
        pp_skip_line(ctx);
    } else {
        SrcLoc loc = { ctx->filename, ctx->line, 1 };
        diag_warn(ctx->pp->err, loc, "unknown preprocessor directive '#%s'",
                  directive);
        pp_skip_line(ctx);
    }
}

/* ---- Main preprocess loop ---- */

char *pp_process(Preprocessor *pp, const char *src, size_t len,
                 const char *filename) {
    PPCtx ctx = {0};
    ctx.pp = pp;
    ctx.src = src;
    ctx.src_len = len;
    ctx.pos = 0;
    ctx.line = 1;
    ctx.filename = filename;
    ctx.out = NULL;
    ctx.out_len = 0;
    ctx.out_cap = 0;
    ctx.if_depth = 0;

    while (ctx.pos < ctx.src_len) {
        /* Skip to significant character */
        int c = pp_ch(&ctx);

        /* Preprocessor directive (# at start of line) */
        if (c == '#') {
            pp_adv(&ctx); /* skip # */
            pp_handle_directive(&ctx);
            /* Emit newline to maintain line numbers */
            pp_buf_putc(&ctx, '\n');
            continue;
        }

        if (!pp_is_active(&ctx)) {
            /* In inactive conditional block, skip everything except directives */
            if (c == '\n') {
                pp_buf_putc(&ctx, '\n');
                pp_adv(&ctx);
            } else {
                pp_adv(&ctx);
            }
            continue;
        }

        /* Handle __LINE__ and __FILE__ substitution */
        if ((isalpha(c) || c == '_')) {
            size_t start = ctx.pos;
            while (ctx.pos < ctx.src_len &&
                   (isalnum(ctx.src[ctx.pos]) || ctx.src[ctx.pos] == '_')) {
                ctx.pos++;
            }
            size_t ilen = ctx.pos - start;
            char ident[256];
            if (ilen >= sizeof(ident)) ilen = sizeof(ident) - 1;
            memcpy(ident, ctx.src + start, ilen);
            ident[ilen] = '\0';

            /* Check for __LINE__ */
            if (strcmp(ident, "__LINE__") == 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", ctx.line);
                pp_buf_append(&ctx, buf, strlen(buf));
                continue;
            }
            /* Check for __FILE__ */
            if (strcmp(ident, "__FILE__") == 0) {
                pp_buf_putc(&ctx, '"');
                pp_buf_append(&ctx, ctx.filename, strlen(ctx.filename));
                pp_buf_putc(&ctx, '"');
                continue;
            }

            /* Check for macro expansion */
            Macro *m = pp_lookup(pp, ident);
            if (m && !m->is_function_like) {
                if (m->body) {
                    pp_buf_append(&ctx, m->body, strlen(m->body));
                }
                continue;
            }
            if (m && m->is_function_like) {
                /* Check for ( */
                size_t save_pos = ctx.pos;
                while (ctx.pos < ctx.src_len &&
                       (ctx.src[ctx.pos] == ' ' || ctx.src[ctx.pos] == '\t'))
                    ctx.pos++;
                if (ctx.pos < ctx.src_len && ctx.src[ctx.pos] == '(') {
                    ctx.pos++; /* skip ( */
                    /* Read arguments */
                    const char *args[32] = {0};
                    char arg_bufs[32][1024];
                    int nargs = 0;
                    int paren_depth = 1;
                    size_t arg_start = ctx.pos;

                    while (ctx.pos < ctx.src_len && paren_depth > 0) {
                        if (ctx.src[ctx.pos] == '(') paren_depth++;
                        else if (ctx.src[ctx.pos] == ')') {
                            paren_depth--;
                            if (paren_depth == 0) {
                                size_t alen = ctx.pos - arg_start;
                                if (alen >= sizeof(arg_bufs[0])) alen = sizeof(arg_bufs[0]) - 1;
                                memcpy(arg_bufs[nargs], ctx.src + arg_start, alen);
                                arg_bufs[nargs][alen] = '\0';
                                /* Trim whitespace */
                                char *a = arg_bufs[nargs];
                                while (*a == ' ' || *a == '\t') a++;
                                args[nargs] = a;
                                nargs++;
                                ctx.pos++;
                                break;
                            }
                        } else if (ctx.src[ctx.pos] == ',' && paren_depth == 1) {
                            size_t alen = ctx.pos - arg_start;
                            if (alen >= sizeof(arg_bufs[0])) alen = sizeof(arg_bufs[0]) - 1;
                            memcpy(arg_bufs[nargs], ctx.src + arg_start, alen);
                            arg_bufs[nargs][alen] = '\0';
                            char *a = arg_bufs[nargs];
                            while (*a == ' ' || *a == '\t') a++;
                            args[nargs] = a;
                            nargs++;
                            ctx.pos++;
                            arg_start = ctx.pos;
                            continue;
                        }
                        ctx.pos++;
                    }

                    macro_expand(pp, m, args, nargs,
                                 &ctx.out, &ctx.out_len, &ctx.out_cap);
                    continue;
                } else {
                    ctx.pos = save_pos;
                }
            }

            /* Plain identifier - output as-is */
            pp_buf_append(&ctx, ctx.src + start, ilen);
            continue;
        }

        /* String literal - pass through */
        if (c == '"') {
            pp_buf_putc(&ctx, '"');
            pp_adv(&ctx);
            while (ctx.pos < ctx.src_len && ctx.src[ctx.pos] != '"') {
                if (ctx.src[ctx.pos] == '\\' && ctx.pos + 1 < ctx.src_len) {
                    pp_buf_putc(&ctx, ctx.src[ctx.pos]);
                    pp_adv(&ctx);
                }
                pp_buf_putc(&ctx, ctx.src[ctx.pos]);
                pp_adv(&ctx);
            }
            if (ctx.pos < ctx.src_len) {
                pp_buf_putc(&ctx, '"');
                pp_adv(&ctx);
            }
            continue;
        }

        /* Character literal - pass through */
        if (c == '\'') {
            pp_buf_putc(&ctx, '\'');
            pp_adv(&ctx);
            while (ctx.pos < ctx.src_len && ctx.src[ctx.pos] != '\'') {
                if (ctx.src[ctx.pos] == '\\' && ctx.pos + 1 < ctx.src_len) {
                    pp_buf_putc(&ctx, ctx.src[ctx.pos]);
                    pp_adv(&ctx);
                }
                pp_buf_putc(&ctx, ctx.src[ctx.pos]);
                pp_adv(&ctx);
            }
            if (ctx.pos < ctx.src_len) {
                pp_buf_putc(&ctx, '\'');
                pp_adv(&ctx);
            }
            continue;
        }

        /* Line comment - skip */
        if (c == '/' && ctx.pos + 1 < ctx.src_len && ctx.src[ctx.pos + 1] == '/') {
            while (ctx.pos < ctx.src_len && ctx.src[ctx.pos] != '\n')
                pp_adv(&ctx);
            continue;
        }

        /* Block comment - replace with space */
        if (c == '/' && ctx.pos + 1 < ctx.src_len && ctx.src[ctx.pos + 1] == '*') {
            pp_adv(&ctx); pp_adv(&ctx);
            while (ctx.pos < ctx.src_len) {
                if (ctx.src[ctx.pos] == '*' && ctx.pos + 1 < ctx.src_len &&
                    ctx.src[ctx.pos + 1] == '/') {
                    pp_adv(&ctx); pp_adv(&ctx);
                    break;
                }
                if (ctx.src[ctx.pos] == '\n') pp_buf_putc(&ctx, '\n');
                pp_adv(&ctx);
            }
            pp_buf_putc(&ctx, ' ');
            continue;
        }

        /* Regular character - pass through */
        pp_buf_putc(&ctx, c);
        pp_adv(&ctx);
    }

    /* Check for unterminated #if */
    if (ctx.if_depth > 0) {
        SrcLoc loc = { ctx.filename, ctx.line, 1 };
        diag_error(ctx.pp->err, loc, "unterminated #if");
    }

    return ctx.out;
}
