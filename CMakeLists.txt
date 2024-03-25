cmake_minimum_required(VERSION 3.5)

PROJECT(ULRE)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMCMakeModule)

set(ULRE_3RDPTY_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdpty)

include(math)
include(vulkan)
include(use_cm_module)

use_cm_module(Core)
use_cm_module(Util)
use_cm_module(Platform)
use_cm_module(AssetsManage)
use_cm_module(SceneGraph)

SET(ULRE    CMCore
            CMPlatform
            CMAssetsManage
            CMSceneGraph
            CMUtil
            ULRE.Util
            ULRE.ShaderGen
            ULRE.SceneGraph
            ${HGL_MATH_LIB}
            ${RENDER_LIBRARY}
            ${Vulkan_LIBRARIES})

include_directories(${ULRE_3RDPTY_ROOT_PATH}/NvTriStrip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

SET(ROOT_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/inc)

SET(ULRE_RUNTIME_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)

add_subdirectory(example)
