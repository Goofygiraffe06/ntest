; ntest.inc — tiny nasm unit test library for x86-32 linux

%ifndef NTEST_INC
%define NTEST_INC

SECTION     .data
_nt_pass_lbl    db  'PASS ', 0
_nt_fail_lbl    db  'FAIL ', 0
_nt_slash       db  '/', 0
_nt_ok_str      db  ' passed', 10, 0
_nt_nl          db  10, 0

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


; _nt_do_exit — print "n/m passed" then exit
; exit code 0 if all passed, 1 if any failed
_nt_do_exit:
        mov     edi,    [_nt_passed]
        call    _nt_putu32          ; print passed count
        mov     edi,    _nt_slash
        call    _nt_puts            ; print "/"
        mov     edi,    [_nt_total]
        call    _nt_putu32          ; print total count
        mov     edi,    _nt_ok_str
        call    _nt_puts            ; print " passed\n"

        mov     eax,    [_nt_passed]
        cmp     eax,    [_nt_total]
        setne   cl                  ; cl = 1 if any test failed
        movzx   ebx,    cl          ; ebx = exit code
        mov     eax,    1           ; sys_exit
        int     80h

; _nt_id is a compile-time counter — each test block gets a unique number
; stamped into its labels so they never collide

%assign _nt_id 0

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
; b can be a register or an immediate
%macro assert_eq 2
        cmp     %1,     %2
        jne     _nt_fail_%[_nt_id]  ; jump to this test's fail label
%endmacro

; assert_ne — fail the current test if a == b
%macro assert_ne 2
        cmp     %1,     %2
        je      _nt_fail_%[_nt_id]
%endmacro

; endtest — close a test block
; pass path: print result, increment passed, jump over fail block
; fail path: _nt_fail_N is where assert_eq lands on mismatch
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

_nt_fail_%[_nt_id]:                 ; assert_eq jumps here on mismatch
        mov     edi,    _nt_fail_lbl
        call    _nt_puts
        mov     edi,    _nt_name_%[_nt_id]
        call    _nt_puts
        mov     edi,    _nt_nl
        call    _nt_puts            ; no increment to _nt_passed

_nt_done_%[_nt_id]:                 ; both paths land here
%endmacro

; ntest_exit — print summary and exit. use instead of a raw sys_exit
%macro ntest_exit 0
        call    _nt_do_exit
%endmacro

%endif ; ntest_inc
