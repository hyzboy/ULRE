cmake_minimum_required(VERSION 3.0)

PROJECT(ULRE)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMCMakeModule)

set(ULRE_3RDPTY_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdpty)

include(vulkan)

include(math)
use_mgl(${ULRE_3RDPTY_ROOT_PATH}/MathGeoLib)

include(use_cm_module)
use_cm_module(Core)
use_cm_module(Platform)
use_cm_module(AssetsManage)
use_cm_module(SceneGraph)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    include_directories(${ULRE_3RDPTY_ROOT_PATH}/jsoncpp/include)
    add_subdirectory(3rdpty/jsoncpp)
    SET(JSONCPP_LIBRARY jsoncpp_lib)
ELSE()
    SET(JSONCPP_LIBRARY jsoncpp)
ENDIF()

SET(ULRE    CMCore
            CMPlatform
            CMAssetsManage
            CMSceneGraph
            ULRE.Util
            ULRE.RenderDevice.Vulkan
            ULRE.SceneGraph
            MathGeoLib
            ${JSONCPP_LIBRARY}
            ${RENDER_LIBRARY}
            ${Vulkan_LIBRARIES})

include_directories(${ULRE_3RDPTY_ROOT_PATH}/NvTriStrip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

SET(ROOT_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/inc)

SET(ULRE_RUNTIME_PATH   ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)

add_subdirectory(example)
