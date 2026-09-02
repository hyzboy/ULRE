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

# Qt 支持（CMQT 模块）
# CMQT 链接 Qt6 的 Core/Gui/Widgets/Svg/Xml，Qt6 由 vcpkg 提供（find_package 自动处理）。
# 使用前请确保 vcpkg 已安装完整 Qt6：vcpkg install qt6-base qtsvg（x64-windows）
option(ULRE_ENABLE_QT "Build the CMQT (Qt6) module" OFF)

set(CM_QT_MAJOR_VERSION 6)
set(QT_MIN_VERSION "6.5")

if(ULRE_ENABLE_QT)
    set(ULRE_QT_MODULE CMQT)
endif()

SET(ULRE    CMCoreType
            CMMath
            CMCore
            CMAssetsManage
            CMUtil
            CM2D
            CMAudio
            ${ULRE_QT_MODULE}
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
use_cm_module(Audio)
use_cm_module(2D)
use_cm_module(GUI)
use_cm_module(AssetsManage)

# CMQT 依赖 CMCore 的头（hgl/platform/Window.h 等），须在其他 CM 模块之后引入
# （此时 CMCore/inc 已全局 include、CMCore target 已存在，CMQT 才能编译/链接 CMCore）
if(ULRE_ENABLE_QT)
    use_cm_module(QT)
endif()

add_subdirectory(src)

add_subdirectory(example)

# Qt + ULRE 集成示例（依赖 ULRE 主树 target 与 ${ULRE} 链接列表，
# 故须在 add_subdirectory(src)/example 之后引入；源码位于 CMQT/examples）
if(ULRE_ENABLE_QT AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/CMQT/examples/QtULREExample/CMakeLists.txt)
    add_subdirectory(CMQT/examples/QtULREExample)
endif()
