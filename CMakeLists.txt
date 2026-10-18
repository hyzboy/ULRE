cmake_minimum_required(VERSION 3.28)
cmake_policy(VERSION 3.28)

PROJECT(ULRE)

include(CTest)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMCMakeModule)

set(ULRE_3RDPTY_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdpty)
set(ULRE_GLTF_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/GLTFConvert.ini" CACHE FILEPATH
    "GLTFConvert runtime configuration file")

include_directories(${ULRE_3RDPTY_ROOT_PATH}/NvTriStrip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

SET(ROOT_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/inc)
SET(ULRE_RUNTIME_PATH ${CMAKE_CURRENT_SOURCE_DIR})

include(common)
include(math)
include(vulkan)
include(use_cm_module)

# C++20 modules：全仓当前零使用（`export module` / `import` 各 0 处），而 compiler.cmake
# 无条件开启了模块扫描 —— 每个 target 多一遍 "Scanning sources for module dependencies..."，
# 并把 /interface /ifcOutput 撒进全局 CMAKE_CXX_FLAGS。这里在根处默认关掉，避免纯开销。
# 将来真要上 modules：cmake -DULRE_ENABLE_CXX_MODULES=ON
option(ULRE_ENABLE_CXX_MODULES "Enable C++20 modules scanning and /interface /ifcOutput flags" OFF)
if(NOT ULRE_ENABLE_CXX_MODULES)
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    string(REPLACE " /interface /ifcOutput ${CMAKE_CXX_MODULE_OUTPUT_DIRECTORY}" ""
        CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

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
            # HGL_MATH_LIB(glm::glm) 与 RENDER_LIBRARY(xcb, 仅 UNIX) 看似"与 ULRE 无关"，
            # 实为消费者获取 glm 头文件 / xcb 链接的唯一途径：add_cm_library 以 PRIVATE
            # 链接它们，不会传播给消费者。删掉会在"Vulkan SDK 不含 glm"或 Linux 上断链。
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
#use_cm_module(GUI)
use_cm_module(AssetsManage)

# CMQT 依赖 CMCore 的头（hgl/platform/Window.h 等），须在其他 CM 模块之后引入
# （此时 CMCore/inc 已全局 include、CMCore target 已存在，CMQT 才能编译/链接 CMCore）
if(ULRE_ENABLE_QT)
    use_cm_module(QT)
endif()

# ── ULRE::Engine：把上面的平铺链接列表收敛成一个具名聚合目标（CMake 阶段 3）──────────
# 消费方只写 `target_link_libraries(<t> PRIVATE ULRE::Engine)`，不必再展开 ${ULRE}。
#
# 这里只做「等价聚合」，不拆逐模块依赖。原因：ULRE.Vulkan / ULRE.ShaderGen /
# ULRE.SceneGraph / ULRE.ECS 四者两两互相 include 对方头文件，构成一个强连通分量，
# 不存在可声明的有向依赖图；要拆必须先做架构级解环，不属于构建系统改造范围。
# 证据见 doc/build-and-module-structure-audit-2026-10.md「ULRE 模块依赖是强连通分量」。
add_library(ULRE_Engine INTERFACE)
target_link_libraries(ULRE_Engine INTERFACE ${ULRE})
add_library(ULRE::Engine ALIAS ULRE_Engine)

add_subdirectory(src)

add_subdirectory(example)

# Qt + ULRE 集成示例（依赖 ULRE 主树 target 与 ${ULRE} 链接列表，
# 故须在 add_subdirectory(src)/example 之后引入；源码位于 CMQT/examples）
if(ULRE_ENABLE_QT AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/CMQT/examples/QtULREExample/CMakeLists.txt)
    add_subdirectory(CMQT/examples/QtULREExample)
endif()
