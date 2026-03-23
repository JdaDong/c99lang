/*
 * c99lang - A C99 Compiler
 * Token definitions for the C99 language
 */
#ifndef C99_TOKEN_H
#define C99_TOKEN_H

#include <stddef.h>
#include "util/error.h"

/* ---------- Token kinds ---------- */
typedef enum TokenKind {
    /* Special */
    TOK_EOF = 0,
    TOK_INVALID,

    /* Literals */
    TOK_INT_LIT,        /* 42, 0x2A, 077, 0b101 */
    TOK_FLOAT_LIT,      /* 3.14, 1e10, 0x1.0p10 */
    TOK_CHAR_LIT,       /* 'a', '\n' */
    TOK_STRING_LIT,     /* "hello" */

    /* Identifier */
    TOK_IDENT,

    /* ---- C99 Keywords ---- */
    TOK_KW_AUTO,
    TOK_KW_BREAK,
    TOK_KW_CASE,
    TOK_KW_CHAR,
    TOK_KW_CONST,
    TOK_KW_CONTINUE,
    TOK_KW_DEFAULT,
    TOK_KW_DO,
    TOK_KW_DOUBLE,
    TOK_KW_ELSE,
    TOK_KW_ENUM,
    TOK_KW_EXTERN,
    TOK_KW_FLOAT,
    TOK_KW_FOR,
    TOK_KW_GOTO,
    TOK_KW_IF,
    TOK_KW_INLINE,       /* C99 */
    TOK_KW_INT,
    TOK_KW_LONG,
    TOK_KW_REGISTER,
    TOK_KW_RESTRICT,     /* C99 */
    TOK_KW_RETURN,
    TOK_KW_SHORT,
    TOK_KW_SIGNED,
    TOK_KW_SIZEOF,
    TOK_KW_STATIC,
    TOK_KW_STRUCT,
    TOK_KW_SWITCH,
    TOK_KW_TYPEDEF,
    TOK_KW_UNION,
    TOK_KW_UNSIGNED,
    TOK_KW_VOID,
    TOK_KW_VOLATILE,
    TOK_KW_WHILE,
    TOK_KW__BOOL,        /* C99 _Bool */
    TOK_KW__COMPLEX,     /* C99 _Complex */
    TOK_KW__IMAGINARY,   /* C99 _Imaginary */

    /* ---- Punctuators / Operators ---- */
    TOK_LPAREN,          /* ( */
    TOK_RPAREN,          /* ) */
    TOK_LBRACKET,        /* [ */
    TOK_RBRACKET,        /* ] */
    TOK_LBRACE,          /* { */
    TOK_RBRACE,          /* } */
    TOK_DOT,             /* . */
    TOK_ARROW,           /* -> */
    TOK_PLUSPLUS,         /* ++ */
    TOK_MINUSMINUS,      /* -- */
    TOK_AMP,             /* & */
    TOK_STAR,            /* * */
    TOK_PLUS,            /* + */
    TOK_MINUS,           /* - */
    TOK_TILDE,           /* ~ */
    TOK_BANG,            /* ! */
    TOK_SLASH,           /* / */
    TOK_PERCENT,         /* % */
    TOK_LSHIFT,          /* << */
    TOK_RSHIFT,          /* >> */
    TOK_LT,             /* < */
    TOK_GT,             /* > */
    TOK_LE,             /* <= */
    TOK_GE,             /* >= */
    TOK_EQ,             /* == */
    TOK_NE,             /* != */
    TOK_CARET,           /* ^ */
    TOK_PIPE,            /* | */
    TOK_AMPAMP,          /* && */
    TOK_PIPEPIPE,        /* || */
    TOK_QUESTION,        /* ? */
    TOK_COLON,           /* : */
    TOK_SEMICOLON,       /* ; */
    TOK_ELLIPSIS,        /* ... */
    TOK_ASSIGN,          /* = */
    TOK_STAR_ASSIGN,     /* *= */
    TOK_SLASH_ASSIGN,    /* /= */
    TOK_PERCENT_ASSIGN,  /* %= */
    TOK_PLUS_ASSIGN,     /* += */
    TOK_MINUS_ASSIGN,    /* -= */
    TOK_LSHIFT_ASSIGN,   /* <<= */
    TOK_RSHIFT_ASSIGN,   /* >>= */
    TOK_AMP_ASSIGN,      /* &= */
    TOK_CARET_ASSIGN,    /* ^= */
    TOK_PIPE_ASSIGN,     /* |= */
    TOK_COMMA,           /* , */
    TOK_HASH,            /* # (preprocessor) */
    TOK_HASHHASH,        /* ## (token paste) */

    TOK_COUNT            /* sentinel: total number of token kinds */
} TokenKind;

/* Integer literal suffix flags */
typedef enum IntSuffix {
    INT_SUFFIX_NONE  = 0,
    INT_SUFFIX_U     = 1,     /* unsigned */
    INT_SUFFIX_L     = 2,     /* long */
    INT_SUFFIX_LL    = 4,     /* long long */
    INT_SUFFIX_UL    = 3,
    INT_SUFFIX_ULL   = 5,
} IntSuffix;

/* Float literal suffix */
typedef enum FloatSuffix {
    FLOAT_SUFFIX_NONE = 0,    /* double */
    FLOAT_SUFFIX_F,           /* float */
    FLOAT_SUFFIX_L,           /* long double */
} FloatSuffix;

/* ---------- Token ---------- */
typedef struct Token {
    TokenKind kind;
    SrcLoc loc;
    const char *text;         /* interned string */
    size_t text_len;

    union {
        struct {
            unsigned long long value;
            IntSuffix suffix;
        } int_lit;
        struct {
            long double value;
            FloatSuffix suffix;
        } float_lit;
        const char *str_val;   /* for string / char literals (unescaped) */
    };
} Token;

/* Utilities */
const char *token_kind_str(TokenKind kind);
void        token_print(const Token *tok);

#endif /* C99_TOKEN_H */
