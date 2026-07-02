#!/usr/bin/env bash
# ============================================================
# RISC-V 模拟器自动化测试脚本
# 用法: bash tests/run_tests.sh [--verbose] [--filter <name>]
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
SIM_BIN="$BUILD_DIR/riscv_sim"
SMOKE_BIN="$BUILD_DIR/riscv_smoke_test"
EXAMPLES_DIR="$PROJECT_DIR/examples"

PASSED=0
FAILED=0
SKIPPED=0
VERBOSE=0
FILTER=""

# ── 参数解析 ──────────────────────────────────────────────

for arg in "$@"; do
    case "$arg" in
        --verbose|-v) VERBOSE=1 ;;
        --filter=*)   FILTER="${arg#*=}" ;;
        *) ;;
    esac
done

echo "=========================================="
echo " RISC-V Simulator Test Suite"
echo "=========================================="
echo ""

# ── 辅助函数 ──────────────────────────────────────────────

run_test() {
    local name="$1"
    local cmd="$2"
    local expected_exit="${3:-0}"

    if [[ -n "$FILTER" && "$name" != *"$FILTER"* ]]; then
        ((SKIPPED++))
        echo "  SKIP  $name (filtered)"
        return
    fi

    if [[ "$VERBOSE" -eq 1 ]]; then
        echo "── $name ──"
        echo "  CMD: $cmd"
    fi

    set +e
    eval "$cmd" > "$BUILD_DIR/.test_stdout" 2> "$BUILD_DIR/.test_stderr"
    local actual_exit=$?
    set -e

    if [[ $actual_exit -eq $expected_exit ]]; then
        ((PASSED++))
        echo "  PASS  $name"
    else
        ((FAILED++))
        echo "  FAIL  $name (exit code: $actual_exit, expected: $expected_exit)"
        if [[ "$VERBOSE" -eq 1 ]]; then
            echo "  --- stdout ---"
            cat "$BUILD_DIR/.test_stdout"
            echo "  --- stderr ---"
            cat "$BUILD_DIR/.test_stderr"
        fi
    fi
}

# ── 1. 单元测试 ───────────────────────────────────────────

echo "[1] Unit Tests"
echo "──────────────────────────────────────────"

if [[ -x "$SMOKE_BIN" ]]; then
    run_test "memory smoke test" "$SMOKE_BIN" 0
else
    echo "  SKIP  memory smoke test (binary not found — build first)"
    ((SKIPPED++))
fi

# ── 2. 指令级测试 ─────────────────────────────────────────

echo ""
echo "[2] Instruction Tests (RV32I)"
echo "──────────────────────────────────────────"

# 等待 A 成员提供测试 ELF 后补充实际路径
# 以下为框架占位，ELD 就绪后解除注释：

# INSTR_DIR="$EXAMPLES_DIR/instructions"
# for elf in "$INSTR_DIR"/*.elf; do
#     name="$(basename "$elf" .elf)"
#     run_test "instr/$name" "'$SIM_BIN' '$elf' -c | tail -1" 0
# done

echo "  SKIP  instruction tests (ELF files not yet available from member A)"
SKIPPED=$((SKIPPED + 47))

# ── 3. 程序级测试 ─────────────────────────────────────────

echo ""
echo "[3] Program Tests"
echo "──────────────────────────────────────────"

# Hello World — 等待 B 提供 hello.elf
if [[ -f "$EXAMPLES_DIR/hello.elf" ]]; then
    run_test "hello world" "'$SIM_BIN' '$EXAMPLES_DIR/hello.elf' -c | grep -q 'Hello'" 0
else
    echo "  SKIP  hello world (hello.elf not yet available from member B)"
    ((SKIPPED++))
fi

# 循环测试
if [[ -f "$EXAMPLES_DIR/loop.elf" ]]; then
    run_test "loop" "'$SIM_BIN' '$EXAMPLES_DIR/loop.elf' -c" 0
else
    echo "  SKIP  loop (loop.elf not yet available)"
    ((SKIPPED++))
fi

# 递归测试
if [[ -f "$EXAMPLES_DIR/recursion.elf" ]]; then
    run_test "recursion" "'$SIM_BIN' '$EXAMPLES_DIR/recursion.elf' -c" 0
else
    echo "  SKIP  recursion (recursion.elf not yet available)"
    ((SKIPPED++))
fi

# 数组运算测试
if [[ -f "$EXAMPLES_DIR/array.elf" ]]; then
    run_test "array" "'$SIM_BIN' '$EXAMPLES_DIR/array.elf' -c" 0
else
    echo "  SKIP  array (array.elf not yet available)"
    ((SKIPPED++))
fi

# ── 4. 调试器命令测试 ─────────────────────────────────────

echo ""
echo "[4] Debugger Command Tests"
echo "──────────────────────────────────────────"

# 用 echo 管道送入命令，验证调试器不崩溃
DEBUG_CMDS="help\nquit\n"
run_test "debugger help+quit" \
    "echo -e '$DEBUG_CMDS' | '$SIM_BIN' > /dev/null" 0

DEBUG_CMDS="regs\nquit\n"
run_test "debugger regs" \
    "echo -e '$DEBUG_CMDS' | '$SIM_BIN' > /dev/null" 0

DEBUG_CMDS="info r\nquit\n"
run_test "debugger info r" \
    "echo -e '$DEBUG_CMDS' | '$SIM_BIN' > /dev/null" 0

DEBUG_CMDS="b 0x1000\ninfo b\ndel 0\nquit\n"
run_test "debugger breakpoint add/list/del" \
    "echo -e '$DEBUG_CMDS' | '$SIM_BIN' > /dev/null" 0

DEBUG_CMDS="trace on\ntrace off\nquit\n"
run_test "debugger trace toggle" \
    "echo -e '$DEBUG_CMDS' | '$SIM_BIN' > /dev/null" 0

DEBUG_CMDS="x 0x0 16\nx 0x100=0xdeadbeef\nx 0x100 4\nquit\n"
run_test "debugger memory examine/write" \
    "echo -e '$DEBUG_CMDS' | '$SIM_BIN' > /dev/null" 0

DEBUG_CMDS="reset\nquit\n"
run_test "debugger reset" \
    "echo -e '$DEBUG_CMDS' | '$SIM_BIN' > /dev/null" 0

# ── 5. 异常测试 ───────────────────────────────────────────

echo ""
echo "[5] Error Handling Tests"
echo "──────────────────────────────────────────"

run_test "debugger unknown command" \
    "echo -e 'xyzzy\nquit\n' | '$SIM_BIN' > /dev/null" 0

run_test "debugger invalid address" \
    "echo -e 'x xyz\nquit\n' | '$SIM_BIN' > /dev/null" 0

run_test "debugger break without addr" \
    "echo -e 'b\nquit\n' | '$SIM_BIN' > /dev/null" 0

run_test "debugger del out of range" \
    "echo -e 'del 999\nquit\n' | '$SIM_BIN' > /dev/null" 0

# ── 汇总 ──────────────────────────────────────────────────

echo ""
echo "=========================================="
echo " Summary"
echo "=========================================="
echo "  Passed:  $PASSED"
echo "  Failed:  $FAILED"
echo "  Skipped: $SKIPPED"
echo ""

TOTAL=$((PASSED + FAILED))
if [[ $TOTAL -gt 0 ]]; then
    PERCENT=$((PASSED * 100 / TOTAL))
    echo "  Pass rate: $PERCENT% ($PASSED/$TOTAL)"
fi

if [[ $FAILED -gt 0 ]]; then
    echo ""
    echo "Some tests FAILED. Re-run with --verbose for details:"
    echo "  bash tests/run_tests.sh --verbose"
    exit 1
else
    echo ""
    echo "All executed tests passed."
    exit 0
fi
