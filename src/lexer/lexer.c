/*
 * c99lang - Lexer (Tokenizer)
 * Handles all C99 token types including:
 *  - Identifiers and keywords
 *  - Integer/float literals (decimal, hex, octal, C99 hex-float)
 *  - Character and string literals with escape sequences
 *  - All C99 operators and punctuators
 */
#include "lexer/lexer.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ---- Keyword table ---- */
typedef struct {
    const char *name;
    TokenKind kind;
} KeywordEntry;

static const KeywordEntry keywords[] = {
    {"auto",       TOK_KW_AUTO},
    {"break",      TOK_KW_BREAK},
    {"case",       TOK_KW_CASE},
    {"char",       TOK_KW_CHAR},
    {"const",      TOK_KW_CONST},
    {"continue",   TOK_KW_CONTINUE},
    {"default",    TOK_KW_DEFAULT},
    {"do",         TOK_KW_DO},
    {"double",     TOK_KW_DOUBLE},
    {"else",       TOK_KW_ELSE},
    {"enum",       TOK_KW_ENUM},
    {"extern",     TOK_KW_EXTERN},
    {"float",      TOK_KW_FLOAT},
    {"for",        TOK_KW_FOR},
    {"goto",       TOK_KW_GOTO},
    {"if",         TOK_KW_IF},
    {"inline",     TOK_KW_INLINE},
    {"int",        TOK_KW_INT},
    {"long",       TOK_KW_LONG},
    {"register",   TOK_KW_REGISTER},
    {"restrict",   TOK_KW_RESTRICT},
    {"return",     TOK_KW_RETURN},
    {"short",      TOK_KW_SHORT},
    {"signed",     TOK_KW_SIGNED},
    {"sizeof",     TOK_KW_SIZEOF},
    {"static",     TOK_KW_STATIC},
    {"struct",     TOK_KW_STRUCT},
    {"switch",     TOK_KW_SWITCH},
    {"typedef",    TOK_KW_TYPEDEF},
    {"union",      TOK_KW_UNION},
    {"unsigned",   TOK_KW_UNSIGNED},
    {"void",       TOK_KW_VOID},
    {"volatile",   TOK_KW_VOLATILE},
    {"while",      TOK_KW_WHILE},
    {"_Bool",      TOK_KW__BOOL},
    {"_Complex",   TOK_KW__COMPLEX},
    {"_Imaginary", TOK_KW__IMAGINARY},
    {NULL, 0}
};

static TokenKind lookup_keyword(const char *text, size_t len) {
    for (const KeywordEntry *e = keywords; e->name; e++) {
        if (strlen(e->name) == len && memcmp(e->name, text, len) == 0) {
            return e->kind;
        }
    }
    return TOK_IDENT;
}

/* ---- Lexer helpers ---- */

void lexer_init(Lexer *lex, const char *src, size_t len,
                const char *filename, Strtab *strtab, ErrorCtx *err) {
    lex->src = src;
    lex->src_len = len;
    lex->pos = 0;
    lex->line = 1;
    lex->col = 1;
    lex->filename = filename;
    lex->strtab = strtab;
    lex->err = err;
    lex->has_peek = false;
    memset(&lex->current, 0, sizeof(Token));
}

static inline int lex_ch(Lexer *lex) {
    if (lex->pos >= lex->src_len) return '\0';
    return (unsigned char)lex->src[lex->pos];
}

static inline int lex_peek_ch(Lexer *lex, int offset) {
    size_t p = lex->pos + offset;
    if (p >= lex->src_len) return '\0';
    return (unsigned char)lex->src[p];
}

static inline void lex_advance(Lexer *lex) {
    if (lex->pos < lex->src_len) {
        if (lex->src[lex->pos] == '\n') {
            lex->line++;
            lex->col = 1;
        } else {
            lex->col++;
        }
        lex->pos++;
    }
}

static SrcLoc lex_loc(Lexer *lex) {
    return (SrcLoc){ lex->filename, lex->line, lex->col };
}

static Token make_token(TokenKind kind, SrcLoc loc, const char *text,
                        size_t text_len) {
    Token t;
    memset(&t, 0, sizeof(t));
    t.kind = kind;
    t.loc = loc;
    t.text = text;
    t.text_len = text_len;
    return t;
}

/* Skip whitespace and comments */
static void skip_whitespace_and_comments(Lexer *lex) {
    while (lex->pos < lex->src_len) {
        int c = lex_ch(lex);

        /* Whitespace */
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
            c == '\f' || c == '\v') {
            lex_advance(lex);
            continue;
        }

        /* Line comment */
        if (c == '/' && lex_peek_ch(lex, 1) == '/') {
            lex_advance(lex); lex_advance(lex);
            while (lex->pos < lex->src_len && lex_ch(lex) != '\n') {
                lex_advance(lex);
            }
            continue;
        }

        /* Block comment */
        if (c == '/' && lex_peek_ch(lex, 1) == '*') {
            SrcLoc comment_start = lex_loc(lex);
            lex_advance(lex); lex_advance(lex);
            while (lex->pos < lex->src_len) {
                if (lex_ch(lex) == '*' && lex_peek_ch(lex, 1) == '/') {
                    lex_advance(lex); lex_advance(lex);
                    goto next_ws;
                }
                lex_advance(lex);
            }
            diag_error(lex->err, comment_start, "unterminated block comment");
            next_ws:
            continue;
        }

        /* Backslash-newline (line continuation) */
        if (c == '\\' && lex_peek_ch(lex, 1) == '\n') {
            lex_advance(lex); lex_advance(lex);
            continue;
        }

        break;
    }
}

/* ---- Number literal parsing ---- */

static Token lex_number(Lexer *lex) {
    SrcLoc loc = lex_loc(lex);
    size_t start = lex->pos;
    bool is_float = false;
    bool is_hex = false;

    /* Hex prefix */
    if (lex_ch(lex) == '0' && (lex_peek_ch(lex, 1) == 'x' || lex_peek_ch(lex, 1) == 'X')) {
        is_hex = true;
        lex_advance(lex); lex_advance(lex);
        while (isxdigit(lex_ch(lex))) lex_advance(lex);

        /* C99 hex float: 0x1.0p10 */
        if (lex_ch(lex) == '.') {
            is_float = true;
            lex_advance(lex);
            while (isxdigit(lex_ch(lex))) lex_advance(lex);
        }
        if (lex_ch(lex) == 'p' || lex_ch(lex) == 'P') {
            is_float = true;
            lex_advance(lex);
            if (lex_ch(lex) == '+' || lex_ch(lex) == '-') lex_advance(lex);
            while (isdigit(lex_ch(lex))) lex_advance(lex);
        }
    }
    /* Octal or decimal */
    else {
        while (isdigit(lex_ch(lex))) lex_advance(lex);

        if (lex_ch(lex) == '.') {
            is_float = true;
            lex_advance(lex);
            while (isdigit(lex_ch(lex))) lex_advance(lex);
        }
        if (lex_ch(lex) == 'e' || lex_ch(lex) == 'E') {
            is_float = true;
            lex_advance(lex);
            if (lex_ch(lex) == '+' || lex_ch(lex) == '-') lex_advance(lex);
            while (isdigit(lex_ch(lex))) lex_advance(lex);
        }
    }

    if (is_float) {
        /* Float suffix */
        FloatSuffix fs = FLOAT_SUFFIX_NONE;
        if (lex_ch(lex) == 'f' || lex_ch(lex) == 'F') {
            fs = FLOAT_SUFFIX_F; lex_advance(lex);
        } else if (lex_ch(lex) == 'l' || lex_ch(lex) == 'L') {
            fs = FLOAT_SUFFIX_L; lex_advance(lex);
        }
        size_t len = lex->pos - start;
        const char *text = strtab_intern(lex->strtab, lex->src + start, len);
        Token t = make_token(TOK_FLOAT_LIT, loc, text, len);
        t.float_lit.value = strtold(lex->src + start, NULL);
        t.float_lit.suffix = fs;
        return t;
    } else {
        /* Integer suffix */
        IntSuffix isuf = INT_SUFFIX_NONE;
        while (1) {
            int c = lex_ch(lex);
            if ((c == 'u' || c == 'U') && !(isuf & INT_SUFFIX_U)) {
                isuf = (IntSuffix)(isuf | INT_SUFFIX_U); lex_advance(lex);
            } else if ((c == 'l' || c == 'L') && !(isuf & INT_SUFFIX_L)) {
                lex_advance(lex);
                if ((lex_ch(lex) == 'l' || lex_ch(lex) == 'L')) {
                    isuf = (IntSuffix)(isuf | INT_SUFFIX_LL); lex_advance(lex);
                } else {
                    isuf = (IntSuffix)(isuf | INT_SUFFIX_L);
                }
            } else {
                break;
            }
        }
        size_t len = lex->pos - start;
        const char *text = strtab_intern(lex->strtab, lex->src + start, len);
        Token t = make_token(TOK_INT_LIT, loc, text, len);
        /* Parse value */
        if (is_hex) {
            t.int_lit.value = strtoull(lex->src + start, NULL, 16);
        } else if (lex->src[start] == '0' && len > 1) {
            t.int_lit.value = strtoull(lex->src + start, NULL, 8);
        } else {
            t.int_lit.value = strtoull(lex->src + start, NULL, 10);
        }
        t.int_lit.suffix = isuf;
        return t;
    }
}

/* ---- Character and string literal parsing ---- */

static int lex_escape_char(Lexer *lex) {
    lex_advance(lex); /* skip backslash */
    int c = lex_ch(lex);
    lex_advance(lex);
    switch (c) {
    case 'a':  return '\a';
    case 'b':  return '\b';
    case 'f':  return '\f';
    case 'n':  return '\n';
    case 'r':  return '\r';
    case 't':  return '\t';
    case 'v':  return '\v';
    case '\\': return '\\';
    case '\'': return '\'';
    case '\"': return '\"';
    case '?':  return '?';
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7': {
        /* Octal escape */
        int val = c - '0';
        for (int i = 0; i < 2 && lex_ch(lex) >= '0' && lex_ch(lex) <= '7'; i++) {
            val = val * 8 + (lex_ch(lex) - '0');
            lex_advance(lex);
        }
        return val;
    }
    case 'x': {
        /* Hex escape */
        int val = 0;
        while (isxdigit(lex_ch(lex))) {
            int d = lex_ch(lex);
            val = val * 16 + (isdigit(d) ? d - '0' : (tolower(d) - 'a' + 10));
            lex_advance(lex);
        }
        return val;
    }
    default:
        return c;
    }
}

static Token lex_char_literal(Lexer *lex) {
    SrcLoc loc = lex_loc(lex);
    size_t start = lex->pos;
    lex_advance(lex); /* skip opening quote */

    int value = 0;
    if (lex_ch(lex) == '\\') {
        value = lex_escape_char(lex);
    } else {
        value = lex_ch(lex);
        lex_advance(lex);
    }

    if (lex_ch(lex) != '\'') {
        diag_error(lex->err, loc, "unterminated character literal");
    } else {
        lex_advance(lex);
    }

    size_t len = lex->pos - start;
    const char *text = strtab_intern(lex->strtab, lex->src + start, len);
    Token t = make_token(TOK_CHAR_LIT, loc, text, len);
    t.str_val = text;
    (void)value;
    return t;
}

static Token lex_string_literal(Lexer *lex) {
    SrcLoc loc = lex_loc(lex);
    size_t start = lex->pos;
    lex_advance(lex); /* skip opening quote */

    while (lex->pos < lex->src_len && lex_ch(lex) != '"') {
        if (lex_ch(lex) == '\\') {
            lex_advance(lex); /* skip backslash */
            if (lex->pos < lex->src_len) lex_advance(lex); /* skip escaped char */
        } else if (lex_ch(lex) == '\n') {
            diag_error(lex->err, loc, "unterminated string literal");
            break;
        } else {
            lex_advance(lex);
        }
    }

    if (lex_ch(lex) == '"') {
        lex_advance(lex);
    } else {
        diag_error(lex->err, loc, "unterminated string literal");
    }

    size_t len = lex->pos - start;
    const char *text = strtab_intern(lex->strtab, lex->src + start, len);
    Token t = make_token(TOK_STRING_LIT, loc, text, len);
    t.str_val = text;
    return t;
}

/* ---- Main lexer ---- */

Token lexer_next(Lexer *lex) {
    if (lex->has_peek) {
        lex->has_peek = false;
        return lex->current;
    }

    skip_whitespace_and_comments(lex);

    if (lex->pos >= lex->src_len) {
        return make_token(TOK_EOF, lex_loc(lex), NULL, 0);
    }

    SrcLoc loc = lex_loc(lex);
    int c = lex_ch(lex);

    /* Identifier or keyword */
    if (isalpha(c) || c == '_') {
        size_t start = lex->pos;
        while (isalnum(lex_ch(lex)) || lex_ch(lex) == '_') {
            lex_advance(lex);
        }
        size_t len = lex->pos - start;
        const char *text = strtab_intern(lex->strtab, lex->src + start, len);
        TokenKind kind = lookup_keyword(text, len);
        return make_token(kind, loc, text, len);
    }

    /* Number literal */
    if (isdigit(c) || (c == '.' && isdigit(lex_peek_ch(lex, 1)))) {
        return lex_number(lex);
    }

    /* Character literal */
    if (c == '\'') {
        return lex_char_literal(lex);
    }

    /* String literal */
    if (c == '"') {
        return lex_string_literal(lex);
    }

    /* Punctuators / operators */
    lex_advance(lex);

    switch (c) {
    case '(': return make_token(TOK_LPAREN, loc, "(", 1);
    case ')': return make_token(TOK_RPAREN, loc, ")", 1);
    case '[': return make_token(TOK_LBRACKET, loc, "[", 1);
    case ']': return make_token(TOK_RBRACKET, loc, "]", 1);
    case '{': return make_token(TOK_LBRACE, loc, "{", 1);
    case '}': return make_token(TOK_RBRACE, loc, "}", 1);
    case '~': return make_token(TOK_TILDE, loc, "~", 1);
    case '?': return make_token(TOK_QUESTION, loc, "?", 1);
    case ':': return make_token(TOK_COLON, loc, ":", 1);
    case ';': return make_token(TOK_SEMICOLON, loc, ";", 1);
    case ',': return make_token(TOK_COMMA, loc, ",", 1);

    case '#':
        if (lex_ch(lex) == '#') {
            lex_advance(lex);
            return make_token(TOK_HASHHASH, loc, "##", 2);
        }
        return make_token(TOK_HASH, loc, "#", 1);

    case '.':
        if (lex_ch(lex) == '.' && lex_peek_ch(lex, 1) == '.') {
            lex_advance(lex); lex_advance(lex);
            return make_token(TOK_ELLIPSIS, loc, "...", 3);
        }
        return make_token(TOK_DOT, loc, ".", 1);

    case '+':
        if (lex_ch(lex) == '+') { lex_advance(lex); return make_token(TOK_PLUSPLUS, loc, "++", 2); }
        if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_PLUS_ASSIGN, loc, "+=", 2); }
        return make_token(TOK_PLUS, loc, "+", 1);

    case '-':
        if (lex_ch(lex) == '-') { lex_advance(lex); return make_token(TOK_MINUSMINUS, loc, "--", 2); }
        if (lex_ch(lex) == '>') { lex_advance(lex); return make_token(TOK_ARROW, loc, "->", 2); }
        if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_MINUS_ASSIGN, loc, "-=", 2); }
        return make_token(TOK_MINUS, loc, "-", 1);

    case '*':
        if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_STAR_ASSIGN, loc, "*=", 2); }
        return make_token(TOK_STAR, loc, "*", 1);

    case '/':
        if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_SLASH_ASSIGN, loc, "/=", 2); }
        return make_token(TOK_SLASH, loc, "/", 1);

    case '%':
        if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_PERCENT_ASSIGN, loc, "%=", 2); }
        return make_token(TOK_PERCENT, loc, "%", 1);

    case '&':
        if (lex_ch(lex) == '&') { lex_advance(lex); return make_token(TOK_AMPAMP, loc, "&&", 2); }
        if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_AMP_ASSIGN, loc, "&=", 2); }
        return make_token(TOK_AMP, loc, "&", 1);

    case '|':
        if (lex_ch(lex) == '|') { lex_advance(lex); return make_token(TOK_PIPEPIPE, loc, "||", 2); }
        if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_PIPE_ASSIGN, loc, "|=", 2); }
        return make_token(TOK_PIPE, loc, "|", 1);

    case '^':
        if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_CARET_ASSIGN, loc, "^=", 2); }
        return make_token(TOK_CARET, loc, "^", 1);

    case '!':
        if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_NE, loc, "!=", 2); }
        return make_token(TOK_BANG, loc, "!", 1);

    case '=':
        if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_EQ, loc, "==", 2); }
        return make_token(TOK_ASSIGN, loc, "=", 1);

    case '<':
        if (lex_ch(lex) == '<') {
            lex_advance(lex);
            if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_LSHIFT_ASSIGN, loc, "<<=", 3); }
            return make_token(TOK_LSHIFT, loc, "<<", 2);
        }
        if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_LE, loc, "<=", 2); }
        return make_token(TOK_LT, loc, "<", 1);

    case '>':
        if (lex_ch(lex) == '>') {
            lex_advance(lex);
            if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_RSHIFT_ASSIGN, loc, ">>=", 3); }
            return make_token(TOK_RSHIFT, loc, ">>", 2);
        }
        if (lex_ch(lex) == '=') { lex_advance(lex); return make_token(TOK_GE, loc, ">=", 2); }
        return make_token(TOK_GT, loc, ">", 1);

    default:
        diag_error(lex->err, loc, "unexpected character '%c' (0x%02x)", c, c);
        return make_token(TOK_INVALID, loc, NULL, 0);
    }
}

Token lexer_peek(Lexer *lex) {
    if (!lex->has_peek) {
        lex->current = lexer_next(lex);
        lex->has_peek = true;
    }
    return lex->current;
}

bool lexer_match(Lexer *lex, TokenKind kind) {
    if (lexer_peek(lex).kind == kind) {
        lexer_next(lex);
        return true;
    }
    return false;
}

Token lexer_expect(Lexer *lex, TokenKind kind) {
    Token t = lexer_next(lex);
    if (t.kind != kind) {
        diag_error(lex->err, t.loc, "expected '%s', got '%s'",
                   token_kind_str(kind), token_kind_str(t.kind));
    }
    return t;
}
