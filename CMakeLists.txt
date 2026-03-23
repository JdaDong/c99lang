cmake_minimum_required(VERSION 3.16)
project(c99lang C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# Debug / Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG_BUILD)
    add_compile_options(-g -O0)
else()
    add_compile_options(-O2)
endif()

# ---------- Sources ----------
set(COMMON_SRCS
    src/util/arena.c
    src/util/strtab.c
    src/util/error.c
    src/util/vector.c
)

set(LEXER_SRCS
    src/lexer/lexer.c
    src/lexer/token.c
)

set(PREPROC_SRCS
    src/preprocessor/preprocessor.c
    src/preprocessor/macro.c
)

set(PARSER_SRCS
    src/parser/parser.c
    src/parser/parser_decl.c
    src/parser/parser_stmt.c
    src/parser/parser_expr.c
    src/parser/ast.c
    src/parser/ast_print.c
)

set(SEMA_SRCS
    src/sema/sema.c
    src/sema/type.c
    src/sema/symtab.c
    src/sema/scope.c
)

set(IR_SRCS
    src/ir/ir_gen.c
    src/ir/ir.c
    src/ir/ir_print.c
)

set(CODEGEN_SRCS
    src/codegen/codegen_x86_64.c
    src/codegen/regalloc.c
)

set(ALL_SRCS
    ${COMMON_SRCS}
    ${LEXER_SRCS}
    ${PREPROC_SRCS}
    ${PARSER_SRCS}
    ${SEMA_SRCS}
    ${IR_SRCS}
    ${CODEGEN_SRCS}
    src/main.c
)

add_executable(c99c ${ALL_SRCS})
target_include_directories(c99c PRIVATE ${CMAKE_SOURCE_DIR}/include)

# ---------- Tests ----------
enable_testing()

# Test: Lexer (original)
add_executable(test_lexer
    tests/test_lexer.c
    ${COMMON_SRCS}
    ${LEXER_SRCS}
)
target_include_directories(test_lexer PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME test_lexer COMMAND test_lexer)

# Test: Lexer extended
add_executable(test_lexer_extended
    tests/test_lexer_extended.c
    ${COMMON_SRCS}
    ${LEXER_SRCS}
)
target_include_directories(test_lexer_extended PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME test_lexer_extended COMMAND test_lexer_extended)

# Test: Parser (original)
add_executable(test_parser
    tests/test_parser.c
    ${COMMON_SRCS}
    ${LEXER_SRCS}
    ${PARSER_SRCS}
    src/sema/type.c
)
target_include_directories(test_parser PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME test_parser COMMAND test_parser)

# Test: Parser extended
add_executable(test_parser_extended
    tests/test_parser_extended.c
    ${COMMON_SRCS}
    ${LEXER_SRCS}
    ${PARSER_SRCS}
    src/sema/type.c
)
target_include_directories(test_parser_extended PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME test_parser_extended COMMAND test_parser_extended)

# Test: Arena allocator
add_executable(test_arena
    tests/test_arena.c
    ${COMMON_SRCS}
)
target_include_directories(test_arena PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME test_arena COMMAND test_arena)

# Test: Vector
add_executable(test_vector
    tests/test_vector.c
    src/util/vector.c
)
target_include_directories(test_vector PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME test_vector COMMAND test_vector)

# Test: String table
add_executable(test_strtab
    tests/test_strtab.c
    src/util/arena.c
    src/util/strtab.c
    src/util/vector.c
)
target_include_directories(test_strtab PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME test_strtab COMMAND test_strtab)

# Test: Preprocessor
add_executable(test_preprocessor
    tests/test_preprocessor.c
    ${COMMON_SRCS}
    ${PREPROC_SRCS}
)
target_include_directories(test_preprocessor PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME test_preprocessor COMMAND test_preprocessor)

# Test: Type system & Symbol table
add_executable(test_type_symtab
    tests/test_type_symtab.c
    ${COMMON_SRCS}
    src/sema/type.c
    src/sema/symtab.c
)
target_include_directories(test_type_symtab PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME test_type_symtab COMMAND test_type_symtab)

# Test: IR infrastructure
add_executable(test_ir
    tests/test_ir.c
    ${COMMON_SRCS}
    src/sema/type.c
    src/ir/ir.c
    src/ir/ir_print.c
)
target_include_directories(test_ir PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME test_ir COMMAND test_ir)

# Install
install(TARGETS c99c RUNTIME DESTINATION bin)
