cmake_minimum_required(VERSION 3.10)

PROJECT(ULRE)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMCMakeModule)

set(ULRE_3RDPTY_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdpty)

include_directories(${ULRE_3RDPTY_ROOT_PATH}/NvTriStrip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

SET(ROOT_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/inc)
SET(ULRE_RUNTIME_PATH ${CMAKE_CURRENT_SOURCE_DIR})

include(math)
include(vulkan)
include(use_cm_module)

SET(ULRE    CMCore
            CMPlatform
            CMAssetsManage
            CMSceneGraph
            CMUtil
            CM2D
#            CMGUI
            ULRE.Work
            ULRE.Util
            ULRE.ShaderGen
            ULRE.SceneGraph
            ${HGL_MATH_LIB}
            ${RENDER_LIBRARY}
            ${Vulkan_LIBRARIES})

use_cm_module(Core)
use_cm_module(Util)
use_cm_module(2D)
use_cm_module(GUI)
use_cm_module(Platform)
use_cm_module(AssetsManage)
use_cm_module(SceneGraph)

add_subdirectory(src)

add_subdirectory(example)
