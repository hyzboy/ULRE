cmake_minimum_required(VERSION 3.0)

PROJECT(ULRE)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMCMakeModule)

include(vulkan)

include(math)
use_mgl(${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/MathGeoLib)

include(use_cm_module)
use_cm_module(Core)
use_cm_module(Platform)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/jsoncpp/include)
    add_subdirectory(3rdpty/jsoncpp)
ENDIF()

SET(ULRE    CMCore
            CMPlatform
            ULRE.Util
            ULRE.Shader
            ULRE.RenderDevice.Vulkan
            ULRE.SceneGraph
            MathGeoLib
            spirv-cross-core
            SPIRV
            glslang
            jsoncpp_lib
            ${RENDER_LIBRARY}
            ${Vulkan_LIBRARIES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/assimp/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/NvTriStrip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

SET(ROOT_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/inc)
SET(SPIRV_CROSS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdpty/SPIRV-Cross)

include_directories(${SPIRV_CROSS_PATH})
add_subdirectory(${SPIRV_CROSS_PATH})

macro(glslang_pch SRCS PCHCPP)
    message("use null [glslang_pch] macro")
endmacro()

include_directories(${SPIRV_CROSS_PATH}/external/glslang/glslang/Public)
add_subdirectory(${SPIRV_CROSS_PATH}/external/glslang/glslang)

include_directories(${SPIRV_CROSS_PATH}/external/glslang)
add_subdirectory(${SPIRV_CROSS_PATH}/external/glslang/SPIRV)

if(WIN32)
add_subdirectory(${SPIRV_CROSS_PATH}/external/glslang/OGLCompilersDLL)
endif()

SET(ULRE_RUNTIME_PATH   ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(3rdpty/assimp)
add_subdirectory(src)

add_subdirectory(example)
