cmake_minimum_required(VERSION 3.0)
SET(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE)

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

IF(WIN32)
	add_subdirectory(3rdpty/glfw)
	include_directories(3rdpty/glfw/include)

	SET(OPENGL_LIB	opengl32)
ELSE()
	SET(OPENGL_LIB	GL)
ENDIF()

add_definitions(-DMATH_USE_OPENGL)
add_definitions(-DMATH_RIGHTHANDED_CAMERA)
add_definitions(-DMATH_AVX)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/MathGeoLib/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/GLEWCore/inc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

SET(ROOT_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/inc)

add_subdirectory(3rdpty/MathGeoLib)
add_subdirectory(3rdpty/GLEWCore)
add_subdirectory(src)

SET(ULRE    ULRE.Base
            ULRE.RenderDevice
            ULRE.RenderDriver
            ULRE.Platform
            MathGeoLib
            GLEWCore
            glfw
            ${OPENGL_LIB})

add_subdirectory(example)
