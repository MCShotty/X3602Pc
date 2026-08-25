option casemap:none

EXTERN RecordHostUnmappedIar:PROC
EXTERN HostUnmappedFatalFormatTarget:QWORD
EXTERN HostUnmappedFatalMessage:QWORD
EXTERN HostUnmappedFatalTrapTarget:QWORD

.code

PUBLIC HostUnmappedIarObserverThunk
HostUnmappedIarObserverThunk PROC
    mov ecx, ebx
    mov rdx, rdi
    mov r8, r15
    call RecordHostUnmappedIar

    mov rdx, QWORD PTR [rdi+78h]
    mov rcx, QWORD PTR [HostUnmappedFatalMessage]
    call QWORD PTR [HostUnmappedFatalFormatTarget]
    jmp QWORD PTR [HostUnmappedFatalTrapTarget]
HostUnmappedIarObserverThunk ENDP

END
