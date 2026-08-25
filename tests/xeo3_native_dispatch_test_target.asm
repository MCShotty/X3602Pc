option casemap:none

EXTERN XeO3DispatchTestObservedCpuState:QWORD
EXTERN XeO3DispatchTestObservedGuestMemory:QWORD
EXTERN XeO3DispatchTestObservedGuestTarget:DWORD
EXTERN XeO3DispatchTestObservedHostFence:QWORD
EXTERN XeO3DispatchTestInvocationCount:QWORD
EXTERN XeO3AotThunk:PROC

.code

PUBLIC XeO3DispatchTestTarget
XeO3DispatchTestTarget PROC
    mov qword ptr [XeO3DispatchTestObservedCpuState], rbx
    mov qword ptr [XeO3DispatchTestObservedGuestMemory], r15
    mov dword ptr [XeO3DispatchTestObservedGuestTarget], ecx
    mov qword ptr [XeO3DispatchTestObservedHostFence], r14
    inc qword ptr [XeO3DispatchTestInvocationCount]
    mov rbp, 1111111111111111h
    mov rsi, 2222222222222222h
    mov rdi, 3333333333333333h
    mov r12, 4444444444444444h
    mov r13, 5555555555555555h
    mov r14, 6666666666666666h
    pcmpeqd xmm6, xmm6
    pcmpeqd xmm7, xmm7
    pcmpeqd xmm8, xmm8
    pcmpeqd xmm9, xmm9
    pcmpeqd xmm10, xmm10
    pcmpeqd xmm11, xmm11
    pcmpeqd xmm12, xmm12
    pcmpeqd xmm13, xmm13
    pcmpeqd xmm14, xmm14
    pcmpeqd xmm15, xmm15
    ret
XeO3DispatchTestTarget ENDP

PUBLIC XeO3DispatchTestInvokeAotThunk
XeO3DispatchTestInvokeAotThunk PROC
    push rbx
    push r14
    push r15
    sub rsp, 20h

    mov rbx, rcx
    mov r15, rdx
    mov ecx, r8d
    mov r14, r9
    call XeO3AotThunk

    add rsp, 20h
    pop r15
    pop r14
    pop rbx
    ret
XeO3DispatchTestInvokeAotThunk ENDP

END
