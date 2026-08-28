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
# CMQT 链接 Qt6 的 Core/Gui/Widgets/Svg/Xml，需要配套的 llvm-mingw 工具链与 Qt 套件
# （本机：D:/Qt/6.11.2/llvm-mingw_64 + D:/Qt/Tools/llvm-mingw1706_64），
# 与 MSVC 主构建 ABI 不兼容，故默认关闭；Qt 构建请使用 CMakePresets.json 的 windows-qt-* preset。
option(ULRE_ENABLE_QT "Build the CMQT (Qt6) module" OFF)

set(CM_QT_MAJOR_VERSION 6)
set(QT_MIN_VERSION "6.5")

if(ULRE_ENABLE_QT)
    # 未显式指定 Qt 路径时，按编译器探测本机 Qt6 套件：
    #   MSVC           → msvc2022_64（官方 msvc 套件，MSVC2015+ 二进制兼容，VS2026 可链接）
    #   clang/MinGW    → llvm-mingw_64
    if(NOT CMAKE_PREFIX_PATH)
        if(MSVC)
            set(_qt_candidates "D:/Qt/6.11.2/msvc2022_64" "C:/Qt/6.11.2/msvc2022_64")
        else()
            set(_qt_candidates "D:/Qt/6.11.2/llvm-mingw_64" "C:/Qt/6.11.2/llvm-mingw_64")
        endif()
        foreach(_qt_dir IN LISTS _qt_candidates)
            if(EXISTS "${_qt_dir}/lib/cmake/Qt6")
                list(APPEND CMAKE_PREFIX_PATH "${_qt_dir}")
                break()
            endif()
        endforeach()
    endif()

    use_cm_module(QT)
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

add_subdirectory(src)

add_subdirectory(example)
