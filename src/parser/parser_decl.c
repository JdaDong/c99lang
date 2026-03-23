/*
 * c99lang - Parser: Declaration parsing
 * Handles variable declarations, function definitions, struct/union/enum,
 * typedef, etc.
 */
#include "parser/parser.h"
#include <string.h>

/* Forward declarations of helpers from parser.c */
static void advance(Parser *p) { p->tok = lexer_next(p->lex); }

static bool check(Parser *p, TokenKind kind) {
    return p->tok.kind == kind;
}

static bool match(Parser *p, TokenKind kind) {
    if (p->tok.kind == kind) { advance(p); return true; }
    return false;
}

static Token expect(Parser *p, TokenKind kind) {
    Token t = p->tok;
    if (t.kind != kind) {
        diag_error(p->err, t.loc, "expected '%s', got '%s'",
                   token_kind_str(kind), token_kind_str(t.kind));
    }
    advance(p);
    return t;
}

/* ---- Type specifier parsing ---- */

static CType *parse_struct_or_union(Parser *p, bool is_struct) {
    SrcLoc loc = p->tok.loc;
    advance(p); /* skip struct/union keyword */

    const char *tag = NULL;
    if (check(p, TOK_IDENT)) {
        tag = p->tok.text;
        advance(p);
    }

    CType *ty = type_new(p->arena, is_struct ? TYPE_STRUCT : TYPE_UNION);
    ty->record.tag = tag;
    ty->record.fields = NULL;
    ty->record.field_count = 0;
    ty->record.is_complete = false;

    if (match(p, TOK_LBRACE)) {
        /* Parse fields */
        Vector fields;
        vec_init(&fields);

        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            /* Parse field type */
            StorageClass sc = SC_NONE;
            TypeQual tq = TQ_NONE;
            FuncSpec fs = FS_NONE;
            CType *base = parse_type_specifier(p, &sc, &tq, &fs);
            if (!base) {
                diag_error(p->err, p->tok.loc, "expected type specifier in struct/union");
                advance(p);
                continue;
            }

            /* Parse field declarators */
            do {
                const char *name = NULL;
                CType *field_type = parse_declarator(p, base, &name);

                CTypeField *f = (CTypeField *)arena_alloc(p->arena, sizeof(CTypeField));
                f->name = name;
                f->type = field_type;
                f->bit_width = -1;
                f->offset = 0;

                /* Bitfield */
                if (match(p, TOK_COLON)) {
                    ASTNode *bw = parse_constant_expression(p);
                    if (bw && bw->kind == AST_INT_LIT) {
                        f->bit_width = (int)bw->int_lit.value;
                    }
                }

                vec_push(&fields, f);
            } while (match(p, TOK_COMMA));

            expect(p, TOK_SEMICOLON);
        }
        expect(p, TOK_RBRACE);

        ty->record.field_count = (int)fields.size;
        ty->record.fields = (CTypeField *)arena_alloc(p->arena,
            sizeof(CTypeField) * fields.size);
        for (size_t i = 0; i < fields.size; i++) {
            ty->record.fields[i] = *(CTypeField *)vec_get(&fields, i);
        }
        ty->record.is_complete = true;
        vec_free(&fields);
    }

    return ty;
}

static CType *parse_enum(Parser *p) {
    SrcLoc loc = p->tok.loc;
    advance(p); /* skip enum keyword */

    const char *tag = NULL;
    if (check(p, TOK_IDENT)) {
        tag = p->tok.text;
        advance(p);
    }

    CType *ty = type_new(p->arena, TYPE_ENUM);
    ty->enumeration.tag = tag;
    ty->enumeration.is_complete = false;

    if (match(p, TOK_LBRACE)) {
        /* Parse enum constants */
        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            /* enum constant name */
            if (!check(p, TOK_IDENT)) {
                diag_error(p->err, p->tok.loc, "expected identifier in enum");
                advance(p);
                continue;
            }
            advance(p); /* skip name (handled by sema) */

            /* Optional = value */
            if (match(p, TOK_ASSIGN)) {
                parse_constant_expression(p);
            }

            if (!match(p, TOK_COMMA)) break;
        }
        expect(p, TOK_RBRACE);
        ty->enumeration.is_complete = true;
    }

    return ty;
}

CType *parse_type_specifier(Parser *p, StorageClass *sc, TypeQual *tq,
                             FuncSpec *fspec) {
    *sc = SC_NONE;
    *tq = TQ_NONE;
    *fspec = FS_NONE;

    bool seen_type = false;
    bool is_signed = true;
    bool seen_signed = false;
    bool seen_unsigned = false;
    int long_count = 0;
    int short_count = 0;
    CTypeKind base_kind = TYPE_INT;
    CType *custom_type = NULL;

    while (true) {
        TokenKind k = p->tok.kind;

        /* Storage class */
        if (k == TOK_KW_TYPEDEF)  { *sc = SC_TYPEDEF;  advance(p); continue; }
        if (k == TOK_KW_EXTERN)   { *sc = SC_EXTERN;   advance(p); continue; }
        if (k == TOK_KW_STATIC)   { *sc = SC_STATIC;   advance(p); continue; }
        if (k == TOK_KW_AUTO)     { *sc = SC_AUTO;     advance(p); continue; }
        if (k == TOK_KW_REGISTER) { *sc = SC_REGISTER; advance(p); continue; }

        /* Type qualifiers */
        if (k == TOK_KW_CONST)    { *tq = (TypeQual)(*tq | TQ_CONST);    advance(p); continue; }
        if (k == TOK_KW_VOLATILE) { *tq = (TypeQual)(*tq | TQ_VOLATILE); advance(p); continue; }
        if (k == TOK_KW_RESTRICT) { *tq = (TypeQual)(*tq | TQ_RESTRICT); advance(p); continue; }

        /* Function specifiers */
        if (k == TOK_KW_INLINE)   { *fspec = FS_INLINE; advance(p); continue; }

        /* Type specifiers */
        if (k == TOK_KW_VOID)     { base_kind = TYPE_VOID;   seen_type = true; advance(p); continue; }
        if (k == TOK_KW_CHAR)     { base_kind = TYPE_CHAR;   seen_type = true; advance(p); continue; }
        if (k == TOK_KW_INT)      { base_kind = TYPE_INT;    seen_type = true; advance(p); continue; }
        if (k == TOK_KW_FLOAT)    { base_kind = TYPE_FLOAT;  seen_type = true; advance(p); continue; }
        if (k == TOK_KW_DOUBLE)   { base_kind = TYPE_DOUBLE; seen_type = true; advance(p); continue; }
        if (k == TOK_KW__BOOL)    { base_kind = TYPE_BOOL;   seen_type = true; advance(p); continue; }

        if (k == TOK_KW_SHORT)    { short_count++; seen_type = true; advance(p); continue; }
        if (k == TOK_KW_LONG)     { long_count++;  seen_type = true; advance(p); continue; }
        if (k == TOK_KW_SIGNED)   { seen_signed = true;   seen_type = true; advance(p); continue; }
        if (k == TOK_KW_UNSIGNED) { seen_unsigned = true;  seen_type = true; advance(p); continue; }

        /* struct/union/enum */
        if (k == TOK_KW_STRUCT) {
            custom_type = parse_struct_or_union(p, true);
            seen_type = true;
            continue;
        }
        if (k == TOK_KW_UNION) {
            custom_type = parse_struct_or_union(p, false);
            seen_type = true;
            continue;
        }
        if (k == TOK_KW_ENUM) {
            custom_type = parse_enum(p);
            seen_type = true;
            continue;
        }

        /* Typedef name */
        if (k == TOK_IDENT && !seen_type) {
            bool is_typedef = false;
            for (size_t i = 0; i < p->typedef_names.size; i++) {
                if (strcmp((const char *)vec_get(&p->typedef_names, i),
                           p->tok.text) == 0) {
                    is_typedef = true;
                    break;
                }
            }
            if (is_typedef) {
                /* Return a typedef reference */
                CType *ty = type_new(p->arena, TYPE_TYPEDEF);
                ty->tdef.name = p->tok.text;
                ty->tdef.underlying = NULL; /* resolved during sema */
                custom_type = ty;
                seen_type = true;
                advance(p);
                continue;
            }
        }

        break;
    }

    if (custom_type) {
        custom_type->quals = *tq;
        return custom_type;
    }

    if (!seen_type && !seen_signed && !seen_unsigned &&
        short_count == 0 && long_count == 0) {
        return NULL;
    }

    /* Resolve the type */
    CTypeKind result_kind = base_kind;

    if (seen_unsigned) {
        if (base_kind == TYPE_CHAR) result_kind = TYPE_UCHAR;
        else if (short_count > 0) result_kind = TYPE_USHORT;
        else if (long_count >= 2) result_kind = TYPE_ULLONG;
        else if (long_count == 1) result_kind = TYPE_ULONG;
        else result_kind = TYPE_UINT;
    } else if (seen_signed) {
        if (base_kind == TYPE_CHAR) result_kind = TYPE_SCHAR;
        else if (short_count > 0) result_kind = TYPE_SHORT;
        else if (long_count >= 2) result_kind = TYPE_LLONG;
        else if (long_count == 1) result_kind = TYPE_LONG;
        else result_kind = TYPE_INT;
    } else {
        if (short_count > 0) result_kind = TYPE_SHORT;
        else if (long_count >= 2) result_kind = TYPE_LLONG;
        else if (long_count == 1) {
            if (base_kind == TYPE_DOUBLE) result_kind = TYPE_LDOUBLE;
            else result_kind = TYPE_LONG;
        }
    }

    CType *ty = type_new(p->arena, result_kind);
    ty->quals = *tq;
    return ty;
}

/* ---- Declarator parsing ---- */

CType *parse_declarator(Parser *p, CType *base, const char **name) {
    *name = NULL;

    /* Pointer */
    while (match(p, TOK_STAR)) {
        int quals = 0;
        while (true) {
            if (match(p, TOK_KW_CONST))    { quals |= QUAL_CONST;    continue; }
            if (match(p, TOK_KW_VOLATILE)) { quals |= QUAL_VOLATILE; continue; }
            if (match(p, TOK_KW_RESTRICT)) { quals |= QUAL_RESTRICT; continue; }
            break;
        }
        base = type_ptr(p->arena, base, quals);
    }

    /* Parenthesized declarator or direct declarator */
    if (match(p, TOK_LPAREN)) {
        if (parser_is_typename(p) || check(p, TOK_RPAREN)) {
            /* This is actually a function parameter list, not a grouping paren.
             * Back up: this means the declarator has no name (abstract declarator)
             * and we're seeing the function suffix. */
            /* Parse parameters */
            bool is_variadic = false;
            Vector params = parse_parameter_list(p, &is_variadic);
            expect(p, TOK_RPAREN);

            CTypeParam *tparams = NULL;
            int nparams = (int)params.size;
            if (nparams > 0) {
                tparams = (CTypeParam *)arena_alloc(p->arena,
                    sizeof(CTypeParam) * nparams);
                for (int i = 0; i < nparams; i++) {
                    ASTNode *pd = (ASTNode *)vec_get(&params, i);
                    tparams[i].name = pd->param_decl.name;
                    tparams[i].type = pd->param_decl.param_type;
                }
            }
            vec_free(&params);

            base = type_func(p->arena, base, tparams, nparams, is_variadic);
            return base;
        } else {
            /* Grouping paren: recursively parse inner declarator */
            CType *inner = parse_declarator(p, base, name);
            expect(p, TOK_RPAREN);
            base = inner;
        }
    } else if (check(p, TOK_IDENT)) {
        *name = p->tok.text;
        advance(p);
    }

    /* Array and function suffixes */
    while (true) {
        if (match(p, TOK_LBRACKET)) {
            size_t count = 0;
            bool is_vla = false;
            if (match(p, TOK_STAR)) {
                /* VLA with unspecified size: [*] */
                is_vla = true;
            } else if (!check(p, TOK_RBRACKET)) {
                ASTNode *size_expr = parse_assignment_expression(p);
                if (size_expr && size_expr->kind == AST_INT_LIT) {
                    count = (size_t)size_expr->int_lit.value;
                } else {
                    is_vla = true; /* non-constant = VLA */
                }
            }
            expect(p, TOK_RBRACKET);
            CType *arr = type_array(p->arena, base, count);
            arr->array.is_vla = is_vla;
            base = arr;
        } else if (match(p, TOK_LPAREN)) {
            /* Function declarator */
            bool is_variadic = false;
            Vector params = parse_parameter_list(p, &is_variadic);
            expect(p, TOK_RPAREN);

            CTypeParam *tparams = NULL;
            int nparams = (int)params.size;
            if (nparams > 0) {
                tparams = (CTypeParam *)arena_alloc(p->arena,
                    sizeof(CTypeParam) * nparams);
                for (int i = 0; i < nparams; i++) {
                    ASTNode *pd = (ASTNode *)vec_get(&params, i);
                    tparams[i].name = pd->param_decl.name;
                    tparams[i].type = pd->param_decl.param_type;
                }
            }
            vec_free(&params);

            base = type_func(p->arena, base, tparams, nparams, is_variadic);
        } else {
            break;
        }
    }

    return base;
}

CType *parse_abstract_declarator(Parser *p, CType *base) {
    const char *dummy = NULL;
    return parse_declarator(p, base, &dummy);
}

Vector parse_parameter_list(Parser *p, bool *is_variadic) {
    Vector params;
    vec_init(&params);
    *is_variadic = false;

    if (check(p, TOK_RPAREN)) return params;

    /* Special case: (void) */
    if (check(p, TOK_KW_VOID) && lexer_peek(p->lex).kind == TOK_RPAREN) {
        advance(p);
        return params;
    }

    while (true) {
        if (match(p, TOK_ELLIPSIS)) {
            *is_variadic = true;
            break;
        }

        StorageClass sc = SC_NONE;
        TypeQual tq = TQ_NONE;
        FuncSpec fs = FS_NONE;
        CType *ptype = parse_type_specifier(p, &sc, &tq, &fs);
        if (!ptype) {
            diag_error(p->err, p->tok.loc, "expected parameter type");
            break;
        }

        const char *pname = NULL;
        ptype = parse_declarator(p, ptype, &pname);

        ASTNode *pd = ast_new(p->arena, AST_PARAM_DECL, p->tok.loc);
        pd->param_decl.param_type = ptype;
        pd->param_decl.name = pname;
        vec_push(&params, pd);

        if (!match(p, TOK_COMMA)) break;
    }

    return params;
}

/* ---- Top-level declaration ---- */

ASTNode *parse_declaration(Parser *p) {
    SrcLoc loc = p->tok.loc;

    StorageClass sc = SC_NONE;
    TypeQual tq = TQ_NONE;
    FuncSpec fspec = FS_NONE;
    CType *base = parse_type_specifier(p, &sc, &tq, &fspec);

    if (!base) {
        diag_error(p->err, loc, "expected type specifier");
        advance(p);
        return NULL;
    }

    /* Standalone struct/union/enum declaration */
    if (check(p, TOK_SEMICOLON)) {
        advance(p);
        ASTNode *decl = ast_new(p->arena, AST_VAR_DECL, loc);
        decl->var_decl.sc = sc;
        decl->var_decl.tq = tq;
        decl->var_decl.var_type = base;
        decl->var_decl.name = NULL;
        decl->var_decl.init = NULL;
        return decl;
    }

    const char *name = NULL;
    CType *decl_type = parse_declarator(p, base, &name);

    /* Function definition: has a compound statement body */
    if (decl_type && decl_type->kind == TYPE_FUNC && check(p, TOK_LBRACE)) {
        ASTNode *func = ast_new(p->arena, AST_FUNC_DEF, loc);
        func->func_def.sc = sc;
        func->func_def.fspec = fspec;
        func->func_def.name = name;
        func->func_def.ret_type = decl_type->func.ret;
        vec_init(&func->func_def.params);
        func->func_def.is_variadic = decl_type->func.is_variadic;

        /* Create parameter AST nodes */
        for (int i = 0; i < decl_type->func.param_count; i++) {
            ASTNode *pd = ast_new(p->arena, AST_PARAM_DECL, loc);
            pd->param_decl.param_type = decl_type->func.params[i].type;
            pd->param_decl.name = decl_type->func.params[i].name;
            vec_push(&func->func_def.params, pd);
        }

        func->func_def.body = parse_compound_statement(p);
        func->type = decl_type;
        return func;
    }

    /* Variable declaration */
    ASTNode *first_decl = ast_new(p->arena, AST_VAR_DECL, loc);
    first_decl->var_decl.sc = sc;
    first_decl->var_decl.tq = tq;
    first_decl->var_decl.var_type = decl_type;
    first_decl->var_decl.name = name;
    first_decl->var_decl.init = NULL;

    /* Register typedef name */
    if (sc == SC_TYPEDEF && name) {
        vec_push(&p->typedef_names, (void *)name);
    }

    /* Initializer */
    if (match(p, TOK_ASSIGN)) {
        if (check(p, TOK_LBRACE)) {
            first_decl->var_decl.init = parse_expression(p); /* init list */
        } else {
            first_decl->var_decl.init = parse_assignment_expression(p);
        }
    }

    /* Handle multiple declarators: int a, b, c; */
    /* For simplicity, we just handle the first one here */
    expect(p, TOK_SEMICOLON);

    return first_decl;
}
