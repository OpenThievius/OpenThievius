# SPDX-License-Identifier: AGPL-3.0-or-later
cmake_minimum_required(VERSION 3.24)
project(OpenThieviusPort VERSION 0.1.0 LANGUAGES C CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(OPENTHIEVIUS_SOURCE_SLY1 "${CMAKE_CURRENT_SOURCE_DIR}/../../sly1-decomp" CACHE PATH
    "Path to the sibling sly1-decomp checkout")

# Native runtime modules are added here as their interfaces are established.
# Keep this configure target dependency-free so a clean checkout is inspectable.
add_custom_target(port_status
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/sly1.py status
            --source ${OPENTHIEVIUS_SOURCE_SLY1}
    COMMENT "Inspecting the configured Sly 1 decompilation")

add_custom_target(port_hostgen
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/sly1.py hostgen
            --source ${OPENTHIEVIUS_SOURCE_SLY1}
    COMMENT "Generating host-side C from matched Sly 1 sources")

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
    DEPENDS port_compile_generated sly1_port_runtime
    COMMENT "Linking the native Sly 1 launcher")

add_library(sly1_port_runtime STATIC
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
    runtime/host_env.cpp
    runtime/kernel.cpp
    runtime/sif.cpp
    runtime/memory.cpp
    runtime/ps2float.cpp
    sly1/build_info.cpp)
target_include_directories(sly1_port_runtime PUBLIC . runtime/include)
target_link_libraries(sly1_port_runtime PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
