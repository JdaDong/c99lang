cd build && cmake .. && make

# 基本编译（生成 .s 汇编）
./c99c hello.c -o hello.s

# 仅预处理
./c99c -E hello.c

# 查看 Token 流
./c99c --dump-tokens hello.c

# 查看 AST
./c99c --dump-ast hello.c

# 查看 IR
./c99c --dump-ir hello.c
