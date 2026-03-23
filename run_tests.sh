#!/bin/bash
# =============================================================================
# c99lang 单元测试 - 编译 & 运行脚本
# =============================================================================

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"

# 统计
TOTAL=0
PASSED=0
FAILED=0
FAILED_TESTS=()

# -----------------------------------------------------------------------------
# 辅助函数
# -----------------------------------------------------------------------------
print_header() {
    echo ""
    echo -e "${CYAN}================================================================${NC}"
    echo -e "${CYAN}  $1${NC}"
    echo -e "${CYAN}================================================================${NC}"
    echo ""
}

print_step() {
    echo -e "${BOLD}>> $1${NC}"
}

print_pass() {
    echo -e "  ${GREEN}✅ PASS${NC} - $1"
}

print_fail() {
    echo -e "  ${RED}❌ FAIL${NC} - $1"
}

print_summary_line() {
    local label=$1 value=$2 color=$3
    printf "  ${color}%-20s %s${NC}\n" "$label" "$value"
}

# 运行单个测试
run_test() {
    local test_name=$1
    local test_bin="${BUILD_DIR}/${test_name}"

    TOTAL=$((TOTAL + 1))

    if [ ! -f "$test_bin" ]; then
        print_fail "${test_name} (二进制文件不存在)"
        FAILED=$((FAILED + 1))
        FAILED_TESTS+=("$test_name")
        return
    fi

    echo -e "\n${YELLOW}---------- ${test_name} ----------${NC}"

    # 运行测试并捕获输出和退出码
    local output
    local exit_code
    output=$("$test_bin" 2>&1) && exit_code=0 || exit_code=$?

    echo "$output"

    if [ $exit_code -eq 0 ]; then
        print_pass "$test_name"
        PASSED=$((PASSED + 1))
    else
        print_fail "$test_name (exit code: $exit_code)"
        FAILED=$((FAILED + 1))
        FAILED_TESTS+=("$test_name")
    fi
}

# -----------------------------------------------------------------------------
# 主流程
# -----------------------------------------------------------------------------
print_header "c99lang 单元测试套件"

# 1) CMake 配置
print_step "CMake 配置..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Debug > /dev/null 2>&1
echo -e "  ${GREEN}✔${NC} CMake 配置完成"

# 2) 编译
print_step "编译所有目标..."
NPROC=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
if make -j"$NPROC" 2>&1 | tail -3; then
    echo -e "  ${GREEN}✔${NC} 编译成功"
else
    echo -e "  ${RED}✘ 编译失败，请检查错误信息${NC}"
    exit 1
fi

# 3) 运行所有测试
print_header "运行测试"

TEST_LIST=(
    test_arena
    test_vector
    test_strtab
    test_lexer
    test_lexer_extended
    test_preprocessor
    test_parser
    test_parser_extended
    test_type_symtab
    test_ir
)

for t in "${TEST_LIST[@]}"; do
    run_test "$t"
done

# 4) 汇总
print_header "测试结果汇总"

print_summary_line "总计:" "$TOTAL" "$BOLD"
print_summary_line "通过:" "$PASSED" "$GREEN"
print_summary_line "失败:" "$FAILED" "$RED"

if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
    echo ""
    echo -e "  ${RED}失败的测试:${NC}"
    for ft in "${FAILED_TESTS[@]}"; do
        echo -e "    ${RED}• ${ft}${NC}"
    done
fi

echo ""
if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}${BOLD}🎉 全部 ${TOTAL} 个测试通过！${NC}"
    echo ""
    exit 0
else
    echo -e "${RED}${BOLD}💥 ${FAILED}/${TOTAL} 个测试失败${NC}"
    echo ""
    exit 1
fi
