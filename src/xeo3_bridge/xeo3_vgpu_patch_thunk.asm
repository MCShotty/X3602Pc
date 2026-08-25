option casemap:none

EXTERN VgpuCreatePlacedResourceLegacyColdBridge:PROC
EXTERN VgpuRecordNullPipelineState:PROC
EXTERN VgpuResolveEdramPipelineState:PROC
EXTERN VgpuNullPipelineStateNormalTarget:QWORD
EXTERN VgpuNullPipelineStateSkipTarget:QWORD

.code

PUBLIC VgpuCreatePlacedResourceLegacyColdThunk
VgpuCreatePlacedResourceLegacyColdThunk PROC
    mov rcx, r11
    jmp VgpuCreatePlacedResourceLegacyColdBridge
VgpuCreatePlacedResourceLegacyColdThunk ENDP

PUBLIC VgpuNullPipelineStateGuardThunk
VgpuNullPipelineStateGuardThunk PROC
    test rdx, rdx
    jz null_pipeline_state

    mov eax, DWORD PTR [rbx]
    shl rax, 5
    mov r10, QWORD PTR [rax+rbx+8]
    sub rsp, 30h
    mov r9, rdx
    mov r8, rbx
    mov rdx, r10
    mov rcx, rdi
    call VgpuResolveEdramPipelineState
    mov rdx, rax
    add rsp, 30h
    test rdx, rdx
    jz skip_pipeline_state

    mov QWORD PTR [rbx+1018h], rdx
    mov eax, DWORD PTR [rbx]
    shl rax, 5
    mov rcx, QWORD PTR [rax+rbx+8]
    jmp QWORD PTR [VgpuNullPipelineStateNormalTarget]

null_pipeline_state:
    mov eax, DWORD PTR [rbx]
    shl rax, 5
    mov r9, QWORD PTR [rbx+1018h]
    mov rdx, QWORD PTR [rax+rbx+8]
    mov rcx, rdi
    mov r8, rbx
    sub rsp, 20h
    call VgpuRecordNullPipelineState
    add rsp, 20h
skip_pipeline_state:
    mov BYTE PTR [rsi+37301h], 0
    jmp QWORD PTR [VgpuNullPipelineStateSkipTarget]
VgpuNullPipelineStateGuardThunk ENDP

END
