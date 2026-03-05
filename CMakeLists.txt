cmake_minimum_required(VERSION 3.28)
cmake_policy(VERSION 3.28)

PROJECT(ULRE)

include(CTest)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMCMakeModule)

set(ULRE_3RDPTY_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdpty)

include_directories(${ULRE_3RDPTY_ROOT_PATH}/NvTriStrip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

SET(ROOT_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/inc)
SET(ULRE_RUNTIME_PATH ${CMAKE_CURRENT_SOURCE_DIR})

include(common)
include(math)
include(vulkan)
include(use_cm_module)

SET(ULRE    CMCoreType
            CMMath
            CMCore
            CMAssetsManage
            CMUtil
            CM2D
#            CMGUI
            ULRE.Work
            ULRE.ECS
            ULRE.Vulkan
            ULRE.ShaderGen
            ULRE.SceneGraph
            ULRE.InlineGeometry
            ${HGL_MATH_LIB}
            ${RENDER_LIBRARY}
            ${Vulkan_LIBRARIES}
)

use_cm_module(CoreType)
use_cm_module(Math)
use_cm_module(Core)
use_cm_module(Util)
use_cm_module(2D)
use_cm_module(GUI)
use_cm_module(AssetsManage)
add_subdirectory(src)

add_subdirectory(test)

add_subdirectory(example)

find_package(Python3 QUIET COMPONENTS Interpreter)

if(Python3_Interpreter_FOUND)
    add_custom_target(check_shadergen_boundary
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_shadergen_boundary.py --repo-root ${CMAKE_CURRENT_SOURCE_DIR} --allowlist-file ${CMAKE_CURRENT_SOURCE_DIR}/tools/shadergen_boundary_allowlist.txt
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running ShaderGen public-header boundary check"
        VERBATIM)

    if(BUILD_TESTING)
        add_test(
            NAME check_shadergen_boundary
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_shadergen_boundary.py --repo-root ${CMAKE_CURRENT_SOURCE_DIR} --allowlist-file ${CMAKE_CURRENT_SOURCE_DIR}/tools/shadergen_boundary_allowlist.txt)
    endif()
else()
    message(WARNING "Python3 interpreter not found; check_shadergen_boundary target is disabled")
endif()
