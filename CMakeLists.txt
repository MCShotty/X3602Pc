cmake_minimum_required(VERSION 3.26)

project(xeo3_ac6_aot LANGUAGES CXX ASM_MASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(CTest)

set(
    AC6_EDRAM_TRANSFER_VERTEX_SHADER_DXIL
    "${CMAKE_CURRENT_SOURCE_DIR}/out/pix/ac6-vpos-exposure-gameplay-export-build/pso523-vs.dxil"
    CACHE FILEPATH
    "Pinned XeO3 EDRAM transfer vertex shader extracted from the local AC6 PIX replay"
)
set(
    AC6_PSO341_WIDTH_FIX_SHADER_DXIL
    "${CMAKE_CURRENT_SOURCE_DIR}/out/pix/pso341-cs-width-fix-rooted.dxil"
    CACHE FILEPATH
    "Pinned PSO341 compute shader with the signature-gated AC6 width fallback"
)

set(XEO3_PRIVATE_SHADER_ARTIFACTS_AVAILABLE OFF)
if(
    EXISTS "${AC6_EDRAM_TRANSFER_VERTEX_SHADER_DXIL}"
    AND EXISTS "${AC6_PSO341_WIDTH_FIX_SHADER_DXIL}"
)
    set(XEO3_PRIVATE_SHADER_ARTIFACTS_AVAILABLE ON)
elseif(
    EXISTS "${AC6_EDRAM_TRANSFER_VERTEX_SHADER_DXIL}"
    OR EXISTS "${AC6_PSO341_WIDTH_FIX_SHADER_DXIL}"
)
    message(FATAL_ERROR
        "Only one of the two required private AC6 shader artifacts is present. "
        "Provide both or neither."
    )
else()
    message(STATUS
        "Private AC6 shader artifacts are unavailable; VGPU patch tests and "
        "the XeO3 AOT target will be skipped"
    )
endif()

if(WIN32)
    add_executable(test_d3d12_shader_linkage
        tools/test-d3d12-shader-linkage.cpp
    )
    target_link_libraries(test_d3d12_shader_linkage PRIVATE d3d12 dxgi d3dcompiler)
    target_compile_options(test_d3d12_shader_linkage PRIVATE /W4 /WX)
    add_executable(ac6_xenos_shader_scan
        tools/ac6_xenos_shader_scan.cpp
    )
    target_link_libraries(ac6_xenos_shader_scan PRIVATE bcrypt)
    target_compile_options(ac6_xenos_shader_scan PRIVATE /W4 /WX)
endif()

if(XEO3_PRIVATE_SHADER_ARTIFACTS_AVAILABLE)
set(
    XEO3_DXC_EXECUTABLE
    "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/dxc.exe"
    CACHE FILEPATH
    "DXC used to build the AC6 EDRAM correction shaders"
)
if(NOT EXISTS "${XEO3_DXC_EXECUTABLE}")
    message(FATAL_ERROR "DXC was not found at ${XEO3_DXC_EXECUTABLE}")
endif()

set(
    AC6_EDRAM_SCALE_SHADER_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/xeo3_bridge/ac6_edram_scale_fix.hlsl"
)
set(
    AC6_EDRAM_SCALE_SHADER_DXIL
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ac6_edram_scale_fix.dxil"
)
set(
    AC6_EDRAM_SCALE_SHADER_INCLUDE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ac6_edram_scale_fix.dxil.inc"
)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(
    OUTPUT "${AC6_EDRAM_SCALE_SHADER_DXIL}"
    COMMAND
        "${XEO3_DXC_EXECUTABLE}"
        -T ps_6_0
        -E main
        -O3
        -Fo "${AC6_EDRAM_SCALE_SHADER_DXIL}"
        "${AC6_EDRAM_SCALE_SHADER_SOURCE}"
    DEPENDS "${AC6_EDRAM_SCALE_SHADER_SOURCE}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${AC6_EDRAM_SCALE_SHADER_INCLUDE}"
    COMMAND
        "${CMAKE_COMMAND}"
        "-DINPUT=${AC6_EDRAM_SCALE_SHADER_DXIL}"
        "-DOUTPUT=${AC6_EDRAM_SCALE_SHADER_INCLUDE}"
        "-DVARIABLE=kAc6EdramScaleFixPixelShader"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
    DEPENDS
        "${AC6_EDRAM_SCALE_SHADER_DXIL}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
    VERBATIM
)
add_custom_target(
    ac6_edram_scale_shader
    DEPENDS "${AC6_EDRAM_SCALE_SHADER_INCLUDE}"
)

set(
    AC6_EDRAM_LOAD_SHADER_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/xeo3_bridge/ac6_edram_load_fix.hlsl"
)
set(
    AC6_EDRAM_LOAD_SHADER_DXIL
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ac6_edram_load_fix.dxil"
)
set(
    AC6_EDRAM_LOAD_SHADER_INCLUDE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ac6_edram_load_fix.dxil.inc"
)
add_custom_command(
    OUTPUT "${AC6_EDRAM_LOAD_SHADER_DXIL}"
    COMMAND
        "${XEO3_DXC_EXECUTABLE}"
        -T ps_6_0
        -E main
        -O3
        -Fo "${AC6_EDRAM_LOAD_SHADER_DXIL}"
        "${AC6_EDRAM_LOAD_SHADER_SOURCE}"
    DEPENDS "${AC6_EDRAM_LOAD_SHADER_SOURCE}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${AC6_EDRAM_LOAD_SHADER_INCLUDE}"
    COMMAND
        "${CMAKE_COMMAND}"
        "-DINPUT=${AC6_EDRAM_LOAD_SHADER_DXIL}"
        "-DOUTPUT=${AC6_EDRAM_LOAD_SHADER_INCLUDE}"
        "-DVARIABLE=kAc6EdramLoadFixPixelShader"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
    DEPENDS
        "${AC6_EDRAM_LOAD_SHADER_DXIL}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
    VERBATIM
)
add_custom_target(
    ac6_edram_load_shader
    DEPENDS "${AC6_EDRAM_LOAD_SHADER_INCLUDE}"
)

file(SHA256 "${AC6_EDRAM_TRANSFER_VERTEX_SHADER_DXIL}"
    AC6_EDRAM_TRANSFER_VERTEX_SHADER_SHA256)
if(NOT AC6_EDRAM_TRANSFER_VERTEX_SHADER_SHA256 STREQUAL
        "7f3f8e0eec4028eccfbf2e63621b28049528ecfae2ecec5045b5d65e72cba81e")
    message(FATAL_ERROR
        "The pinned local EDRAM transfer vertex shader hash changed: "
        "${AC6_EDRAM_TRANSFER_VERTEX_SHADER_SHA256}"
    )
endif()
set(
    AC6_EDRAM_TRANSFER_VERTEX_SHADER_INCLUDE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ac6_edram_transfer_vs.dxil.inc"
)
add_custom_command(
    OUTPUT "${AC6_EDRAM_TRANSFER_VERTEX_SHADER_INCLUDE}"
    COMMAND
        "${CMAKE_COMMAND}"
        "-DINPUT=${AC6_EDRAM_TRANSFER_VERTEX_SHADER_DXIL}"
        "-DOUTPUT=${AC6_EDRAM_TRANSFER_VERTEX_SHADER_INCLUDE}"
        "-DVARIABLE=kAc6EdramTransferVertexShader"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
    DEPENDS
        "${AC6_EDRAM_TRANSFER_VERTEX_SHADER_DXIL}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
    VERBATIM
)
add_custom_target(
    ac6_edram_transfer_vertex_shader
    DEPENDS "${AC6_EDRAM_TRANSFER_VERTEX_SHADER_INCLUDE}"
)

file(SHA256 "${AC6_PSO341_WIDTH_FIX_SHADER_DXIL}"
    AC6_PSO341_WIDTH_FIX_SHADER_SHA256)
if(NOT AC6_PSO341_WIDTH_FIX_SHADER_SHA256 STREQUAL
        "158bf93b7d292dd46eb78ef2605dfafb9dedef1971e3634cdbf907f5794fef38")
    message(FATAL_ERROR
        "The pinned local PSO341 width-fix shader hash changed: "
        "${AC6_PSO341_WIDTH_FIX_SHADER_SHA256}"
    )
endif()
set(
    AC6_PSO341_WIDTH_FIX_SHADER_INCLUDE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ac6_pso341_width_fix.dxil.inc"
)
add_custom_command(
    OUTPUT "${AC6_PSO341_WIDTH_FIX_SHADER_INCLUDE}"
    COMMAND
        "${CMAKE_COMMAND}"
        "-DINPUT=${AC6_PSO341_WIDTH_FIX_SHADER_DXIL}"
        "-DOUTPUT=${AC6_PSO341_WIDTH_FIX_SHADER_INCLUDE}"
        "-DVARIABLE=kAc6Pso341WidthFixComputeShader"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
    DEPENDS
        "${AC6_PSO341_WIDTH_FIX_SHADER_DXIL}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
    VERBATIM
)
add_custom_target(
    ac6_pso341_width_fix_shader
    DEPENDS "${AC6_PSO341_WIDTH_FIX_SHADER_INCLUDE}"
)
endif()

option(
    XEO3_ENABLE_PROBE_TESTS
    "Build and register tests that require generated probe mappings"
    OFF
)
option(
    XEO3_CONTINUOUS_STATE_PUBLICATION
    "Publish every translated PPC register mutation into XeO3 CPU state"
    OFF
)

add_executable(debug_break_after
    tools/debug_break_after.cpp
)

add_executable(xeo3_mapping_dump
    tools/xeo3_mapping_dump.cpp
)

target_compile_definitions(debug_break_after
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

target_compile_options(debug_break_after PRIVATE /W4 /WX)
target_compile_definitions(xeo3_mapping_dump
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)
target_compile_options(xeo3_mapping_dump PRIVATE /W4 /WX)

set(
    AC6_GENERATED_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/out/ac6/generated"
    CACHE PATH
    "Directory containing ignored XenonRecomp AC6 output"
)

add_library(xeo3_ac6_probe EXCLUDE_FROM_ALL SHARED
    src/xeo3_contract/xeo3_contract.cpp
    src/xeo3_contract/xeo3_probe_thunk.asm
    src/xeo3_contract/xeo3_contract.def
)

target_include_directories(xeo3_ac6_probe
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_compile_definitions(xeo3_ac6_probe
    PRIVATE
        XEO3_CONTRACT_BUILD
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

target_compile_options(xeo3_ac6_probe
    PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W4 /WX /EHs-c- /GR->
)

target_link_options(xeo3_ac6_probe
    PRIVATE
        /DEBUG:FULL
        /INCREMENTAL:NO
)

set_target_properties(xeo3_ac6_probe PROPERTIES
    OUTPUT_NAME "xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6"
    PREFIX ""
)

if(BUILD_TESTING)
    if(XEO3_ENABLE_PROBE_TESTS)
        add_executable(xeo3_contract_tests
            tests/xeo3_contract_tests.cpp
        )
    endif()

    add_executable(xeo3_cpu_state_tests
        tests/xeo3_cpu_state_tests.cpp
    )

    add_executable(xeo3_mmio_endian_tests
        tests/xeo3_mmio_endian_tests.cpp
    )

    add_executable(xeo3_fiber_frame_tests
        tests/xeo3_fiber_frame_tests.cpp
    )

    add_executable(ac6_queue_wait_tests
        tests/ac6_queue_wait_tests.cpp
    )

    add_executable(ac6_audio_poll_tests
        tests/ac6_audio_poll_tests.cpp
    )

    add_executable(ac6_vd_swap_trace_tests
        tests/ac6_vd_swap_trace_tests.cpp
        src/xeo3_bridge/ac6_vd_swap_trace.cpp
    )
    target_include_directories(ac6_vd_swap_trace_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(ac6_vd_swap_trace_tests PRIVATE /W4 /WX)
    add_test(NAME ac6_vd_swap_trace_tests COMMAND ac6_vd_swap_trace_tests)

    add_executable(ac6_fast_critical_section_tests
        tests/ac6_fast_critical_section_tests.cpp
        src/xeo3_bridge/ac6_fast_critical_section.cpp
    )

    add_executable(ac6_fast_spin_lock_tests
        tests/ac6_fast_spin_lock_tests.cpp
        src/xeo3_bridge/ac6_fast_spin_lock.cpp
    )

    add_executable(ac6_fast_irql_tests
        tests/ac6_fast_irql_tests.cpp
        src/xeo3_bridge/ac6_fast_irql.cpp
    )

    add_executable(xeo3_native_dispatch_tests
        tests/xeo3_native_dispatch_tests.cpp
        tests/xeo3_native_dispatch_test_target.asm
        src/xeo3_bridge/xeo3_aot_thunk.asm
    )

    if(XEO3_PRIVATE_SHADER_ARTIFACTS_AVAILABLE)
        add_executable(xeo3_vgpu_patch_tests
            tests/xeo3_vgpu_patch_tests.cpp
            src/xeo3_bridge/ac6_g2h_trace.cpp
            src/xeo3_bridge/xeo3_vgpu_patch.cpp
            src/xeo3_bridge/xeo3_vgpu_patch_thunk.asm
        )
        add_executable(patch_xeo3_shader
            tools/patch-xeo3-shader.cpp
            src/xeo3_bridge/ac6_g2h_trace.cpp
            src/xeo3_bridge/xeo3_vgpu_patch.cpp
            src/xeo3_bridge/xeo3_vgpu_patch_thunk.asm
        )
        target_include_directories(patch_xeo3_shader PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_BINARY_DIR}/generated
        )
        add_dependencies(patch_xeo3_shader
            ac6_edram_scale_shader ac6_edram_load_shader
            ac6_edram_transfer_vertex_shader ac6_pso341_width_fix_shader
        )
        target_compile_options(patch_xeo3_shader PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/W4 /WX /EHsc>
        )
    endif()

    add_executable(xeo3_host_unmapped_observer_tests
        tests/xeo3_host_unmapped_observer_tests.cpp
        src/xeo3_bridge/xeo3_host_unmapped_observer.cpp
        src/xeo3_bridge/xeo3_host_unmapped_observer_thunk.asm
    )

    add_executable(ac6_sync_dvd_io_tests
        tests/ac6_sync_dvd_io_tests.cpp
        src/xeo3_bridge/ac6_sync_dvd_io.cpp
    )

    if(XEO3_ENABLE_PROBE_TESTS)
        target_include_directories(xeo3_contract_tests
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
    endif()

    target_include_directories(xeo3_cpu_state_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_include_directories(xeo3_mmio_endian_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_include_directories(xeo3_fiber_frame_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_include_directories(ac6_queue_wait_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_include_directories(ac6_audio_poll_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_include_directories(ac6_fast_critical_section_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_include_directories(ac6_fast_spin_lock_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_include_directories(ac6_fast_irql_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_include_directories(xeo3_native_dispatch_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    if(XEO3_PRIVATE_SHADER_ARTIFACTS_AVAILABLE)
        target_include_directories(xeo3_vgpu_patch_tests
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${CMAKE_CURRENT_BINARY_DIR}/generated
        )
        add_dependencies(
            xeo3_vgpu_patch_tests
            ac6_edram_scale_shader
            ac6_edram_load_shader
            ac6_edram_transfer_vertex_shader
            ac6_pso341_width_fix_shader
        )
    endif()

    target_include_directories(xeo3_host_unmapped_observer_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_include_directories(ac6_sync_dvd_io_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    if(XEO3_ENABLE_PROBE_TESTS)
        target_link_libraries(xeo3_contract_tests
            PRIVATE
                xeo3_ac6_probe
        )
    endif()

    if(XEO3_ENABLE_PROBE_TESTS)
        target_compile_options(xeo3_contract_tests PRIVATE /W4 /WX)
    endif()
    target_compile_options(xeo3_cpu_state_tests PRIVATE /W4 /WX)
    target_compile_options(xeo3_mmio_endian_tests PRIVATE /W4 /WX)
    target_compile_options(xeo3_fiber_frame_tests PRIVATE /W4 /WX)
    target_compile_options(ac6_queue_wait_tests PRIVATE /W4 /WX)
    target_compile_options(ac6_audio_poll_tests PRIVATE /W4 /WX)
    target_compile_options(ac6_fast_critical_section_tests PRIVATE /W4 /WX)
    target_compile_options(ac6_fast_spin_lock_tests PRIVATE /W4 /WX)
    target_compile_options(ac6_fast_irql_tests PRIVATE /W4 /WX)
    target_compile_options(xeo3_native_dispatch_tests
        PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/W4 /WX>
    )
    if(XEO3_PRIVATE_SHADER_ARTIFACTS_AVAILABLE)
        target_compile_options(xeo3_vgpu_patch_tests
            PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:/W4 /WX /EHsc>
        )
    endif()
    target_compile_options(xeo3_host_unmapped_observer_tests
        PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/W4 /WX /EHsc>
    )
    target_compile_options(ac6_sync_dvd_io_tests PRIVATE /W4 /WX)
    target_compile_definitions(ac6_sync_dvd_io_tests
        PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
    )
    if(XEO3_ENABLE_PROBE_TESTS)
        add_test(NAME xeo3_contract_tests COMMAND xeo3_contract_tests)
    endif()
    add_test(NAME xeo3_cpu_state_tests COMMAND xeo3_cpu_state_tests)
    add_test(NAME xeo3_mmio_endian_tests COMMAND xeo3_mmio_endian_tests)
    add_test(NAME xeo3_fiber_frame_tests COMMAND xeo3_fiber_frame_tests)
    add_test(NAME ac6_queue_wait_tests COMMAND ac6_queue_wait_tests)
    add_test(NAME ac6_audio_poll_tests COMMAND ac6_audio_poll_tests)
    add_test(
        NAME ac6_fast_critical_section_tests
        COMMAND ac6_fast_critical_section_tests
    )
    add_test(NAME ac6_fast_spin_lock_tests COMMAND ac6_fast_spin_lock_tests)
    add_test(NAME ac6_fast_irql_tests COMMAND ac6_fast_irql_tests)
    add_test(NAME xeo3_native_dispatch_tests COMMAND xeo3_native_dispatch_tests)
    if(XEO3_PRIVATE_SHADER_ARTIFACTS_AVAILABLE)
        add_test(NAME xeo3_vgpu_patch_tests COMMAND xeo3_vgpu_patch_tests)
    endif()
    add_test(
        NAME xeo3_host_unmapped_observer_tests
        COMMAND xeo3_host_unmapped_observer_tests
    )
    add_test(NAME ac6_sync_dvd_io_tests COMMAND ac6_sync_dvd_io_tests)
endif()

file(GLOB AC6_RECOMPILED_SOURCES CONFIGURE_DEPENDS
    "${AC6_GENERATED_DIR}/ppc_recomp.*.cpp"
)
if(AC6_RECOMPILED_SOURCES AND EXISTS "${AC6_GENERATED_DIR}/ppc_func_mapping.cpp")
    add_library(ac6_recompiled_code OBJECT
        ${AC6_RECOMPILED_SOURCES}
        "${AC6_GENERATED_DIR}/ppc_func_mapping.cpp"
    )

    target_include_directories(ac6_recompiled_code
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${AC6_GENERATED_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/XenonRecomp/thirdparty/simde"
    )

    target_compile_options(ac6_recompiled_code
        PRIVATE
            $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:/O2>
            /bigobj
            /W0
            -mmovbe
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/xeo3_bridge/xenonrecomp_overrides.h"
    )
    target_compile_definitions(ac6_recompiled_code
        PRIVATE
            XEO3_RUNTIME_TELEMETRY
    )
    if(XEO3_CONTINUOUS_STATE_PUBLICATION)
        target_compile_definitions(ac6_recompiled_code
            PRIVATE XEO3_CONTINUOUS_STATE_PUBLICATION
        )
    endif()

    # LLVM 22.1.8's GVN pass reproducibly crashes while optimizing AC6's
    # generated __restvmx_65 helper at /O2. Limit the workaround to whichever
    # generated shard contains that helper; all other shards retain /O2.
    foreach(ac6_recompiled_source IN LISTS AC6_RECOMPILED_SOURCES)
        file(STRINGS "${ac6_recompiled_source}" ac6_restvmx_65_match
            REGEX "PPC_FUNC_IMPL\\(__imp____restvmx_65\\)"
            LIMIT_COUNT 1
        )
        if(ac6_restvmx_65_match)
            set_property(SOURCE "${ac6_recompiled_source}" APPEND
                PROPERTY COMPILE_OPTIONS /O1
            )
            message(STATUS
                "Applying LLVM 22.1.8 GVN workaround to ${ac6_recompiled_source}"
            )
        endif()
        unset(ac6_restvmx_65_match)
    endforeach()

    set_property(GLOBAL PROPERTY JOB_POOLS ac6_codegen_pool=4)
    set_property(TARGET ac6_recompiled_code PROPERTY JOB_POOL_COMPILE ac6_codegen_pool)

    if(
        EXISTS "${AC6_GENERATED_DIR}/ac6_imports.inc"
        AND XEO3_PRIVATE_SHADER_ARTIFACTS_AVAILABLE
    )
        add_library(xeo3_ac6_aot EXCLUDE_FROM_ALL SHARED
            src/xeo3_bridge/ac6_vd_swap_trace.cpp
            src/xeo3_bridge/ac6_g2h_trace.cpp
            src/xeo3_bridge/ac6_fast_critical_section.cpp
            src/xeo3_bridge/ac6_fast_irql.cpp
            src/xeo3_bridge/ac6_fast_spin_lock.cpp
            src/xeo3_bridge/ac6_post_intro_probe.cpp
            src/xeo3_bridge/ac6_synchronous_queue.cpp
            src/xeo3_bridge/ac6_sync_dvd_io.cpp
            src/xeo3_bridge/xeo3_bridge.cpp
            src/xeo3_bridge/xeo3_host_unmapped_observer.cpp
            src/xeo3_bridge/xeo3_host_unmapped_observer_thunk.asm
            src/xeo3_bridge/xeo3_kernel_continuations.cpp
            src/xeo3_bridge/xeo3_state_sync.cpp
            src/xeo3_bridge/xeo3_vgpu_patch.cpp
            src/xeo3_bridge/xeo3_vgpu_patch_thunk.asm
            src/xeo3_bridge/xenonrecomp_runtime.cpp
            src/xeo3_bridge/xeo3_aot_thunk.asm
            src/xeo3_bridge/xeo3_aot.def
            $<TARGET_OBJECTS:ac6_recompiled_code>
        )

        target_include_directories(xeo3_ac6_aot
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${CMAKE_CURRENT_BINARY_DIR}/generated
                "${AC6_GENERATED_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/XenonRecomp/thirdparty/simde"
        )
        add_dependencies(
            xeo3_ac6_aot
            ac6_edram_scale_shader
            ac6_edram_load_shader
            ac6_edram_transfer_vertex_shader
            ac6_pso341_width_fix_shader
        )

        target_compile_definitions(xeo3_ac6_aot
            PRIVATE
            XEO3_CONTRACT_BUILD
            XEO3_RUNTIME_TELEMETRY
            WIN32_LEAN_AND_MEAN
                NOMINMAX
        )
        if(XEO3_CONTINUOUS_STATE_PUBLICATION)
            target_compile_definitions(xeo3_ac6_aot
                PRIVATE XEO3_CONTINUOUS_STATE_PUBLICATION
            )
        endif()

        target_compile_options(xeo3_ac6_aot
            PRIVATE
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>>:/O2>
                $<$<COMPILE_LANGUAGE:CXX>:/W4 /WX /EHsc /bigobj>
        )
        set_property(
            SOURCE
                src/xeo3_bridge/ac6_post_intro_probe.cpp
                src/xeo3_bridge/ac6_synchronous_queue.cpp
            APPEND PROPERTY COMPILE_OPTIONS -mmovbe
        )

        target_link_options(xeo3_ac6_aot
            PRIVATE
                /DEBUG:FULL
                /INCREMENTAL:NO
        )

        set_target_properties(xeo3_ac6_aot PROPERTIES
            OUTPUT_NAME "xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6"
            PREFIX ""
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot"
        )

        if(BUILD_TESTING)
            add_executable(ac6_generated_state_check
                tools/ac6_generated_state_check.cpp
            )
            target_compile_options(ac6_generated_state_check PRIVATE /W4 /WX)

            add_test(
                NAME ac6_generated_state_publication
                COMMAND
                    ac6_generated_state_check
                    "${AC6_GENERATED_DIR}"
            )

            add_executable(xeo3_state_sync_tests
                tests/xeo3_state_sync_tests.cpp
                src/xeo3_bridge/xeo3_state_sync.cpp
            )

            add_executable(xeo3_kernel_continuation_tests
                tests/xeo3_kernel_continuation_tests.cpp
                src/xeo3_bridge/xeo3_kernel_continuations.cpp
            )

            add_executable(xeo3_aot_contract_tests
                tests/xeo3_aot_contract_tests.cpp
            )

            target_include_directories(xeo3_state_sync_tests
                PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}/src
                    "${AC6_GENERATED_DIR}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/XenonRecomp/thirdparty/simde"
            )

            target_compile_options(xeo3_state_sync_tests PRIVATE /W4 /WX)
            if(XEO3_CONTINUOUS_STATE_PUBLICATION)
                target_compile_definitions(xeo3_state_sync_tests
                    PRIVATE XEO3_CONTINUOUS_STATE_PUBLICATION
                )
            endif()
            target_include_directories(xeo3_kernel_continuation_tests
                PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}/src
                    "${AC6_GENERATED_DIR}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/XenonRecomp/thirdparty/simde"
            )
            target_compile_options(
                xeo3_kernel_continuation_tests
                PRIVATE
                    /W4
                    /WX
                    -mmovbe
            )
            target_include_directories(xeo3_aot_contract_tests
                PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}/src
            )
            target_compile_options(xeo3_aot_contract_tests PRIVATE /W4 /WX)
            add_test(NAME xeo3_state_sync_tests COMMAND xeo3_state_sync_tests)
            add_test(
                NAME xeo3_kernel_continuation_tests
                COMMAND xeo3_kernel_continuation_tests
            )
        endif()
    endif()
endif()

set(
    XENONRECOMP_BUILD_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/build/xenonrecomp"
    CACHE PATH
    "Build directory containing the pinned XenonRecomp libraries"
)

set(XENONUTILS_LIBRARY "${XENONRECOMP_BUILD_ROOT}/XenonUtils/XenonUtils.lib")
set(XENON_DISASM_LIBRARY "${XENONRECOMP_BUILD_ROOT}/thirdparty/disasm/disasm.lib")
if(
    EXISTS "${XENONUTILS_LIBRARY}"
    AND EXISTS "${XENON_DISASM_LIBRARY}"
)
    add_executable(ac6_xex_symbols
        tools/ac6_xex_symbols.cpp
    )

    add_executable(ac6_xex_scan
        tools/ac6_xex_scan.cpp
    )

    add_executable(ac6_hidden_entry_scan
        tools/ac6_hidden_entry_scan.cpp
    )

    target_include_directories(ac6_xex_symbols
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/XenonRecomp/XenonUtils
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/XenonRecomp/thirdparty/simde
    )

    target_include_directories(ac6_xex_scan
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/XenonRecomp/XenonUtils
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/XenonRecomp/thirdparty/simde
    )

    target_include_directories(ac6_hidden_entry_scan
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/XenonRecomp/XenonUtils
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/XenonRecomp/thirdparty/disasm
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/XenonRecomp/thirdparty/simde
    )

    target_link_libraries(ac6_xex_symbols
        PRIVATE
            "${XENONUTILS_LIBRARY}"
            "${XENON_DISASM_LIBRARY}"
    )

    target_link_libraries(ac6_xex_scan
        PRIVATE
            "${XENONUTILS_LIBRARY}"
            "${XENON_DISASM_LIBRARY}"
    )

    target_link_libraries(ac6_hidden_entry_scan
        PRIVATE
            "${XENONUTILS_LIBRARY}"
            "${XENON_DISASM_LIBRARY}"
    )

    target_compile_options(ac6_xex_symbols PRIVATE /W4 /WX)
    target_compile_options(ac6_xex_scan PRIVATE /W4 /WX)
    target_compile_options(ac6_hidden_entry_scan PRIVATE /W4 /WX)
endif()
