/*
 * c99lang - A C99 Compiler
 * Parser (recursive descent)
 */
#ifndef C99_PARSER_H
#define C99_PARSER_H

#include "lexer/lexer.h"
#include "parser/ast.h"
#include "util/arena.h"
#include "util/strtab.h"

typedef struct Parser {
    Lexer *lex;
    Arena *arena;
    Strtab *strtab;
    ErrorCtx *err;
    Token tok;               /* current token */
    Vector typedef_names;    /* const char* - names defined with typedef */
} Parser;

void     parser_init(Parser *p, Lexer *lex, Arena *arena,
                     Strtab *strtab, ErrorCtx *err);
ASTNode *parser_parse(Parser *p);   /* returns AST_TRANSLATION_UNIT */

/* Internal entry points (called by parser.c, implemented in separate files) */
ASTNode *parse_declaration(Parser *p);
ASTNode *parse_function_def(Parser *p);
ASTNode *parse_statement(Parser *p);
ASTNode *parse_compound_statement(Parser *p);
ASTNode *parse_expression(Parser *p);
ASTNode *parse_assignment_expression(Parser *p);
ASTNode *parse_constant_expression(Parser *p);
ASTNode *parse_conditional_expression(Parser *p);

/* Declaration parsing helpers */
CType   *parse_type_specifier(Parser *p, StorageClass *sc, TypeQual *tq,
                               FuncSpec *fspec);
CType   *parse_declarator(Parser *p, CType *base, const char **name);
CType   *parse_abstract_declarator(Parser *p, CType *base);
Vector   parse_parameter_list(Parser *p, bool *is_variadic);

/* Helpers */
bool     parser_is_typename(Parser *p);

#endif /* C99_PARSER_H */
