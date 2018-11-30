cmake_minimum_required(VERSION 3.0)

PROJECT(ULRE)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(system_bit)
include(version)
include(compiler)
include(output_path)

check_system_bits()
check_system_version()
set_compiler_param()
set_output_directory()

include_directories(3rdpty/MathGeoLib/src)
include_directories(3rdpty/GLEWCore/inc)
include_directories(inc)

add_subdirectory(3rdpty/MathGeoLib)
add_subdirectory(3rdpty/GLEWCore)
add_subdirectory(src)

SET(ULRE    ULRE.RenderDevice
            ULRE.RenderDriver
            MathGeoLib
            GLEWCore
            glfw
            GL)

add_subdirectory(example)
