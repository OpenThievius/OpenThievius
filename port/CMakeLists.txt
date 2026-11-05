# SPDX-License-Identifier: AGPL-3.0-or-later
cmake_minimum_required(VERSION 3.24)
project(OpenThieviusPort VERSION 0.1.0 LANGUAGES C CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(OPENTHIEVIUS_SOURCE_SLY1 "${CMAKE_CURRENT_SOURCE_DIR}/../../sly1-decomp" CACHE PATH
    "Path to the sibling sly1-decomp checkout")
option(OPENRAC_NATIVE_FUNCTION_TRACE
    "Instrument the direct host32 build to trace active game functions" OFF)
set(SLY1_NATIVE_TRACE_ARGUMENTS)
if(OPENRAC_NATIVE_FUNCTION_TRACE)
    list(APPEND SLY1_NATIVE_TRACE_ARGUMENTS --trace-functions)
endif()

# Native runtime modules are added here as their interfaces are established.
# SDL3 is required for the native window target; source inspection remains
# independent of the retail executable and generated code.
add_custom_target(port_status
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/sly1.py status
            --source ${OPENTHIEVIUS_SOURCE_SLY1}
    COMMENT "Inspecting the configured Sly 1 decompilation")

# Standalone native prototype for the Sly catalog and compressed resources.
# It deliberately does not depend on the experimental guest execution runtime.
add_executable(sly1_resource_inspect
    sly1/resource_inspect.cpp
    sly1/resource_loader.cpp)

add_custom_target(port_hostgen
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/sly1.py hostgen
            --asm off
            --source ${OPENTHIEVIUS_SOURCE_SLY1}
    COMMENT "Generating host-side C from matched Sly 1 sources without retail assembly")

add_custom_target(port_compile_decomp_host32
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/compile_decomp_host32.py
            --source ${OPENTHIEVIUS_SOURCE_SLY1}
            --out ${CMAKE_CURRENT_BINARY_DIR}/host32
            --port ${CMAKE_CURRENT_SOURCE_DIR}
            --cxx ${CMAKE_CXX_COMPILER}
            ${SLY1_NATIVE_TRACE_ARGUMENTS}
    COMMENT "Compiling decompiled Sly C++ directly for the native 32-bit ABI")

add_custom_target(port_combine_decomp_host32
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/combine_decomp_host32.py
            --objects ${CMAKE_CURRENT_BINARY_DIR}/host32/objects
            --out ${CMAKE_CURRENT_BINARY_DIR}/host32/link
            --functions ${OPENTHIEVIUS_SOURCE_SLY1}/config/us/functions.tsv
            --nm ${CMAKE_NM}
            --ld ${CMAKE_LINKER}
            --cxx ${CMAKE_CXX_COMPILER}
    DEPENDS port_compile_decomp_host32
    COMMENT "Combining native C++ objects using retail function and data addresses")

add_custom_target(port_link_native_host32
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/link_native_host32.py
            --object ${CMAKE_CURRENT_BINARY_DIR}/host32/link/decomp-native32.o
            --aliases ${CMAKE_CURRENT_BINARY_DIR}/host32/link/retail-address-aliases.ld
            --out ${CMAKE_CURRENT_BINARY_DIR}/host32/launcher
            --source ${CMAKE_CURRENT_SOURCE_DIR}/sly1/native_host32
            --cxx ${CMAKE_CXX_COMPILER}
            --ld ${CMAKE_LINKER}
            --nm ${CMAKE_NM}
    DEPENDS port_combine_decomp_host32
    COMMENT "Linking freestanding Linux/i386 native Sly launcher")

add_custom_target(port_compile_generated
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/compile_hostgen.py
            --generated ${CMAKE_CURRENT_BINARY_DIR}/hostgen/us
            --out ${CMAKE_CURRENT_BINARY_DIR}/objects
            --runtime-includes ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include
    DEPENDS port_hostgen
    COMMENT "Compiling generated Sly 1 host code")

add_custom_target(port_link_generated
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/link_hostgen.py
            --generated ${CMAKE_CURRENT_BINARY_DIR}/hostgen/us
            --objects ${CMAKE_CURRENT_BINARY_DIR}/objects
            --runtime $<TARGET_FILE:sly1_port_runtime>
            --runtime-includes ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include
            --stub ${CMAKE_CURRENT_SOURCE_DIR}/sly1/link_check.cpp
            --out ${CMAKE_CURRENT_BINARY_DIR}/sly1_link_check
            --cxx ${CMAKE_CXX_COMPILER}
            --link-library=${CMAKE_DL_LIBS}
            --link-library=SDL3
    DEPENDS port_compile_generated sly1_port_runtime
    COMMENT "Linking generated Sly 1 code without running it")

add_custom_target(port_native
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/link_hostgen.py
            --generated ${CMAKE_CURRENT_BINARY_DIR}/hostgen/us
            --objects ${CMAKE_CURRENT_BINARY_DIR}/objects
            --runtime $<TARGET_FILE:sly1_port_runtime>
            --runtime-includes ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include
            --stub ${CMAKE_CURRENT_SOURCE_DIR}/sly1/main.cpp
            --out ${CMAKE_CURRENT_BINARY_DIR}/sly1_native
            --cxx ${CMAKE_CXX_COMPILER}
            --link-library=${CMAKE_DL_LIBS}
            --link-library=SDL3
    DEPENDS port_compile_generated sly1_port_runtime
    COMMENT "Linking the native Sly 1 launcher")

add_library(sly1_port_runtime STATIC
    renderer/direct.cpp
    renderer/framebuffer.cpp
    renderer/gl.cpp
    renderer/gl_warmup.cpp
    renderer/program_cache.cpp
    renderer/render_opt.cpp
    renderer/shader.cpp
    renderer/texture.cpp
    renderer/texture_pack.cpp
    renderer/texture_pool.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/renderer_shaders.cpp
    common/breadcrumbs.cpp
    common/cache_folder.cpp
    common/crash.cpp
    common/crash_symbols.cpp
    common/env.cpp
    common/json.cpp
    common/log.cpp
    common/parallel.cpp
    common/perf_capture.cpp
    common/precise_wait.cpp
    common/profiler.cpp
    common/process.cpp
    common/ps2_float.cpp
    common/sampler.cpp
    common/slow_cpu.cpp
    common/stats.cpp
    common/system_info.cpp
    common/zip_writer.cpp
    runtime/crash.cpp
    runtime/dma.cpp
    runtime/disc_image.cpp
    runtime/ee.cpp
    runtime/elf.cpp
    runtime/guest.cpp
    runtime/gs_dma.cpp
    runtime/gs_stream.cpp
    runtime/host_env.cpp
    runtime/kernel.cpp
    runtime/sif.cpp
    runtime/sly1_timer.cpp
    runtime/sly1_bootstrap.cpp
    runtime/sly1_format.cpp
    game/sly1-us/hand/native/sly1_native_worker.cpp
    runtime/vif1_dma.cpp
    runtime/vu1_native.cpp
    runtime/sdl_renderer.cpp
    runtime/memory.cpp
    runtime/ps2float.cpp
    platform/audio.cpp
    platform/clock.cpp
    platform/sly_input.cpp
    platform/gpu_driver.cpp
    platform/pad.cpp
    platform/window.cpp
    sly1/build_info.cpp)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/renderer_shaders.h ${CMAKE_CURRENT_BINARY_DIR}/renderer_shaders.cpp
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_renderer_shaders.py
            --out ${CMAKE_CURRENT_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/renderer/shaders/direct.vert
            ${CMAKE_CURRENT_SOURCE_DIR}/renderer/shaders/direct.frag
            ${CMAKE_CURRENT_SOURCE_DIR}/renderer/shaders/upscaled_texture_colours.glsl
    DEPENDS tools/embed_renderer_shaders.py renderer/shaders/direct.vert renderer/shaders/direct.frag
            renderer/shaders/upscaled_texture_colours.glsl
    VERBATIM)
target_include_directories(sly1_port_runtime PUBLIC . runtime/include ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(sly1_port_runtime PUBLIC Threads::Threads SDL3::SDL3 ${CMAKE_DL_LIBS})

# A separate 32-bit build can run the decompiled C++ against retail-address
# memory and OpenRAC's native SDL/GS renderer. The normal 64-bit hostgen
# launcher remains available in the default build.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_compile_definitions(sly1_port_runtime PRIVATE OPENRAC_SLY1_DIRECT_RETAIL_MEMORY)
    add_custom_target(port_native_sdl_host32
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/link_native_sdl_host32.py
                --object ${CMAKE_CURRENT_BINARY_DIR}/host32/link/decomp-native32.o
                --runtime $<TARGET_FILE:sly1_port_runtime>
                --out ${CMAKE_CURRENT_BINARY_DIR}/host32/sdl-launcher
                --source ${CMAKE_CURRENT_SOURCE_DIR}/sly1/native_host32
                --port ${CMAKE_CURRENT_SOURCE_DIR}
                --runtime-includes ${CMAKE_CURRENT_BINARY_DIR}
                --cxx ${CMAKE_CXX_COMPILER}
                --cxxflags "${CMAKE_CXX_FLAGS}"
        DEPENDS port_combine_decomp_host32 sly1_port_runtime
        COMMENT "Linking native 32-bit Sly with the SDL3 GS renderer")
endif()
