#!/usr/bin/env bash
# ============================================================
# RISC-V 模拟器自动化测试脚本
# 用法: bash tests/run_tests.sh [--verbose] [--filter <name>]
#
# 先 cmake 构建，再跑此脚本：
#   cd build && cmake .. -G "MinGW Makefiles" && cmake --build .
#   bash ../tests/run_tests.sh --verbose
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

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

run_exe_test() {
    local name="$1"
    local exe="$BUILD_DIR/$2"

    if [[ -n "$FILTER" && "$name" != *"$FILTER"* ]]; then
        SKIPPED=$((SKIPPED + 1))
        return
    fi

    if [[ ! -f "$exe" ]] && [[ ! -f "$exe.exe" ]]; then
        # try with .exe extension
        if [[ -f "$exe.exe" ]]; then
            exe="$exe.exe"
        else
            echo "  SKIP  $name (binary not found: $exe — build first)"
            SKIPPED=$((SKIPPED + 1))
            return
        fi
    fi

    if [[ "$VERBOSE" -eq 1 ]]; then
        echo "── $name ──"
        echo "  RUN: $exe"
    fi

    if "$exe" > /dev/null 2>&1; then
        PASSED=$((PASSED + 1))
        echo "  PASS  $name"
    else
        FAILED=$((FAILED + 1))
        echo "  FAIL  $name (exit code: $?)"
        if [[ "$VERBOSE" -eq 1 ]]; then
            "$exe" 2>&1 || true
        fi
    fi
}

run_pipe_test() {
    local name="$1"
    local input="$2"
    local exe="$BUILD_DIR/$3"

    if [[ -n "$FILTER" && "$name" != *"$FILTER"* ]]; then
        SKIPPED=$((SKIPPED + 1))
        return
    fi

    if [[ ! -f "$exe" ]] && [[ ! -f "$exe.exe" ]]; then
        if [[ -f "$exe.exe" ]]; then
            exe="$exe.exe"
        else
            echo "  SKIP  $name (binary not found: $exe)"
            SKIPPED=$((SKIPPED + 1))
            return
        fi
    fi

    if printf '%b' "$input" | "$exe" > /dev/null 2>&1; then
        PASSED=$((PASSED + 1))
        echo "  PASS  $name"
    else
        FAILED=$((FAILED + 1))
        local actual=$?
        echo "  FAIL  $name (exit code: $actual)"
        if [[ "$VERBOSE" -eq 1 ]]; then
            printf '%b' "$input" | "$exe" 2>&1 || true
        fi
    fi
}

# ── 1. 调试器测试 ────────────────────────────────────────

echo "[1] Debugger Tests (C)"
echo "──────────────────────────────────────────"

run_exe_test "debugger basic commands"     "riscv_debugger_test"
run_exe_test "debugger edge/error cases"   "riscv_debugger_edges_test"
run_exe_test "breakpoint add/del/has"      "riscv_breakpoint_test"

# 交互式验证
run_pipe_test "debugger shell help+quit" \
    "help\nquit\n" \
    "riscv_sim"

run_pipe_test "debugger shell regs+info" \
    "regs\ninfo r\ninfo b\nquit\n" \
    "riscv_sim"

run_pipe_test "debugger shell breakpoints" \
    "b 0x1012c\nb 0x10130\ninfo b\ndel 0\ninfo b\nquit\n" \
    "riscv_sim"

run_pipe_test "debugger shell memory" \
    "x 0x0 32\nx 0x100=0xdead\nx 0x100 4\nquit\n" \
    "riscv_sim"

run_pipe_test "debugger shell trace+reset" \
    "trace on\ntrace off\nreset\nquit\n" \
    "riscv_sim"

run_pipe_test "debugger shell errors" \
    "xyzzy\nb\nx\nx xyz\ndel\ndel 999\ninfo x\nquit\n" \
    "riscv_sim"

# ── 2. CPU 测试 ────────────────────────────────────────

echo ""
echo "[2] CPU Tests (A)"
echo "──────────────────────────────────────────"

run_exe_test "cpu instructions"   "riscv_cpu_test"
run_exe_test "syscall"           "riscv_syscall_test"
run_exe_test "syscall edges"     "riscv_syscall_edges_test"

# ── 3. 内存测试 ─────────────────────────────────────────

echo ""
echo "[3] Memory Tests (B)"
echo "──────────────────────────────────────────"

run_exe_test "memory read/write"  "riscv_memory_test"

# ── 4. ELF 加载器测试 ────────────────────────────────────

echo ""
echo "[4] ELF Loader Tests (B)"
echo "──────────────────────────────────────────"

run_exe_test "hello program"     "riscv_hello_test"
run_exe_test "elf edges"         "riscv_elf_loader_edges_test"
run_exe_test "illegal exit"      "riscv_illegal_exit_test"

# ── 5. 程序级测试 ────────────────────────────────────────

echo ""
echo "[5] Program Tests (A+B)"
echo "──────────────────────────────────────────"

run_exe_test "loop program"       "riscv_loop_test"
run_exe_test "recursion program"  "riscv_recursion_test"
run_exe_test "array program"      "riscv_array_test"

# ── 6. 冒烟测试 ─────────────────────────────────────────

echo ""
echo "[6] Smoke Test"
echo "──────────────────────────────────────────"

run_exe_test "smoke test" "riscv_smoke_test"

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
    echo "  Pass rate: $PERCENT% ($PASSED/$TOTAL executed)"
fi

if [[ $FAILED -gt 0 ]]; then
    echo ""
    echo "Some tests FAILED. Re-run with --verbose:"
    echo "  bash tests/run_tests.sh --verbose"
    exit 1
else
    echo ""
    echo "All executed tests passed."
    exit 0
fi
