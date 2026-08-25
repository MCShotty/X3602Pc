option casemap:none

EXTERN XeO3Dispatch:PROC

.code

PUBLIC XeO3AotThunk
XeO3AotThunk PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog

    mov r9, r14
    mov r8d, ecx
    mov rdx, r15
    mov rcx, rbx
    call XeO3Dispatch

    mov ecx, dword ptr [rbx - 8]
    add rsp, 40
    ret
XeO3AotThunk ENDP

PUBLIC XeO3CallMappedGuest
XeO3CallMappedGuest PROC FRAME
    push rbx
    .pushreg rbx
    push rbp
    .pushreg rbp
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 0C8h
    .allocstack 0C8h
    movdqa xmmword ptr [rsp + 20h], xmm6
    .savexmm128 xmm6, 20h
    movdqa xmmword ptr [rsp + 30h], xmm7
    .savexmm128 xmm7, 30h
    movdqa xmmword ptr [rsp + 40h], xmm8
    .savexmm128 xmm8, 40h
    movdqa xmmword ptr [rsp + 50h], xmm9
    .savexmm128 xmm9, 50h
    movdqa xmmword ptr [rsp + 60h], xmm10
    .savexmm128 xmm10, 60h
    movdqa xmmword ptr [rsp + 70h], xmm11
    .savexmm128 xmm11, 70h
    movdqa xmmword ptr [rsp + 80h], xmm12
    .savexmm128 xmm12, 80h
    movdqa xmmword ptr [rsp + 90h], xmm13
    .savexmm128 xmm13, 90h
    movdqa xmmword ptr [rsp + 0A0h], xmm14
    .savexmm128 xmm14, 0A0h
    movdqa xmmword ptr [rsp + 0B0h], xmm15
    .savexmm128 xmm15, 0B0h
    .endprolog

    mov rbx, rcx
    mov r15, rdx
    mov r14, r9
    mov eax, r8d
    mov rdx, qword ptr [rbx - 16]
    mov ecx, eax
    call qword ptr [rdx + rax * 2]

    movdqa xmm6, xmmword ptr [rsp + 20h]
    movdqa xmm7, xmmword ptr [rsp + 30h]
    movdqa xmm8, xmmword ptr [rsp + 40h]
    movdqa xmm9, xmmword ptr [rsp + 50h]
    movdqa xmm10, xmmword ptr [rsp + 60h]
    movdqa xmm11, xmmword ptr [rsp + 70h]
    movdqa xmm12, xmmword ptr [rsp + 80h]
    movdqa xmm13, xmmword ptr [rsp + 90h]
    movdqa xmm14, xmmword ptr [rsp + 0A0h]
    movdqa xmm15, xmmword ptr [rsp + 0B0h]
    add rsp, 0C8h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
XeO3CallMappedGuest ENDP

PUBLIC XeO3CallHostGuest
XeO3CallHostGuest PROC FRAME
    push rbx
    .pushreg rbx
    push rbp
    .pushreg rbp
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 0C8h
    .allocstack 0C8h
    movdqa xmmword ptr [rsp + 20h], xmm6
    .savexmm128 xmm6, 20h
    movdqa xmmword ptr [rsp + 30h], xmm7
    .savexmm128 xmm7, 30h
    movdqa xmmword ptr [rsp + 40h], xmm8
    .savexmm128 xmm8, 40h
    movdqa xmmword ptr [rsp + 50h], xmm9
    .savexmm128 xmm9, 50h
    movdqa xmmword ptr [rsp + 60h], xmm10
    .savexmm128 xmm10, 60h
    movdqa xmmword ptr [rsp + 70h], xmm11
    .savexmm128 xmm11, 70h
    movdqa xmmword ptr [rsp + 80h], xmm12
    .savexmm128 xmm12, 80h
    movdqa xmmword ptr [rsp + 90h], xmm13
    .savexmm128 xmm13, 90h
    movdqa xmmword ptr [rsp + 0A0h], xmm14
    .savexmm128 xmm14, 0A0h
    movdqa xmmword ptr [rsp + 0B0h], xmm15
    .savexmm128 xmm15, 0B0h
    .endprolog

    mov rax, r9
    mov rbx, rcx
    mov r15, rdx
    mov r14, qword ptr [rsp + 130h]
    mov ecx, r8d
    mov rdx, qword ptr [rbx - 16]
    call rax

    movdqa xmm6, xmmword ptr [rsp + 20h]
    movdqa xmm7, xmmword ptr [rsp + 30h]
    movdqa xmm8, xmmword ptr [rsp + 40h]
    movdqa xmm9, xmmword ptr [rsp + 50h]
    movdqa xmm10, xmmword ptr [rsp + 60h]
    movdqa xmm11, xmmword ptr [rsp + 70h]
    movdqa xmm12, xmmword ptr [rsp + 80h]
    movdqa xmm13, xmmword ptr [rsp + 90h]
    movdqa xmm14, xmmword ptr [rsp + 0A0h]
    movdqa xmm15, xmmword ptr [rsp + 0B0h]
    add rsp, 0C8h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
XeO3CallHostGuest ENDP

END
