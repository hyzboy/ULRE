cmake_minimum_required(VERSION 3.0)

PROJECT(ULRE)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(system_bit)
include(version)
include(compiler)
include(output_path)
include(FindVulkan)

check_system_bits()
check_system_version()
set_compiler_param()
set_output_directory()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR -DWIN32_LEAN_AND_MEAN)

    include_directories(${Vulkan_INCLUDE_DIRS})

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/jsoncpp/include)
    add_subdirectory(3rdpty/jsoncpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR)
elseif(UNIX)
    add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
    SET(RENDER_LIBRARY xcb)
else()
    message(FATAL_ERROR "Unsupported Platform!")
ENDIF()

add_definitions(-DMATH_USE_OPENGL)
add_definitions(-DMATH_RIGHTHANDED_CAMERA)
add_definitions(-DMATH_AVX)

SET(ULRE    ULRE.Base
            ULRE.Util
            ULRE.RenderDevice.Vulkan
            ULRE.SceneGraph
            ULRE.Platform
            MathGeoLib
            spirv-cross-core
            glslang
            jsoncpp_lib
            ${RENDER_LIBRARY}
            ${Vulkan_LIBRARIES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/glslang/glslang/Include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/assimp/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/MathGeoLib/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/NvTriStrip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

SET(ROOT_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/inc)
SET(SPIRV_CROSS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/SPIRV-Cross)

include_directories(${SPIRV_CROSS_PATH})
add_subdirectory(${SPIRV_CROSS_PATH})

SET(ULRE_RUNTIME_PATH   ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(3rdpty/glslang)
add_subdirectory(3rdpty/MathGeoLib)
add_subdirectory(3rdpty/NvTriStrip)
add_subdirectory(3rdpty/assimp)
add_subdirectory(src)

add_subdirectory(example)
