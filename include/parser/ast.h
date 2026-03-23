/*
 * c99lang - A C99 Compiler
 * Abstract Syntax Tree definitions
 */
#ifndef C99_AST_H
#define C99_AST_H

#include "util/error.h"
#include "util/vector.h"
#include "util/arena.h"
#include "sema/type.h"

/* Forward declarations */
typedef struct ASTNode ASTNode;
typedef struct ASTDeclarator ASTDeclarator;

/* ---------- AST Node Kinds ---------- */
typedef enum ASTKind {
    /* Translation unit */
    AST_TRANSLATION_UNIT,

    /* Declarations */
    AST_FUNC_DEF,
    AST_VAR_DECL,
    AST_PARAM_DECL,
    AST_TYPEDEF_DECL,
    AST_STRUCT_DECL,
    AST_UNION_DECL,
    AST_ENUM_DECL,
    AST_ENUM_CONST,
    AST_FIELD_DECL,

    /* Statements */
    AST_COMPOUND_STMT,
    AST_EXPR_STMT,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_DO_WHILE_STMT,
    AST_FOR_STMT,
    AST_SWITCH_STMT,
    AST_CASE_STMT,
    AST_DEFAULT_STMT,
    AST_RETURN_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_GOTO_STMT,
    AST_LABEL_STMT,
    AST_NULL_STMT,

    /* Expressions */
    AST_INT_LIT,
    AST_FLOAT_LIT,
    AST_CHAR_LIT,
    AST_STRING_LIT,
    AST_IDENT_EXPR,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_POSTFIX_EXPR,
    AST_TERNARY_EXPR,
    AST_CALL_EXPR,
    AST_INDEX_EXPR,
    AST_MEMBER_EXPR,
    AST_CAST_EXPR,
    AST_SIZEOF_EXPR,
    AST_COMPOUND_LIT,       /* C99 compound literal */
    AST_INIT_LIST,
    AST_DESIGNATOR,          /* C99 designated initializer */
    AST_ASSIGN_EXPR,
    AST_COMMA_EXPR,

    AST_KIND_COUNT
} ASTKind;

/* Binary operators */
typedef enum BinOp {
    BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV, BIN_MOD,
    BIN_LSHIFT, BIN_RSHIFT,
    BIN_LT, BIN_GT, BIN_LE, BIN_GE, BIN_EQ, BIN_NE,
    BIN_BIT_AND, BIN_BIT_XOR, BIN_BIT_OR,
    BIN_LOG_AND, BIN_LOG_OR,
} BinOp;

/* Unary operators */
typedef enum UnaryOp {
    UNARY_NEG,       /* - */
    UNARY_POS,       /* + */
    UNARY_NOT,       /* ! */
    UNARY_BITNOT,    /* ~ */
    UNARY_DEREF,     /* * */
    UNARY_ADDR,      /* & */
    UNARY_PRE_INC,   /* ++x */
    UNARY_PRE_DEC,   /* --x */
} UnaryOp;

/* Postfix operators */
typedef enum PostfixOp {
    POSTFIX_INC,     /* x++ */
    POSTFIX_DEC,     /* x-- */
} PostfixOp;

/* Assignment operators */
typedef enum AssignOp {
    ASSIGN_EQ,
    ASSIGN_MUL, ASSIGN_DIV, ASSIGN_MOD,
    ASSIGN_ADD, ASSIGN_SUB,
    ASSIGN_LSHIFT, ASSIGN_RSHIFT,
    ASSIGN_AND, ASSIGN_XOR, ASSIGN_OR,
} AssignOp;

/* Storage class */
typedef enum StorageClass {
    SC_NONE = 0,
    SC_TYPEDEF,
    SC_EXTERN,
    SC_STATIC,
    SC_AUTO,
    SC_REGISTER,
} StorageClass;

/* Type qualifiers (bitmask) */
typedef enum TypeQual {
    TQ_NONE     = 0,
    TQ_CONST    = 1,
    TQ_VOLATILE = 2,
    TQ_RESTRICT = 4,  /* C99 */
} TypeQual;

/* Function specifier */
typedef enum FuncSpec {
    FS_NONE   = 0,
    FS_INLINE = 1,   /* C99 */
} FuncSpec;

/* ---------- AST Node ---------- */
struct ASTNode {
    ASTKind kind;
    SrcLoc loc;
    CType *type;           /* set during semantic analysis */

    union {
        /* AST_TRANSLATION_UNIT */
        struct { Vector decls; } tu;

        /* AST_FUNC_DEF */
        struct {
            StorageClass sc;
            FuncSpec fspec;
            CType *ret_type;
            const char *name;
            Vector params;      /* ASTNode* (AST_PARAM_DECL) */
            bool is_variadic;
            ASTNode *body;      /* AST_COMPOUND_STMT */
        } func_def;

        /* AST_VAR_DECL */
        struct {
            StorageClass sc;
            TypeQual tq;
            CType *var_type;
            const char *name;
            ASTNode *init;      /* NULL if no initializer */
        } var_decl;

        /* AST_PARAM_DECL */
        struct {
            CType *param_type;
            const char *name;   /* can be NULL for abstract declarator */
        } param_decl;

        /* AST_STRUCT_DECL / AST_UNION_DECL */
        struct {
            const char *tag;    /* NULL for anonymous */
            Vector fields;      /* ASTNode* (AST_FIELD_DECL) */
            bool is_definition;
        } record_decl;

        /* AST_FIELD_DECL */
        struct {
            CType *field_type;
            const char *name;
            ASTNode *bit_width; /* NULL if not a bitfield */
        } field_decl;

        /* AST_ENUM_DECL */
        struct {
            const char *tag;
            Vector constants;   /* ASTNode* (AST_ENUM_CONST) */
            bool is_definition;
        } enum_decl;

        /* AST_ENUM_CONST */
        struct {
            const char *name;
            ASTNode *value;     /* NULL if implicit */
        } enum_const;

        /* AST_COMPOUND_STMT */
        struct { Vector stmts; } compound;

        /* AST_EXPR_STMT */
        struct { ASTNode *expr; } expr_stmt;

        /* AST_IF_STMT */
        struct {
            ASTNode *cond;
            ASTNode *then_stmt;
            ASTNode *else_stmt; /* NULL if no else */
        } if_stmt;

        /* AST_WHILE_STMT */
        struct {
            ASTNode *cond;
            ASTNode *body;
        } while_stmt;

        /* AST_DO_WHILE_STMT */
        struct {
            ASTNode *cond;
            ASTNode *body;
        } do_while_stmt;

        /* AST_FOR_STMT */
        struct {
            ASTNode *init;      /* expr or decl, or NULL */
            ASTNode *cond;      /* NULL = always true */
            ASTNode *incr;      /* NULL = no increment */
            ASTNode *body;
        } for_stmt;

        /* AST_SWITCH_STMT */
        struct {
            ASTNode *expr;
            ASTNode *body;
        } switch_stmt;

        /* AST_CASE_STMT */
        struct {
            ASTNode *value;
            ASTNode *stmt;
        } case_stmt;

        /* AST_DEFAULT_STMT */
        struct { ASTNode *stmt; } default_stmt;

        /* AST_RETURN_STMT */
        struct { ASTNode *expr; /* NULL for bare return */ } return_stmt;

        /* AST_GOTO_STMT */
        struct { const char *label; } goto_stmt;

        /* AST_LABEL_STMT */
        struct {
            const char *label;
            ASTNode *stmt;
        } label_stmt;

        /* AST_INT_LIT */
        struct {
            unsigned long long value;
            int suffix; /* IntSuffix */
        } int_lit;

        /* AST_FLOAT_LIT */
        struct {
            long double value;
            int suffix; /* FloatSuffix */
        } float_lit;

        /* AST_CHAR_LIT */
        struct { int value; } char_lit;

        /* AST_STRING_LIT */
        struct {
            const char *value;
            size_t length;
        } string_lit;

        /* AST_IDENT_EXPR */
        struct { const char *name; } ident;

        /* AST_BINARY_EXPR */
        struct {
            BinOp op;
            ASTNode *lhs;
            ASTNode *rhs;
        } binary;

        /* AST_UNARY_EXPR */
        struct {
            UnaryOp op;
            ASTNode *operand;
        } unary;

        /* AST_POSTFIX_EXPR */
        struct {
            PostfixOp op;
            ASTNode *operand;
        } postfix;

        /* AST_TERNARY_EXPR */
        struct {
            ASTNode *cond;
            ASTNode *then_expr;
            ASTNode *else_expr;
        } ternary;

        /* AST_CALL_EXPR */
        struct {
            ASTNode *callee;
            Vector args;         /* ASTNode* */
        } call;

        /* AST_INDEX_EXPR */
        struct {
            ASTNode *array;
            ASTNode *index;
        } index_expr;

        /* AST_MEMBER_EXPR */
        struct {
            ASTNode *object;
            const char *member;
            bool is_arrow;       /* -> vs . */
        } member;

        /* AST_CAST_EXPR */
        struct {
            CType *cast_type;
            ASTNode *operand;
        } cast;

        /* AST_SIZEOF_EXPR */
        struct {
            CType *sizeof_type;  /* sizeof(type) */
            ASTNode *sizeof_expr;/* sizeof expr */
            bool is_type;
        } sizeof_expr;

        /* AST_ASSIGN_EXPR */
        struct {
            AssignOp op;
            ASTNode *lhs;
            ASTNode *rhs;
        } assign;

        /* AST_COMMA_EXPR */
        struct {
            ASTNode *lhs;
            ASTNode *rhs;
        } comma;

        /* AST_INIT_LIST / AST_COMPOUND_LIT */
        struct {
            CType *lit_type;     /* for compound literal */
            Vector items;        /* ASTNode* */
        } init_list;

        /* AST_DESIGNATOR */
        struct {
            ASTNode *index;      /* for array designator [idx] */
            const char *field;   /* for struct designator .field */
            ASTNode *init;       /* initializer value */
        } designator;
    };
};

/* AST construction helpers */
ASTNode *ast_new(Arena *a, ASTKind kind, SrcLoc loc);

/* Pretty printer */
void ast_print(const ASTNode *node, int indent);
const char *ast_kind_str(ASTKind kind);
const char *binop_str(BinOp op);
const char *unaryop_str(UnaryOp op);
const char *assignop_str(AssignOp op);

#endif /* C99_AST_H */
