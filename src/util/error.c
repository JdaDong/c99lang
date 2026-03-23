/*
 * c99lang - Error and diagnostic reporting
 */
#include "util/error.h"
#include <stdio.h>
#include <stdlib.h>

void error_ctx_init(ErrorCtx *ctx) {
    ctx->error_count = 0;
    ctx->warning_count = 0;
    ctx->max_errors = 20;
    ctx->warnings_as_errors = false;
}

static const char *diag_level_str(DiagLevel level) {
    switch (level) {
    case DIAG_NOTE:    return "note";
    case DIAG_WARNING: return "warning";
    case DIAG_ERROR:   return "error";
    case DIAG_FATAL:   return "fatal error";
    }
    return "unknown";
}

static const char *diag_level_color(DiagLevel level) {
    switch (level) {
    case DIAG_NOTE:    return "\033[36m";   /* cyan */
    case DIAG_WARNING: return "\033[35m";   /* magenta */
    case DIAG_ERROR:   return "\033[31m";   /* red */
    case DIAG_FATAL:   return "\033[1;31m"; /* bold red */
    }
    return "";
}

void diag_emit_v(ErrorCtx *ctx, DiagLevel level, SrcLoc loc,
                 const char *fmt, va_list ap) {
    if (ctx->warnings_as_errors && level == DIAG_WARNING) {
        level = DIAG_ERROR;
    }

    /* Location */
    if (loc.filename) {
        fprintf(stderr, "\033[1m%s:%d:%d: ", loc.filename, loc.line, loc.col);
    }

    /* Level */
    fprintf(stderr, "%s%s:\033[0m ", diag_level_color(level),
            diag_level_str(level));

    /* Message */
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    switch (level) {
    case DIAG_WARNING:
        ctx->warning_count++;
        break;
    case DIAG_ERROR:
        ctx->error_count++;
        if (ctx->max_errors > 0 && ctx->error_count >= ctx->max_errors) {
            fprintf(stderr, "c99c: too many errors, stopping.\n");
            exit(1);
        }
        break;
    case DIAG_FATAL:
        exit(1);
        break;
    case DIAG_NOTE:
        break;
    }
}

void diag_emit(ErrorCtx *ctx, DiagLevel level, SrcLoc loc,
               const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    diag_emit_v(ctx, level, loc, fmt, ap);
    va_end(ap);
}
