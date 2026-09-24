%include "ntest.inc"

SECTION .data
testval     dd  7

SECTION .text
global _start

_start:
    test "add 1+1"
    mov     eax,    1
    add     eax,    1
    assert_eq eax, 2
    endtest

    test "add 2+2 != 5"
    mov     eax,    2
    add     eax,    2
    assert_ne eax, 5
    endtest

    test "sub 10-3"
    mov     eax,    10
    sub     eax,    3
    assert_eq eax, 7
    endtest

    test "mul 6*7"
    mov     eax,    6
    mov     ecx,    7
    mul     ecx
    assert_eq eax, 42
    endtest

    test "and 0xff & 0x0f"
    mov     eax,    0xff
    and     eax,    0x0f
    assert_eq eax, 0x0f
    endtest

    test "shl 1 << 8"
    mov     eax,    1
    shl     eax,    8
    assert_eq eax, 256
    endtest

    test "multiple asserts in one test"
    mov     eax,    10
    assert_eq eax, 10
    inc     eax
    assert_eq eax, 11
    assert_ne eax, 10
    endtest

    test "mem compare"
    mov     eax,    [testval]
    assert_eq_mem eax, [testval]
    mov     edx,    5
    assert_neq_mem edx, [testval]
    endtest

    test "this test fails"
    mov     eax,    1
    assert_eq eax, 99
    endtest

    ntest_exit
