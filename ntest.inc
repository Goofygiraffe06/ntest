; ntest.inc — tiny nasm unit test library for x86-32 linux

%ifndef NTEST_INC
%define NTEST_INC

SECTION     .data
_nt_pass_lbl    db  'PASS ', 0
_nt_fail_lbl    db  'FAIL ', 0
_nt_slash       db  '/', 0
_nt_ok_str      db  ' passed', 10, 0
_nt_nl          db  10, 0
_nt_expect_str  db  ' expected ', 0
_nt_nexpect_str db  ' expected not ', 0
_nt_got_str     db  ' got ', 0

SECTION     .bss
_nt_total       resd    1       ; total tests run
_nt_passed      resd    1       ; total tests passed

SECTION     .text

; _nt_puts — print a nul-terminated string to stdout
; expects:  edi = pointer to string
; clobbers: eax, ebx, ecx, edx
_nt_puts:
        push    esi
        mov     esi,    edi     ; copy ptr to esi — ebx is needed by sys_write
        xor     edx,    edx     ; edx = 0 (length counter)

.scan:
        cmp     byte [esi + edx], 0 ; hit the nul terminator?
        je      .emit
        inc     edx             ; edx = length so far
        jmp     .scan

.emit:
        test    edx,    edx     ; skip syscall if string is empty
        jz      .ret
        mov     eax,    4       ; sys_write
        mov     ebx,    1       ; fd = stdout
        mov     ecx,    esi     ; buf = string pointer
                                ; edx = length (already set)
        int     80h

.ret:
        pop     esi
        ret


; _nt_putu32 — print a uint32 as a decimal number, no newline
; expects:  edi = number to print
; digits come out least-significant first so we fill a stack buffer backwards
_nt_putu32:
        push    ebx
        push    ebp
        sub     esp,    16          ; scratch buffer — 10 digits + nul fits here
        mov     eax,    edi         ; eax = number we're converting
        lea     ebp,    [esp + 15]
        mov     byte [ebp], 0       ; plant nul at end of buffer
        mov     ecx,    10          ; divisor

.digit:
        xor     edx,    edx         ; clear edx before div (div uses edx:eax)
        div     ecx                 ; eax = eax / 10,  edx = eax % 10
        dec     ebp                 ; move one byte left in buffer
        add     dl,     '0'         ; remainder → ASCII digit
        mov     [ebp],  dl          ; write digit into buffer
        test    eax,    eax         ; quotient zero means no more digits
        jnz     .digit

        mov     edi,    ebp         ; edi = pointer to first digit
        call    _nt_puts
        add     esp,    16          ; restore scratch buffer
        pop     ebp
        pop     ebx
        ret


; _nt_fail_diag — print "FAIL <name><relation><expected> got <actual>\n"
; expects:  eax = relation string (" expected " / " expected not ")
;           ecx = expected value, edx = actual value
;           edi = test name string
; clobbers: eax, ebx, ecx, edx, edi
; args are saved on the stack because _nt_puts/_nt_putu32 clobber eax..edx
_nt_fail_diag:
        push    edx                 ; save actual — popped last, sits at bottom
        push    ecx                 ; save expected
        push    eax                 ; save relation string
        push    edi                 ; save name — on top

        mov     edi,    _nt_fail_lbl
        call    _nt_puts            ; "FAIL "
        pop     edi                 ; test name
        call    _nt_puts

        pop     edi                 ; relation string
        call    _nt_puts            ; " expected " or " expected not "

        pop     edi                 ; expected value
        call    _nt_putu32

        mov     edi,    _nt_got_str
        call    _nt_puts            ; " got "

        pop     edi                 ; actual value
        call    _nt_putu32

        mov     edi,    _nt_nl
        call    _nt_puts            ; newline

        ret


; _nt_do_exit — print "n/m passed" then exit
; exit code = number of failed tests, capped at 255 (linux exit codes are 8-bit)
_nt_do_exit:
        mov     edi,    [_nt_passed]
        call    _nt_putu32          ; print passed count
        mov     edi,    _nt_slash
        call    _nt_puts            ; print "/"
        mov     edi,    [_nt_total]
        call    _nt_putu32          ; print total count
        mov     edi,    _nt_ok_str
        call    _nt_puts            ; print " passed\n"

        mov     eax,    [_nt_total]
        sub     eax,    [_nt_passed]   ; eax = number of failed tests
        cmp     eax,    255
        jbe     .cap                ; don't exceed linux's 8-bit exit range
        mov     eax,    255
.cap:
        mov     ebx,    eax         ; exit code
        mov     eax,    1           ; sys_exit
        int     80h

; _nt_id is a compile-time counter — each test block gets a unique number
; stamped into its labels so they never collide

%assign _nt_id 0

; _nt_assert_2 — shared guts of the value-comparing asserts
; compares %1 against %2; on mismatch jumps to the test's fail block with
;   eax = relation string, ecx = expected value, edx = actual value
; both values are stashed on the stack first because %1/%2 can be registers,
; immediates, or memory — the stack is the only storage that never aliases them
; expects:  %1 = operand to test (32-bit register)
;           %2 = expected operand (32-bit register, immediate, or memory)
;           %3 = jump-if-fail condition (je / jne)
;           %4 = relation string label for the failure message
; clobbers: nothing on the pass path, eax/ecx/edx on the failure path
%macro _nt_assert_2 4
        push    dword %2            ; save expected on stack (bottom)
        push    dword %1            ; save actual on stack (top)
        cmp     %1,     %2          ; test the real operands
        %3      %%fail              ; mismatch → diagnose
        add     esp,    8           ; matched → drop saved values
        jmp     %%done
%%fail:
        pop     edx                 ; edx = actual value
        pop     ecx                 ; ecx = expected value
        mov     eax,    %4          ; relation string
        jmp     _nt_fail_%[_nt_id]  ; fail block prints "FAIL name ..."
%%done:
%endmacro

; test — open a test block
; stores the name string in .data, bumps the total counter at runtime
%macro test 1
    %assign _nt_id _nt_id + 1
    SECTION .data
        _nt_name_%[_nt_id]: db %1, 0
    SECTION .text
    inc     dword [_nt_total]
%endmacro

; assert_eq — fail the current test if a != b
; b can be a register, immediate, or memory
%macro assert_eq 2
        _nt_assert_2 %1, %2, jne, _nt_expect_str
%endmacro

; assert_ne — fail the current test if a == b
%macro assert_ne 2
        _nt_assert_2 %1, %2, je, _nt_nexpect_str
%endmacro

; assert_eq_mem — fail the current test if register a != value at b
; b must be a memory operand, e.g. assert_eq_mem eax, [counter]
%macro assert_eq_mem 2
        _nt_assert_2 %1, %2, jne, _nt_expect_str
%endmacro

; assert_neq_mem — fail the current test if register a == value at b
%macro assert_neq_mem 2
        _nt_assert_2 %1, %2, je, _nt_nexpect_str
%endmacro

; endtest — close a test block
; pass path: print result, increment passed, jump over fail block
; fail path: _nt_fail_N is where the asserts land on mismatch
; first failure wins — rest of block is skipped
%macro endtest 0
        mov     edi,    _nt_pass_lbl
        call    _nt_puts
        mov     edi,    _nt_name_%[_nt_id]
        call    _nt_puts
        mov     edi,    _nt_nl
        call    _nt_puts
        inc     dword [_nt_passed]
        jmp     _nt_done_%[_nt_id]  ; skip the fail block below

_nt_fail_%[_nt_id]:                 ; asserts jump here on mismatch
        mov     edi,    _nt_name_%[_nt_id]
        call    _nt_fail_diag       ; eax = relation, ecx = expected, edx = actual
        jmp     _nt_done_%[_nt_id]  ; no increment to _nt_passed

_nt_done_%[_nt_id]:                 ; both paths land here
%endmacro

; ntest_exit — print summary and exit. use instead of a raw sys_exit
%macro ntest_exit 0
        call    _nt_do_exit
%endmacro

%endif ; ntest_inc
