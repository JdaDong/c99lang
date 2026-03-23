/*
 * c99lang - AST node construction
 */
#include "parser/ast.h"
#include <string.h>

ASTNode *ast_new(Arena *a, ASTKind kind, SrcLoc loc) {
    ASTNode *n = (ASTNode *)arena_calloc(a, sizeof(ASTNode));
    n->kind = kind;
    n->loc = loc;
    n->type = NULL;
    return n;
}
