/*
 * c99lang - Token utilities
 */
#include "lexer/token.h"
#include <stdio.h>

static const char *token_names[TOK_COUNT] = {
    [TOK_EOF]            = "EOF",
    [TOK_INVALID]        = "INVALID",
    [TOK_INT_LIT]        = "INT_LIT",
    [TOK_FLOAT_LIT]      = "FLOAT_LIT",
    [TOK_CHAR_LIT]       = "CHAR_LIT",
    [TOK_STRING_LIT]     = "STRING_LIT",
    [TOK_IDENT]          = "IDENT",

    /* Keywords */
    [TOK_KW_AUTO]        = "auto",
    [TOK_KW_BREAK]       = "break",
    [TOK_KW_CASE]        = "case",
    [TOK_KW_CHAR]        = "char",
    [TOK_KW_CONST]       = "const",
    [TOK_KW_CONTINUE]    = "continue",
    [TOK_KW_DEFAULT]     = "default",
    [TOK_KW_DO]          = "do",
    [TOK_KW_DOUBLE]      = "double",
    [TOK_KW_ELSE]        = "else",
    [TOK_KW_ENUM]        = "enum",
    [TOK_KW_EXTERN]      = "extern",
    [TOK_KW_FLOAT]       = "float",
    [TOK_KW_FOR]         = "for",
    [TOK_KW_GOTO]        = "goto",
    [TOK_KW_IF]          = "if",
    [TOK_KW_INLINE]      = "inline",
    [TOK_KW_INT]         = "int",
    [TOK_KW_LONG]        = "long",
    [TOK_KW_REGISTER]    = "register",
    [TOK_KW_RESTRICT]    = "restrict",
    [TOK_KW_RETURN]      = "return",
    [TOK_KW_SHORT]       = "short",
    [TOK_KW_SIGNED]      = "signed",
    [TOK_KW_SIZEOF]      = "sizeof",
    [TOK_KW_STATIC]      = "static",
    [TOK_KW_STRUCT]      = "struct",
    [TOK_KW_SWITCH]      = "switch",
    [TOK_KW_TYPEDEF]     = "typedef",
    [TOK_KW_UNION]       = "union",
    [TOK_KW_UNSIGNED]    = "unsigned",
    [TOK_KW_VOID]        = "void",
    [TOK_KW_VOLATILE]    = "volatile",
    [TOK_KW_WHILE]       = "while",
    [TOK_KW__BOOL]       = "_Bool",
    [TOK_KW__COMPLEX]    = "_Complex",
    [TOK_KW__IMAGINARY]  = "_Imaginary",

    /* Punctuators */
    [TOK_LPAREN]         = "(",
    [TOK_RPAREN]         = ")",
    [TOK_LBRACKET]       = "[",
    [TOK_RBRACKET]       = "]",
    [TOK_LBRACE]         = "{",
    [TOK_RBRACE]         = "}",
    [TOK_DOT]            = ".",
    [TOK_ARROW]          = "->",
    [TOK_PLUSPLUS]        = "++",
    [TOK_MINUSMINUS]     = "--",
    [TOK_AMP]            = "&",
    [TOK_STAR]           = "*",
    [TOK_PLUS]           = "+",
    [TOK_MINUS]          = "-",
    [TOK_TILDE]          = "~",
    [TOK_BANG]           = "!",
    [TOK_SLASH]          = "/",
    [TOK_PERCENT]        = "%",
    [TOK_LSHIFT]         = "<<",
    [TOK_RSHIFT]         = ">>",
    [TOK_LT]            = "<",
    [TOK_GT]            = ">",
    [TOK_LE]            = "<=",
    [TOK_GE]            = ">=",
    [TOK_EQ]            = "==",
    [TOK_NE]            = "!=",
    [TOK_CARET]          = "^",
    [TOK_PIPE]           = "|",
    [TOK_AMPAMP]         = "&&",
    [TOK_PIPEPIPE]       = "||",
    [TOK_QUESTION]       = "?",
    [TOK_COLON]          = ":",
    [TOK_SEMICOLON]      = ";",
    [TOK_ELLIPSIS]       = "...",
    [TOK_ASSIGN]         = "=",
    [TOK_STAR_ASSIGN]    = "*=",
    [TOK_SLASH_ASSIGN]   = "/=",
    [TOK_PERCENT_ASSIGN] = "%=",
    [TOK_PLUS_ASSIGN]    = "+=",
    [TOK_MINUS_ASSIGN]   = "-=",
    [TOK_LSHIFT_ASSIGN]  = "<<=",
    [TOK_RSHIFT_ASSIGN]  = ">>=",
    [TOK_AMP_ASSIGN]     = "&=",
    [TOK_CARET_ASSIGN]   = "^=",
    [TOK_PIPE_ASSIGN]    = "|=",
    [TOK_COMMA]          = ",",
    [TOK_HASH]           = "#",
    [TOK_HASHHASH]       = "##",
};

const char *token_kind_str(TokenKind kind) {
    if (kind >= 0 && kind < TOK_COUNT && token_names[kind]) {
        return token_names[kind];
    }
    return "???";
}

void token_print(const Token *tok) {
    printf("[%s:%d:%d] %s",
           tok->loc.filename ? tok->loc.filename : "<unknown>",
           tok->loc.line, tok->loc.col,
           token_kind_str(tok->kind));

    switch (tok->kind) {
    case TOK_IDENT:
    case TOK_STRING_LIT:
        printf(" \"%.*s\"", (int)tok->text_len, tok->text);
        break;
    case TOK_INT_LIT:
        printf(" %llu", tok->int_lit.value);
        break;
    case TOK_FLOAT_LIT:
        printf(" %Lg", tok->float_lit.value);
        break;
    case TOK_CHAR_LIT:
        printf(" '%.*s'", (int)tok->text_len, tok->text);
        break;
    default:
        break;
    }
    printf("\n");
}
