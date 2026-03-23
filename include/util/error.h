/*
 * c99lang - A C99 Compiler
 * Error and diagnostic reporting
 */
#ifndef C99_ERROR_H
#define C99_ERROR_H

#include <stdarg.h>
#include <stdbool.h>

/* Source location */
typedef struct SrcLoc {
    const char *filename;
    int line;
    int col;
} SrcLoc;

typedef enum DiagLevel {
    DIAG_NOTE,
    DIAG_WARNING,
    DIAG_ERROR,
    DIAG_FATAL,
} DiagLevel;

/* Global error state */
typedef struct ErrorCtx {
    int error_count;
    int warning_count;
    int max_errors;       /* 0 = unlimited */
    bool warnings_as_errors;
} ErrorCtx;

void error_ctx_init(ErrorCtx *ctx);

void diag_emit(ErrorCtx *ctx, DiagLevel level, SrcLoc loc,
               const char *fmt, ...);
void diag_emit_v(ErrorCtx *ctx, DiagLevel level, SrcLoc loc,
                 const char *fmt, va_list ap);

/* Convenience macros */
#define diag_note(ctx, loc, ...)    diag_emit(ctx, DIAG_NOTE,    loc, __VA_ARGS__)
#define diag_warn(ctx, loc, ...)    diag_emit(ctx, DIAG_WARNING, loc, __VA_ARGS__)
#define diag_error(ctx, loc, ...)   diag_emit(ctx, DIAG_ERROR,   loc, __VA_ARGS__)
#define diag_fatal(ctx, loc, ...)   diag_emit(ctx, DIAG_FATAL,   loc, __VA_ARGS__)

#endif /* C99_ERROR_H */
