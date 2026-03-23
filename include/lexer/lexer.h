/*
 * c99lang - A C99 Compiler
 * Lexer (tokenizer)
 */
#ifndef C99_LEXER_H
#define C99_LEXER_H

#include <stdbool.h>
#include "lexer/token.h"
#include "util/strtab.h"
#include "util/error.h"

typedef struct Lexer {
    const char *src;          /* source buffer (null-terminated) */
    size_t src_len;
    size_t pos;               /* current read position */
    int line;
    int col;
    const char *filename;
    Strtab *strtab;
    ErrorCtx *err;
    Token current;            /* current token (peek) */
    bool has_peek;
} Lexer;

void  lexer_init(Lexer *lex, const char *src, size_t len,
                 const char *filename, Strtab *strtab, ErrorCtx *err);
Token lexer_next(Lexer *lex);
Token lexer_peek(Lexer *lex);
bool  lexer_match(Lexer *lex, TokenKind kind);
Token lexer_expect(Lexer *lex, TokenKind kind);

#endif /* C99_LEXER_H */
