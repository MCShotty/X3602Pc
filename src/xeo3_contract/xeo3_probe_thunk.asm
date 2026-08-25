option casemap:none

EXTERN PrecompiledPointers:QWORD
EXTERN ProbeInvocationCount:QWORD
EXTERN ProbeLastCpuState:QWORD
EXTERN ProbeLastGuestMemory:QWORD
EXTERN ProbeLastGuestIar:DWORD

.code

PUBLIC XeO3ProbeThunk
XeO3ProbeThunk PROC
    lock inc qword ptr [ProbeInvocationCount]
    mov qword ptr [ProbeLastCpuState], rbx
    mov qword ptr [ProbeLastGuestMemory], r15
    mov dword ptr [ProbeLastGuestIar], ecx

    mov eax, 06BF0F910h
    mov qword ptr [rbx - 8], rax
    mov rcx, rax
    ret
XeO3ProbeThunk ENDP

END
