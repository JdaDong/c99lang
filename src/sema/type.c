/*
 * c99lang - Type system implementation
 */
#include "sema/type.h"
#include <stdio.h>
#include <string.h>

/* ---- Singleton base types ---- */
static CType _type_void   = { .kind = TYPE_VOID,   .size = 0, .align = 1 };
static CType _type_bool   = { .kind = TYPE_BOOL,   .size = 1, .align = 1 };
static CType _type_char   = { .kind = TYPE_CHAR,   .size = 1, .align = 1 };
static CType _type_schar  = { .kind = TYPE_SCHAR,  .size = 1, .align = 1 };
static CType _type_uchar  = { .kind = TYPE_UCHAR,  .size = 1, .align = 1 };
static CType _type_short  = { .kind = TYPE_SHORT,  .size = 2, .align = 2 };
static CType _type_ushort = { .kind = TYPE_USHORT, .size = 2, .align = 2 };
static CType _type_int    = { .kind = TYPE_INT,    .size = 4, .align = 4 };
static CType _type_uint   = { .kind = TYPE_UINT,   .size = 4, .align = 4 };
static CType _type_long   = { .kind = TYPE_LONG,   .size = 8, .align = 8 };
static CType _type_ulong  = { .kind = TYPE_ULONG,  .size = 8, .align = 8 };
static CType _type_llong  = { .kind = TYPE_LLONG,  .size = 8, .align = 8 };
static CType _type_ullong = { .kind = TYPE_ULLONG, .size = 8, .align = 8 };
static CType _type_float  = { .kind = TYPE_FLOAT,  .size = 4, .align = 4 };
static CType _type_double = { .kind = TYPE_DOUBLE, .size = 8, .align = 8 };
static CType _type_ldouble = { .kind = TYPE_LDOUBLE, .size = 16, .align = 16 };

CType *type_void_get(void)   { return &_type_void; }
CType *type_bool_get(void)   { return &_type_bool; }
CType *type_char_get(void)   { return &_type_char; }
CType *type_int_get(void)    { return &_type_int; }
CType *type_uint_get(void)   { return &_type_uint; }
CType *type_long_get(void)   { return &_type_long; }
CType *type_ulong_get(void)  { return &_type_ulong; }
CType *type_llong_get(void)  { return &_type_llong; }
CType *type_ullong_get(void) { return &_type_ullong; }
CType *type_float_get(void)  { return &_type_float; }
CType *type_double_get(void) { return &_type_double; }

/* ---- Type construction ---- */

CType *type_new(Arena *a, CTypeKind kind) {
    /* Return singletons for basic types */
    switch (kind) {
    case TYPE_VOID:   return &_type_void;
    case TYPE_BOOL:   return &_type_bool;
    case TYPE_CHAR:   return &_type_char;
    case TYPE_SCHAR:  return &_type_schar;
    case TYPE_UCHAR:  return &_type_uchar;
    case TYPE_SHORT:  return &_type_short;
    case TYPE_USHORT: return &_type_ushort;
    case TYPE_INT:    return &_type_int;
    case TYPE_UINT:   return &_type_uint;
    case TYPE_LONG:   return &_type_long;
    case TYPE_ULONG:  return &_type_ulong;
    case TYPE_LLONG:  return &_type_llong;
    case TYPE_ULLONG: return &_type_ullong;
    case TYPE_FLOAT:  return &_type_float;
    case TYPE_DOUBLE: return &_type_double;
    case TYPE_LDOUBLE: return &_type_ldouble;
    default: break;
    }

    CType *t = (CType *)arena_calloc(a, sizeof(CType));
    t->kind = kind;
    return t;
}

CType *type_ptr(Arena *a, CType *pointee, int quals) {
    CType *t = (CType *)arena_calloc(a, sizeof(CType));
    t->kind = TYPE_POINTER;
    t->size = 8;    /* 64-bit pointers */
    t->align = 8;
    t->quals = quals;
    t->ptr.pointee = pointee;
    return t;
}

CType *type_array(Arena *a, CType *elem, size_t count) {
    CType *t = (CType *)arena_calloc(a, sizeof(CType));
    t->kind = TYPE_ARRAY;
    t->array.elem = elem;
    t->array.count = count;
    t->array.is_vla = false;
    t->size = elem->size * count;
    t->align = elem->align;
    return t;
}

CType *type_func(Arena *a, CType *ret, CTypeParam *params,
                 int nparams, bool variadic) {
    CType *t = (CType *)arena_calloc(a, sizeof(CType));
    t->kind = TYPE_FUNC;
    t->func.ret = ret;
    t->func.params = params;
    t->func.param_count = nparams;
    t->func.is_variadic = variadic;
    t->func.is_oldstyle = false;
    return t;
}

/* ---- Type queries ---- */

bool type_is_integer(const CType *t) {
    if (!t) return false;
    switch (t->kind) {
    case TYPE_BOOL: case TYPE_CHAR: case TYPE_SCHAR: case TYPE_UCHAR:
    case TYPE_SHORT: case TYPE_USHORT:
    case TYPE_INT: case TYPE_UINT:
    case TYPE_LONG: case TYPE_ULONG:
    case TYPE_LLONG: case TYPE_ULLONG:
    case TYPE_ENUM:
        return true;
    default: return false;
    }
}

bool type_is_floating(const CType *t) {
    if (!t) return false;
    switch (t->kind) {
    case TYPE_FLOAT: case TYPE_DOUBLE: case TYPE_LDOUBLE:
    case TYPE_FLOAT_COMPLEX: case TYPE_DOUBLE_COMPLEX:
    case TYPE_LDOUBLE_COMPLEX:
        return true;
    default: return false;
    }
}

bool type_is_arithmetic(const CType *t) {
    return type_is_integer(t) || type_is_floating(t);
}

bool type_is_scalar(const CType *t) {
    return type_is_arithmetic(t) || (t && t->kind == TYPE_POINTER);
}

bool type_is_complete(const CType *t) {
    if (!t) return false;
    if (t->kind == TYPE_VOID) return false;
    if (t->kind == TYPE_STRUCT || t->kind == TYPE_UNION)
        return t->record.is_complete;
    if (t->kind == TYPE_ARRAY) return t->array.count > 0 || t->array.is_vla;
    return true;
}

bool type_is_compatible(const CType *a, const CType *b) {
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    if (a->kind == TYPE_POINTER)
        return type_is_compatible(a->ptr.pointee, b->ptr.pointee);
    return true; /* simplified */
}

CType *type_unqualified(CType *t) {
    /* For basic types, just return the singleton without quals */
    return t; /* simplified */
}

/* ---- Type printing ---- */

static const char *type_kind_str(CTypeKind kind) {
    switch (kind) {
    case TYPE_VOID:   return "void";
    case TYPE_BOOL:   return "_Bool";
    case TYPE_CHAR:   return "char";
    case TYPE_SCHAR:  return "signed char";
    case TYPE_UCHAR:  return "unsigned char";
    case TYPE_SHORT:  return "short";
    case TYPE_USHORT: return "unsigned short";
    case TYPE_INT:    return "int";
    case TYPE_UINT:   return "unsigned int";
    case TYPE_LONG:   return "long";
    case TYPE_ULONG:  return "unsigned long";
    case TYPE_LLONG:  return "long long";
    case TYPE_ULLONG: return "unsigned long long";
    case TYPE_FLOAT:  return "float";
    case TYPE_DOUBLE: return "double";
    case TYPE_LDOUBLE:return "long double";
    case TYPE_FLOAT_COMPLEX:  return "float _Complex";
    case TYPE_DOUBLE_COMPLEX: return "double _Complex";
    case TYPE_LDOUBLE_COMPLEX:return "long double _Complex";
    default: return "<type>";
    }
}

void type_print(const CType *t) {
    if (!t) { printf("<null-type>"); return; }

    if (t->quals & QUAL_CONST)    printf("const ");
    if (t->quals & QUAL_VOLATILE) printf("volatile ");
    if (t->quals & QUAL_RESTRICT) printf("restrict ");

    switch (t->kind) {
    case TYPE_POINTER:
        type_print(t->ptr.pointee);
        printf("*");
        break;
    case TYPE_ARRAY:
        type_print(t->array.elem);
        if (t->array.is_vla) printf("[*]");
        else printf("[%zu]", t->array.count);
        break;
    case TYPE_FUNC:
        type_print(t->func.ret);
        printf("(");
        for (int i = 0; i < t->func.param_count; i++) {
            if (i > 0) printf(", ");
            type_print(t->func.params[i].type);
            if (t->func.params[i].name)
                printf(" %s", t->func.params[i].name);
        }
        if (t->func.is_variadic) printf(", ...");
        printf(")");
        break;
    case TYPE_STRUCT:
        printf("struct %s", t->record.tag ? t->record.tag : "<anon>");
        break;
    case TYPE_UNION:
        printf("union %s", t->record.tag ? t->record.tag : "<anon>");
        break;
    case TYPE_ENUM:
        printf("enum %s", t->enumeration.tag ? t->enumeration.tag : "<anon>");
        break;
    case TYPE_TYPEDEF:
        printf("%s", t->tdef.name ? t->tdef.name : "<typedef>");
        break;
    default:
        printf("%s", type_kind_str(t->kind));
        break;
    }
}

char *type_to_string(const CType *t, Arena *a) {
    /* Simple implementation: use a static buffer */
    char buf[512] = {0};
    /* We'd need to rewrite type_print to write to a buffer, but for now: */
    (void)t;
    snprintf(buf, sizeof(buf), "<type>");
    return arena_strdup(a, buf);
}
