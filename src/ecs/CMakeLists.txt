SET(ECS_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/ecs)

# ECS 调试/校验面（对拍器 + 诊断打印）。**开关与默认值的唯一台帐 = inc/hgl/common/RenderOptions.h §2**：
# 本 option 只表达「在所有构建配置里**强制打开**」；默认 OFF = 不下发宏 ⇒ 由台帐按配置决定（Debug 开 / Release 关）。
# 想「全配置强制关」：配置时另加 `-DCMAKE_CXX_FLAGS=-DULRE_ECS_DEBUG_API=0`（罕见）。
option(ULRE_ECS_DEBUG_API "在所有构建配置里强制打开 ECS 调试/校验面（OFF = 交给 RenderOptions.h 台帐：Debug 开 / Release 关）" OFF)
option(ULRE_ENABLE_CPU_CULL_PARITY_TEST "注册 CPU 剔除参考台架（每 60 帧 GPU 读回对拍）；默认关，生产管线不付那笔 stall" OFF)

# ── 源文件清单：目录递归扫描 ──────────────────────────────────────────────────
# 阶段 2（CMake 模块化）改造：此前是 22 份手工清单（ECS_CORE_FILES、17 份
# ECS_SYS_*_FILES、4 份 ECS_SUPPORT*_FILES）共 260 行，增删一个 .cpp 都要手改，
# 且已经开始漂移 —— 实测有 10 个头文件被代码 include 却没进清单，IDE 里看不到。
# 改为按目录扫描：
#   - components/ core/ support/ systems/ 下的 .cpp 全部进库（tests/ 除外）；
#   - inc/hgl/ecs 下的 .h 全部列为工程文件（只为 IDE 组织，不参与编译）。
# 已核对：本目录扫描出的 .cpp 集合与旧清单**完全一致（67 个）**，编译产物不变。
# CONFIGURE_DEPENDS 让 cmake --build 在文件增删时自动重跑 configure，新增源文件
# 不必再手动 configure。
file(GLOB_RECURSE ECS_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/components/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/support/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/*.cpp
)
file(GLOB_RECURSE ECS_HEADERS CONFIGURE_DEPENDS
    ${ECS_INCLUDE_PATH}/*.h
)

# VS 解决方案分组跟随目录结构（source_group(TREE)），不再手写 22 条。
# 代价：分组名里原先编码的**执行阶段序号**（00/01/02/05/06B/…/27）随之消失。
# 那套顺序的唯一台帐是 `.ai/skills/SKILL_EXECUTION_PHASE_ORDERING.md`，此处留一份
# 索引便于对照：
#   00 RenderSystemCore → 01 TickInput_InputSystem → 02 TickTransform_TransformSystem
#   → 05 TickCamera_CameraSystem → 06B TickTransform_LineBoundsUpdateSystem
#   → 07 RenderSwapchainNextImage_SwapchainNextImageSystem
#   → 08 RenderPreBeginFrame_RenderTargetSystem
#   → 09A RenderPreBeginFrame_MainLightShadowSystem
#   → 09B RenderPreBeginFrame_ColorPaletteSystem
#   → 09C RenderPreBeginFrame_LightSystem
#   → 09 RenderPreBeginFrame_EnvironmentSystem
#   → 13B RenderBufferCommit_ViewUBOCommitSystem
#   → 14 RenderBufferUpload_RenderBufferUploadSystem
#   → 15B RenderFrameSync_RenderSceneUBOSystem
#   → 16 RenderCollect_RenderPrimitiveCollectSystem
#   → 26B RenderStat_LineStatsSystem → 27 RenderSubmit_SwapchainSubmitSystem
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "ECS" FILES ${ECS_SOURCES})
source_group(TREE ${ECS_INCLUDE_PATH} PREFIX "ECS\\include" FILES ${ECS_HEADERS})

add_cm_library(ULRE.ECS "ULRE" ${ECS_INCLUDE_PATH}
${ECS_SOURCES} ${ECS_HEADERS})

# 只在「强制打开」时下发；不下发时由 `inc/hgl/common/RenderOptions.h` §2 按 NDEBUG 决定（Debug 1 / Release 0）。
# ⚠ 不要在这里下发 `=0`：那会压过台帐的 Debug 默认值，让 Debug 也丢掉校验面（2026-10-13 统一时删掉的分支）。
if(ULRE_ECS_DEBUG_API)
    target_compile_definitions(ULRE.ECS PUBLIC ULRE_ECS_DEBUG_API=1)
endif()

# CPU 剔除参考台架（每 60 帧 GPU 读回对拍；读回 stall ⇒ 默认关，生产不付这笔钱）。
# 台帐默认值见 `inc/hgl/common/RenderOptions.h` §2（`ULRE_ENABLE_CPU_CULL_PARITY_TEST`，默认 0）。
# 判定一律写 `#if ULRE_ENABLE_CPU_CULL_PARITY_TEST` —— 禁 `defined()`（值为 0 时它同样为真）。
if(ULRE_ENABLE_CPU_CULL_PARITY_TEST)
    target_compile_definitions(ULRE.ECS PRIVATE ULRE_ENABLE_CPU_CULL_PARITY_TEST=1)
endif()

add_executable(TestTransformFlatStorage tests/TestTransformFlatStorage.cpp)
target_link_libraries(TestTransformFlatStorage PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestTransformFlatStorage PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

add_executable(TestCSMIncrementalPass tests/TestCSMIncrementalPass.cpp)
target_link_libraries(TestCSMIncrementalPass PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestCSMIncrementalPass PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# 包围盒 SoA 存储 + 值类型句柄（组件 → BoundingBoxDataStorage/BoundingBoxAccessor 改造）
add_executable(TestBoundingBoxStorage tests/TestBoundingBoxStorage.cpp)
target_link_libraries(TestBoundingBoxStorage PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestBoundingBoxStorage PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# 渲染策略判定表（A1；A7a 起为收集/阴影能力的**唯一判据**）
add_executable(TestRenderStrategyTable tests/TestRenderStrategyTable.cpp support/RenderStrategyParity.cpp)
target_link_libraries(TestRenderStrategyTable PRIVATE ULRE::Engine)
# 测试目标强制开对拍（默认值 / 打开方式见 inc/hgl/common/RenderOptions.h §2）
target_compile_definitions(TestRenderStrategyTable PRIVATE ULRE_STRATEGY_PARITY_ENABLED=1)
if(MSVC)
    set_target_properties(TestRenderStrategyTable PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# A3 材质变体表（静态键去重 + 预算制 fail-fast）
add_executable(TestMaterialVariantTable tests/TestMaterialVariantTable.cpp)
target_link_libraries(TestMaterialVariantTable PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestMaterialVariantTable PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# A4 材质运行期表（共享行 interned + CoW + refcount + 世代 + 每实例 slot）
add_executable(TestMaterialRuntimeTable tests/TestMaterialRuntimeTable.cpp)
target_link_libraries(TestMaterialRuntimeTable PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestMaterialRuntimeTable PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# TOML MaterialRecipe parser schema and diagnostics.
add_executable(TestMaterialRecipeFile tests/TestMaterialRecipeFile.cpp)
target_link_libraries(TestMaterialRecipeFile PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestMaterialRecipeFile PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# TOML GeometryAsset parser schema and diagnostics.
add_executable(TestGeometryAssetFile tests/TestGeometryAssetFile.cpp)
target_link_libraries(TestGeometryAssetFile PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestGeometryAssetFile PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# TOML PrimitiveAsset parser (geometry + recipe binding).
add_executable(TestPrimitiveAssetFile tests/TestPrimitiveAssetFile.cpp)
target_link_libraries(TestPrimitiveAssetFile PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestPrimitiveAssetFile PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

add_executable(TestComponentRows tests/TestComponentRows.cpp)
target_link_libraries(TestComponentRows PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestComponentRows PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

add_executable(TestVisibilityStorage tests/TestVisibilityStorage.cpp)
target_link_libraries(TestVisibilityStorage PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestVisibilityStorage PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

add_executable(TestBoundingVolumeCull tests/TestBoundingVolumeCull.cpp)
target_link_libraries(TestBoundingVolumeCull PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestBoundingVolumeCull PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# 诊断探针（T0 量化基线 + T4 旋转提取证据）：只打印数值，不做通过/失败判定
add_executable(ProbeTransformDiagnostics tests/ProbeTransformDiagnostics.cpp)
target_link_libraries(ProbeTransformDiagnostics PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(ProbeTransformDiagnostics PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# 批 1.3 C5 判据探针：一次性告警闩必须**每实例**（进程级 static ⇒ 第二个世界静默）。
# 判据（外部）：跑完 grep -c "切片起点未 4 行对齐" ⇒ 期望 2（两个实例各 1）。
add_executable(ProbeWarnLatchPerWorld tests/ProbeWarnLatchPerWorld.cpp)
target_link_libraries(ProbeWarnLatchPerWorld PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(ProbeWarnLatchPerWorld PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# S1/S2 判据：同类型系统替换必须摘旧实例；裸行池 Row() 必须验活跃（+ 两个池语义对齐 + 析构顺序钉）
add_executable(TestSystemReplaceAndPoolLiveness tests/TestSystemReplaceAndPoolLiveness.cpp)
target_link_libraries(TestSystemReplaceAndPoolLiveness PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestSystemReplaceAndPoolLiveness PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# §2 #11 判据：PrepareFrame 必须是每帧一次的真守卫（同帧三次调用只做一次准备）
add_executable(TestFramePrepareOncePerFrame tests/TestFramePrepareOncePerFrame.cpp)
target_link_libraries(TestFramePrepareOncePerFrame PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestFramePrepareOncePerFrame PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# §3-②/⑥ 判据：相机纯读（Resolve）与认领（Ensure）分离（+ 实体级相机模板编译期禁死）
add_executable(TestCameraResolveAndClaim tests/TestCameraResolveAndClaim.cpp)
target_link_libraries(TestCameraResolveAndClaim PRIVATE ULRE::Engine)
if(MSVC)
    set_target_properties(TestCameraResolveAndClaim PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

if(BUILD_TESTING)
    add_test(
        NAME TestTransformFlatStorage
        COMMAND TestTransformFlatStorage
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    # TIMEOUT：B2 的父环回归一旦复发就是**挂死**（死循环），加超时把挂死变成测试失败
    set_tests_properties(
        TestTransformFlatStorage
        PROPERTIES LABELS "ecs;transform" TIMEOUT 120)

    add_test(
        NAME TestCSMIncrementalPass
        COMMAND TestCSMIncrementalPass
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestCSMIncrementalPass
        PROPERTIES LABELS "csm;pass")

    add_test(
        NAME TestBoundingVolumeCull
        COMMAND TestBoundingVolumeCull)
    set_tests_properties(
        TestBoundingVolumeCull
        PROPERTIES LABELS "ecs;cull")

    add_test(
        NAME TestMaterialVariantTable
        COMMAND TestMaterialVariantTable)
    set_tests_properties(
        TestMaterialVariantTable
        PROPERTIES LABELS "ecs;material_variant")

    add_test(
        NAME TestMaterialRuntimeTable
        COMMAND TestMaterialRuntimeTable)
    set_tests_properties(
        TestMaterialRuntimeTable
        PROPERTIES LABELS "ecs;material_runtime")

    add_test(
        NAME TestMaterialRecipeFile
        COMMAND TestMaterialRecipeFile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestMaterialRecipeFile
        PROPERTIES LABELS "material;recipe")

    add_test(
        NAME TestGeometryAssetFile
        COMMAND TestGeometryAssetFile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestGeometryAssetFile
        PROPERTIES LABELS "geometry;asset")

    add_test(
        NAME TestPrimitiveAssetFile
        COMMAND TestPrimitiveAssetFile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestPrimitiveAssetFile
        PROPERTIES LABELS "geometry;primitive;asset")

    add_test(
        NAME TestBoundingBoxStorage
        COMMAND TestBoundingBoxStorage)
    set_tests_properties(
        TestBoundingBoxStorage
        PROPERTIES LABELS "ecs;bounding_box")

    add_test(
        NAME TestRenderStrategyTable
        COMMAND TestRenderStrategyTable)
    set_tests_properties(
        TestRenderStrategyTable
        PROPERTIES LABELS "ecs;render_strategy")

    add_test(
        NAME TestComponentRows
        COMMAND TestComponentRows)
    set_tests_properties(
        TestComponentRows
        PROPERTIES LABELS "ecs;component_rows")

    add_test(
        NAME TestVisibilityStorage
        COMMAND TestVisibilityStorage)
    set_tests_properties(
        TestVisibilityStorage
        PROPERTIES LABELS "ecs;visibility")

    add_test(
        NAME TestSystemReplaceAndPoolLiveness
        COMMAND TestSystemReplaceAndPoolLiveness
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestSystemReplaceAndPoolLiveness
        PROPERTIES LABELS "ecs;registry" TIMEOUT 60)

    add_test(
        NAME TestFramePrepareOncePerFrame
        COMMAND TestFramePrepareOncePerFrame)
    set_tests_properties(
        TestFramePrepareOncePerFrame
        PROPERTIES LABELS "ecs;render" TIMEOUT 60)

    add_test(
        NAME TestCameraResolveAndClaim
        COMMAND TestCameraResolveAndClaim)
    set_tests_properties(
        TestCameraResolveAndClaim
        PROPERTIES LABELS "ecs;camera" TIMEOUT 60)
endif()
