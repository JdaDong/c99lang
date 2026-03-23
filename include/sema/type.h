/*
 * c99lang - A C99 Compiler
 * Type system
 */
#ifndef C99_TYPE_H
#define C99_TYPE_H

#include <stdbool.h>
#include <stddef.h>
#include "util/arena.h"
#include "util/vector.h"

typedef struct CType CType;

typedef enum CTypeKind {
    TYPE_VOID,
    TYPE_BOOL,          /* C99 _Bool */
    TYPE_CHAR,
    TYPE_SCHAR,
    TYPE_UCHAR,
    TYPE_SHORT,
    TYPE_USHORT,
    TYPE_INT,
    TYPE_UINT,
    TYPE_LONG,
    TYPE_ULONG,
    TYPE_LLONG,         /* long long */
    TYPE_ULLONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_LDOUBLE,       /* long double */
    TYPE_FLOAT_COMPLEX, /* C99 */
    TYPE_DOUBLE_COMPLEX,
    TYPE_LDOUBLE_COMPLEX,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNC,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
    TYPE_TYPEDEF,       /* alias */
} CTypeKind;

/* Type qualifier bitmask */
#define QUAL_CONST    1
#define QUAL_VOLATILE 2
#define QUAL_RESTRICT 4

typedef struct CTypeField {
    const char *name;
    CType *type;
    int bit_width;       /* -1 if not a bitfield */
    size_t offset;       /* computed during layout */
} CTypeField;

typedef struct CTypeParam {
    const char *name;
    CType *type;
} CTypeParam;

struct CType {
    CTypeKind kind;
    int quals;           /* QUAL_CONST | QUAL_VOLATILE | QUAL_RESTRICT */
    size_t size;         /* sizeof, computed during sema */
    size_t align;        /* alignment */

    union {
        /* TYPE_POINTER */
        struct { CType *pointee; } ptr;

        /* TYPE_ARRAY */
        struct {
            CType *elem;
            size_t count;     /* 0 for VLA / incomplete */
            bool is_vla;      /* C99 variable-length array */
        } array;

        /* TYPE_FUNC */
        struct {
            CType *ret;
            CTypeParam *params;
            int param_count;
            bool is_variadic;
            bool is_oldstyle;
        } func;

        /* TYPE_STRUCT / TYPE_UNION */
        struct {
            const char *tag;
            CTypeField *fields;
            int field_count;
            bool is_complete;
        } record;

        /* TYPE_ENUM */
        struct {
            const char *tag;
            /* enum constants stored in symtab */
            bool is_complete;
        } enumeration;

        /* TYPE_TYPEDEF */
        struct {
            const char *name;
            CType *underlying;
        } tdef;
    };
};

/* Type construction */
CType *type_new(Arena *a, CTypeKind kind);
CType *type_ptr(Arena *a, CType *pointee, int quals);
CType *type_array(Arena *a, CType *elem, size_t count);
CType *type_func(Arena *a, CType *ret, CTypeParam *params,
                 int nparams, bool variadic);

/* Type queries */
bool type_is_integer(const CType *t);
bool type_is_floating(const CType *t);
bool type_is_arithmetic(const CType *t);
bool type_is_scalar(const CType *t);
bool type_is_complete(const CType *t);
bool type_is_compatible(const CType *a, const CType *b);
CType *type_unqualified(CType *t);

/* Canonical base types (singletons) */
CType *type_void_get(void);
CType *type_bool_get(void);
CType *type_char_get(void);
CType *type_int_get(void);
CType *type_uint_get(void);
CType *type_long_get(void);
CType *type_ulong_get(void);
CType *type_llong_get(void);
CType *type_ullong_get(void);
CType *type_float_get(void);
CType *type_double_get(void);

/* Type printing (for debug) */
void type_print(const CType *t);
char *type_to_string(const CType *t, Arena *a);

#endif /* C99_TYPE_H */
