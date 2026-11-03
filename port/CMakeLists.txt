# SPDX-License-Identifier: AGPL-3.0-or-later
cmake_minimum_required(VERSION 3.24)
project(OpenThieviusPort VERSION 0.1.0 LANGUAGES C CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(OPENTHIEVIUS_SOURCE_SLY1 "${CMAKE_CURRENT_SOURCE_DIR}/../../sly1-decomp" CACHE PATH
    "Path to the sibling sly1-decomp checkout")

# Native runtime modules are added here as their interfaces are established.
# Keep this configure target dependency-free so a clean checkout is inspectable.
add_custom_target(port_status
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/sly1.py status
            --source ${OPENTHIEVIUS_SOURCE_SLY1}
    COMMENT "Inspecting the configured Sly 1 decompilation")
