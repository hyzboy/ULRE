cmake_minimum_required(VERSION 3.28)
cmake_policy(VERSION 3.28)

PROJECT(ULRE)

# 禁止 in-source 构建：根目录曾残留 CMakeCache.txt / CMakeFiles/（CMAKE_HOME_DIRECTORY 指向早已失效的
# D:/ULRE），且 in-source 配置会把生成物混进源码树。请配置到独立目录，例如 build/。
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source build is not allowed. Configure into a separate directory, e.g.:\n"
        "    cmake -S . -B build -G \"Visual Studio 18 2026\" -A x64")
endif()

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

# ── 发行形态：是否把材质程序构建与阶段编译接入开发引擎 ─────────────────────────────
# ULRE_ENABLE_SHADERGEN 保留为既有配置名。ON（默认）构建 src/ShaderProgramBuild、
# 回归工具和示例；OFF 则只保留读取预烘焙 SPV 的运行时。
# C++ 侧同名宏登记在 inc/hgl/common/RenderOptions.h §2b，值域只有 0/1，判断写 `#if`。
option(ULRE_ENABLE_SHADERGEN "Build and link material program compilation support" ON)

if(NOT ULRE_ENABLE_SHADERGEN)
    # 全工程统一下发 0（不按配置区分），避免同一头在不同 TU 看到不同值。
    add_compile_definitions(ULRE_ENABLE_SHADERGEN=0)
endif()

# ULRE_RUNTIME：运行时真正需要的一切，**不含** ULRE.ShaderProgramBuild。
SET(ULRE_RUNTIME
            CMCoreType
            CMMath
            CMCore
            CMAssetsManage
            CMUtil
            CM2D
            CMAudio
            ${ULRE_QT_MODULE}
#            CMGUI
            ULRE.Debug
            ULRE.Work
            ULRE.ECS
            ULRE.Vulkan
            ULRE.ShaderRuntimeContract
            ULRE.ShaderMaterialData
            ULRE.ShaderCompiler
            ULRE.SceneGraph
            ULRE.InlineGeometry
            # HGL_MATH_LIB(glm::glm) 与 RENDER_LIBRARY(xcb, 仅 UNIX) 看似"与 ULRE 无关"，
            # 实为消费者获取 glm 头文件 / xcb 链接的唯一途径：add_cm_library 以 PRIVATE
            # 链接它们，不会传播给消费者。删掉会在"Vulkan SDK 不含 glm"或 Linux 上断链。
            ${HGL_MATH_LIB}
            ${RENDER_LIBRARY}
            ${Vulkan_LIBRARIES}
)

# ULRE_PROGRAM_BUILD：材质程序组装/阶段编译侧，只在 ULRE_ENABLE_SHADERGEN=ON 时存在。
#
# **必须整库链接（WHOLE_ARCHIVE），不能用普通静态库链接。**
# Windows 下 add_cm_library 产出的是静态库，链接器只从 .lib 中取「被引用符号」所在的 .obj。
# 生命周期接线的唯一入口是 compile/ShaderBuildBackendLifecycle.cpp 末尾那个 TU 级静态注册
# 对象（构造时调用 SetShaderCompilerLifecycleHooks），它没有任何对外符号，应用也不会引用
# 它所在 TU 的其它函数 ⇒ 整个 TU 被链接器丢弃 ⇒ 生命周期钩子恒为空 ⇒
# GetShaderProgramBuildBackend() 返回 nullptr ⇒ 材质构建退化到 manifest 后端（开发树里
# 没有烘焙缓存，必然 miss，所有图元因 "unresolved runtime pipeline" 被跳过）。
# 实证：SimpleSphere.exe 中搜不到 AssembleMaterialProgram / options.glsl，
# 却能搜到 "No shader program build backend installed"。
# 注意发行形态（ULRE_ENABLE_SHADERGEN=OFF）没有这个目标，钩子保持为空是**正确**行为：
# 那时由 EnsureShaderProgramManifestBackend() 装 manifest 后端读预烘焙缓存。
# 只对聚合入口做整库链接；直接 PRIVATE 链接 ULRE.ShaderProgramBuild 且确实调用了构建 API 的
# 工具/回归门不受影响（它们本来就按需取 TU）。
IF(ULRE_ENABLE_SHADERGEN)
    SET(ULRE_PROGRAM_BUILD "$<LINK_LIBRARY:WHOLE_ARCHIVE,ULRE.ShaderProgramBuild>")
ELSE()
    SET(ULRE_PROGRAM_BUILD "")
ENDIF()

SET(ULRE    ${ULRE_RUNTIME}
            ${ULRE_PROGRAM_BUILD}
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

# ── ULRE::Engine / ULRE::Runtime：把上面的平铺链接列表收敛成具名聚合目标（CMake 阶段 3）──
# 消费方只写 `target_link_libraries(<t> PRIVATE ULRE::Engine)`，不必再展开 ${ULRE}。
#
# **ULRE::Runtime 是发行版的唯一入口**：与 ULRE::Engine 完全相同，只是**永不含**
# ULRE.ShaderProgramBuild。开发机上的工具/示例仍可继续用 ULRE::Engine。
#
# 这里只做「等价聚合」，不拆逐模块依赖。原因：ULRE.Vulkan / ULRE.ShaderProgramBuild /
# ULRE.SceneGraph / ULRE.ECS 四者两两互相 include 对方头文件，构成一个强连通分量，
# 不存在可声明的有向依赖图；要拆必须先做架构级解环，不属于构建系统改造范围。
# 证据见 doc/build-and-module-structure-audit-2026-10.md「ULRE 模块依赖是强连通分量」。
add_library(ULRE_Runtime INTERFACE)
target_link_libraries(ULRE_Runtime INTERFACE ${ULRE_RUNTIME})
add_library(ULRE::Runtime ALIAS ULRE_Runtime)

add_library(ULRE_Engine INTERFACE)
target_link_libraries(ULRE_Engine INTERFACE ${ULRE})
add_library(ULRE::Engine ALIAS ULRE_Engine)

add_subdirectory(src)

# example/ 示例需要材质程序构建能力（MaterialDefinitionRegistry / 模板解析等），
# 发行形态（ULRE_ENABLE_SHADERGEN=OFF）下没有它们可链接的目标，故一并退出构建。
IF(ULRE_ENABLE_SHADERGEN)
    add_subdirectory(example)
ENDIF()

# Qt + ULRE 集成示例（依赖 ULRE 主树 target 与 ${ULRE} 链接列表，
# 故须在 add_subdirectory(src)/example 之后引入；源码位于 CMQT/examples）
if(ULRE_ENABLE_QT AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/CMQT/examples/QtULREExample/CMakeLists.txt)
    add_subdirectory(CMQT/examples/QtULREExample)
endif()
