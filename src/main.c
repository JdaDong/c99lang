/*
 * c99c - A C99 Compiler
 * Main entry point
 *
 * Usage: c99c [options] <source.c>
 *
 * Options:
 *   -o <file>     Output file name
 *   -E            Preprocess only
 *   -S            Compile to assembly only
 *   --dump-tokens Dump token stream
 *   --dump-ast    Dump AST
 *   --dump-ir     Dump IR
 *   -I<dir>       Add include search path
 *   -D<name>=<val> Define preprocessor macro
 *   -w            Suppress warnings
 *   -Werror       Treat warnings as errors
 *   -h, --help    Show help
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "util/arena.h"
#include "util/strtab.h"
#include "util/error.h"
#include "lexer/lexer.h"
#include "preprocessor/preprocessor.h"
#include "parser/parser.h"
#include "parser/ast.h"
#include "sema/sema.h"
#include "ir/ir.h"
#include "ir/ir_gen.h"
#include "codegen/codegen_x86_64.h"

typedef struct Options {
    const char *input_file;
    const char *output_file;
    bool preprocess_only;     /* -E */
    bool compile_only;        /* -S */
    bool dump_tokens;
    bool dump_ast;
    bool dump_ir;
    bool suppress_warnings;
    bool warnings_as_errors;
    Vector include_paths;     /* const char* */
    Vector defines;           /* const char* ("NAME=VALUE") */
} Options;

static void print_usage(const char *prog) {
    fprintf(stderr,
        "c99c - A C99 Compiler\n"
        "\n"
        "Usage: %s [options] <source.c>\n"
        "\n"
        "Options:\n"
        "  -o <file>       Output file name\n"
        "  -E              Preprocess only (output to stdout)\n"
        "  -S              Compile to assembly (.s) only\n"
        "  --dump-tokens   Dump token stream and exit\n"
        "  --dump-ast      Dump AST and exit\n"
        "  --dump-ir       Dump IR and exit\n"
        "  -I<dir>         Add include search path\n"
        "  -D<name>[=val]  Define preprocessor macro\n"
        "  -w              Suppress all warnings\n"
        "  -Werror         Treat warnings as errors\n"
        "  -h, --help      Show this help\n"
        "\n"
        "Examples:\n"
        "  %s hello.c -o hello.s     Compile to assembly\n"
        "  %s -E hello.c             Preprocess only\n"
        "  %s --dump-ast hello.c     Show AST\n",
        prog, prog, prog, prog);
}

static bool parse_options(int argc, char **argv, Options *opts) {
    memset(opts, 0, sizeof(*opts));
    vec_init(&opts->include_paths);
    vec_init(&opts->defines);

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(arg, "-E") == 0) {
            opts->preprocess_only = true;
        } else if (strcmp(arg, "-S") == 0) {
            opts->compile_only = true;
        } else if (strcmp(arg, "--dump-tokens") == 0) {
            opts->dump_tokens = true;
        } else if (strcmp(arg, "--dump-ast") == 0) {
            opts->dump_ast = true;
        } else if (strcmp(arg, "--dump-ir") == 0) {
            opts->dump_ir = true;
        } else if (strcmp(arg, "-w") == 0) {
            opts->suppress_warnings = true;
        } else if (strcmp(arg, "-Werror") == 0) {
            opts->warnings_as_errors = true;
        } else if (strcmp(arg, "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "c99c: error: missing argument to '-o'\n");
                return false;
            }
            opts->output_file = argv[++i];
        } else if (strncmp(arg, "-I", 2) == 0) {
            const char *path = arg + 2;
            if (*path == '\0') {
                if (i + 1 >= argc) {
                    fprintf(stderr, "c99c: error: missing argument to '-I'\n");
                    return false;
                }
                path = argv[++i];
            }
            vec_push(&opts->include_paths, (void *)path);
        } else if (strncmp(arg, "-D", 2) == 0) {
            const char *def = arg + 2;
            if (*def == '\0') {
                if (i + 1 >= argc) {
                    fprintf(stderr, "c99c: error: missing argument to '-D'\n");
                    return false;
                }
                def = argv[++i];
            }
            vec_push(&opts->defines, (void *)def);
        } else if (arg[0] == '-') {
            fprintf(stderr, "c99c: error: unknown option '%s'\n", arg);
            return false;
        } else {
            if (opts->input_file) {
                fprintf(stderr, "c99c: error: multiple input files\n");
                return false;
            }
            opts->input_file = arg;
        }
    }

    if (!opts->input_file) {
        fprintf(stderr, "c99c: error: no input file\n");
        return false;
    }

    return true;
}

static char *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "c99c: error: cannot open '%s': ", path);
        perror(NULL);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc(size + 1);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "c99c: error: out of memory\n");
        return NULL;
    }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    *out_size = (size_t)size;
    return buf;
}

int main(int argc, char **argv) {
    Options opts;
    if (!parse_options(argc, argv, &opts)) {
        return 1;
    }

    /* Read source file */
    size_t src_size;
    char *source = read_file(opts.input_file, &src_size);
    if (!source) return 1;

    /* Initialize infrastructure */
    Arena arena;
    Strtab strtab;
    ErrorCtx err;

    arena_init(&arena);
    strtab_init(&strtab);
    error_ctx_init(&err);

    if (opts.warnings_as_errors)
        err.warnings_as_errors = true;

    /* ---- Stage 1: Preprocess ---- */
    Preprocessor pp;
    pp_init(&pp, &strtab, &err);

    for (size_t i = 0; i < opts.include_paths.size; i++) {
        pp_add_include_path(&pp, (const char *)vec_get(&opts.include_paths, i));
    }

    for (size_t i = 0; i < opts.defines.size; i++) {
        const char *def = (const char *)vec_get(&opts.defines, i);
        char name[256] = {0};
        const char *value = "1";
        const char *eq = strchr(def, '=');
        if (eq) {
            size_t nlen = eq - def;
            if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
            memcpy(name, def, nlen);
            value = eq + 1;
        } else {
            strncpy(name, def, sizeof(name) - 1);
        }
        pp_define(&pp, name, value);
    }

    char *preprocessed = pp_process(&pp, source, src_size, opts.input_file);

    if (opts.preprocess_only) {
        if (preprocessed) {
            printf("%s", preprocessed);
        }
        free(preprocessed);
        free(source);
        pp_free(&pp);
        strtab_free(&strtab);
        arena_free(&arena);
        vec_free(&opts.include_paths);
        vec_free(&opts.defines);
        return err.error_count > 0 ? 1 : 0;
    }

    /* Use preprocessed source for subsequent stages */
    const char *pp_src = preprocessed ? preprocessed : source;
    size_t pp_len = preprocessed ? strlen(preprocessed) : src_size;

    /* ---- Stage 2: Lex ---- */
    Lexer lex;
    lexer_init(&lex, pp_src, pp_len, opts.input_file, &strtab, &err);

    if (opts.dump_tokens) {
        Token tok;
        do {
            tok = lexer_next(&lex);
            token_print(&tok);
        } while (tok.kind != TOK_EOF);

        free(preprocessed);
        free(source);
        pp_free(&pp);
        strtab_free(&strtab);
        arena_free(&arena);
        vec_free(&opts.include_paths);
        vec_free(&opts.defines);
        return err.error_count > 0 ? 1 : 0;
    }

    /* ---- Stage 3: Parse ---- */
    Parser parser;
    parser_init(&parser, &lex, &arena, &strtab, &err);
    ASTNode *tu = parser_parse(&parser);

    if (err.error_count > 0) {
        fprintf(stderr, "c99c: %d error(s) generated.\n", err.error_count);
        free(preprocessed);
        free(source);
        pp_free(&pp);
        strtab_free(&strtab);
        arena_free(&arena);
        vec_free(&opts.include_paths);
        vec_free(&opts.defines);
        return 1;
    }

    if (opts.dump_ast) {
        ast_print(tu, 0);
        free(preprocessed);
        free(source);
        pp_free(&pp);
        strtab_free(&strtab);
        arena_free(&arena);
        vec_free(&opts.include_paths);
        vec_free(&opts.defines);
        return 0;
    }

    /* ---- Stage 4: Semantic analysis ---- */
    Sema sema;
    sema_init(&sema, &arena, &err);
    sema_check(&sema, tu);

    if (err.error_count > 0) {
        fprintf(stderr, "c99c: %d error(s) generated.\n", err.error_count);
        free(preprocessed);
        free(source);
        pp_free(&pp);
        strtab_free(&strtab);
        arena_free(&arena);
        vec_free(&opts.include_paths);
        vec_free(&opts.defines);
        return 1;
    }

    /* ---- Stage 5: IR generation ---- */
    IRGen irgen;
    ir_gen_init(&irgen, &arena, &sema.symtab, &err);
    IRModule *module = ir_gen_translate(&irgen, tu);

    if (opts.dump_ir) {
        ir_print_module(module);
        free(preprocessed);
        free(source);
        pp_free(&pp);
        strtab_free(&strtab);
        arena_free(&arena);
        vec_free(&opts.include_paths);
        vec_free(&opts.defines);
        return 0;
    }

    /* ---- Stage 6: Code generation ---- */
    const char *out_file = opts.output_file;
    if (!out_file) {
        /* Default: replace .c with .s */
        static char default_out[256];
        strncpy(default_out, opts.input_file, sizeof(default_out) - 3);
        char *dot = strrchr(default_out, '.');
        if (dot) strcpy(dot, ".s");
        else strcat(default_out, ".s");
        out_file = default_out;
    }

    FILE *out_fp = fopen(out_file, "w");
    if (!out_fp) {
        fprintf(stderr, "c99c: error: cannot open output file '%s': ", out_file);
        perror(NULL);
        free(preprocessed);
        free(source);
        pp_free(&pp);
        strtab_free(&strtab);
        arena_free(&arena);
        vec_free(&opts.include_paths);
        vec_free(&opts.defines);
        return 1;
    }

    CodegenX86 codegen;
    codegen_x86_init(&codegen, &arena, out_fp, &err);
    codegen_x86_emit(&codegen, module);
    fclose(out_fp);

    if (err.error_count > 0) {
        fprintf(stderr, "c99c: %d error(s) generated.\n", err.error_count);
    } else {
        fprintf(stderr, "c99c: compiled '%s' -> '%s'\n",
                opts.input_file, out_file);
        if (err.warning_count > 0) {
            fprintf(stderr, "c99c: %d warning(s) generated.\n",
                    err.warning_count);
        }
    }

    /* Cleanup */
    free(preprocessed);
    free(source);
    pp_free(&pp);
    strtab_free(&strtab);
    arena_free(&arena);
    vec_free(&opts.include_paths);
    vec_free(&opts.defines);

    return err.error_count > 0 ? 1 : 0;
}
