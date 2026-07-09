.section .text
.global _start
_start:
    li a0, 1
    la a1, prompt
    li a2, 7
    li a7, 64
    ecall

    la a1, input_buf
    li a2, 32
    li a0, 0
    li a7, 63
    ecall
    mv s0, a0

    li a0, 1
    la a1, prefix
    li a2, 11
    li a7, 64
    ecall

    li a0, 1
    la a1, input_buf
    mv a2, s0
    li a7, 64
    ecall

    li a0, 1
    la a1, brk_before_msg
    li a2, 12
    li a7, 64
    ecall

    li a0, 0
    li a7, 214
    ecall
    mv s1, a0

    li t0, 0x1000
    add a0, s1, t0
    li a7, 214
    ecall
    mv s2, a0

    li a0, 1
    la a1, brk_after_msg
    li a2, 12
    li a7, 64
    ecall

    li a0, 1
    mv a1, s1
    li a2, 11
    li a7, 64
    ecall

    li a0, 1
    la a1, brk_sep
    li a2, 4
    li a7, 64
    ecall

    li a0, 1
    mv a1, s2
    li a2, 11
    li a7, 64
    ecall

    li a0, 1
    la a1, newline
    li a2, 1
    li a7, 64
    ecall

    li a0, 0
    li a7, 93
    ecall

.section .rodata
prompt:
    .asciz "Input: "
prompt_len = . - prompt - 1
prefix:
    .asciz "You typed: "
prefix_len = . - prefix - 1
brk_before_msg:
    .asciz "brk before: "
brk_before_len = . - brk_before_msg - 1
brk_after_msg:
    .asciz "brk after : "
brk_after_len = . - brk_after_msg - 1
brk_sep:
    .asciz " -> "
brk_sep_len = . - brk_sep - 1
newline:
    .asciz "\n"
newline_len = . - newline - 1

.section .bss
    .align 4
input_buf:
    .skip 32
