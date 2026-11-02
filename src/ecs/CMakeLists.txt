SET(ECS_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/ecs)

# ECS 调试/校验面（对拍器 + 诊断打印）。**开关与默认值的唯一台帐 = inc/hgl/common/RenderOptions.h §2**：
# 本 option 只表达「在所有构建配置里**强制打开**」；默认 OFF = 不下发宏 ⇒ 由台帐按配置决定（Debug 开 / Release 关）。
# 想「全配置强制关」：配置时另加 `-DCMAKE_CXX_FLAGS=-DULRE_ECS_DEBUG_API=0`（罕见）。
option(ULRE_ECS_DEBUG_API "在所有构建配置里强制打开 ECS 调试/校验面（OFF = 交给 RenderOptions.h 台帐：Debug 开 / Release 关）" OFF)
option(ULRE_ENABLE_CPU_CULL_PARITY_TEST "注册 CPU 剔除参考台架（每 60 帧 GPU 读回对拍）；默认关，生产管线不付那笔 stall" OFF)

# ── 源文件收集：按架构层递归扫描 ──────────────────────────────────────────────
# 实现文件进入 ULRE.ECS；公开头文件同样登记到目标，只为 IDE 提供完整视图。
# 物理目录即架构层：components / core / systems/{tick,render} / support/<职能>。
# CONFIGURE_DEPENDS 使增删文件后自动重新 configure。
file(GLOB_RECURSE ECS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/support/*.cpp")
file(GLOB_RECURSE ECS_HEADERS CONFIGURE_DEPENDS
    "${ECS_INCLUDE_PATH}/components/*.h"
    "${ECS_INCLUDE_PATH}/core/*.h"
    "${ECS_INCLUDE_PATH}/systems/*.h"
    "${ECS_INCLUDE_PATH}/support/*.h")

# Tests 是可执行目标（见文件后半），此处只收集其源文件用于 IDE 分组。
file(GLOB_RECURSE ECS_TEST_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h")

# ── IDE 分组：物理目录 → 可读组名 ────────────────────────────────────────────
# 头文件与实现文件按同一目录落到同一组，不再分 `ECS` / `ECS\include` 两棵树。
# 每条只管辖本层目录（不递归）：子目录自己登记一条。漏登记的文件落到
# `ECS\Ungrouped` 并告警，不会从构建里消失。
set(ECS_GROUP_DIRS
    "components:Components"
    "core:Core"
    "core/entity:Core\\Entity"
    "core/render:Core\\Render"
    "systems/tick:Systems\\Tick"
    "systems/render:Systems\\Render"
    "systems/render/frame:Systems\\Render\\Frame"
    "systems/render/collect:Systems\\Render\\Collect"
    "systems/render/buffer:Systems\\Render\\Buffer"
    "systems/render/light_shadow:Systems\\Render\\LightShadow"
    "systems/render/streaming:Systems\\Render\\Streaming"
    "support/cull:Support\\Culling"
    "support/transform:Support\\Transform"
    "support/storage:Support\\Storage"
    "support/material:Support\\Material"
    "support/pipeline:Support\\Pipeline"
    "support/line:Support\\Line"
    "support/primitive:Support\\Primitive"
    "support/text:Support\\Text")

set(ECS_GROUPED_FILES)
foreach(entry IN LISTS ECS_GROUP_DIRS)
    string(FIND "${entry}" ":" sep)
    string(SUBSTRING "${entry}" 0 ${sep} rel_dir)
    math(EXPR sep "${sep} + 1")
    string(SUBSTRING "${entry}" ${sep} -1 group_name)

    file(GLOB _ecs_group_src CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${rel_dir}/*.cpp")
    file(GLOB _ecs_group_hdr CONFIGURE_DEPENDS
        "${ECS_INCLUDE_PATH}/${rel_dir}/*.h")

    if(_ecs_group_src)
        source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/${rel_dir}"
            PREFIX "ECS\\${group_name}" FILES ${_ecs_group_src})
        list(APPEND ECS_GROUPED_FILES ${_ecs_group_src})
    endif()
    if(_ecs_group_hdr)
        source_group(TREE "${ECS_INCLUDE_PATH}/${rel_dir}"
            PREFIX "ECS\\${group_name}" FILES ${_ecs_group_hdr})
        list(APPEND ECS_GROUPED_FILES ${_ecs_group_hdr})
    endif()
endforeach()

# 兜底：未登记目录里的文件照样编译，只在 IDE 里落到 Ungrouped 并给出告警。
set(ECS_UNGROUPED_FILES)
foreach(_ecs_file IN LISTS ECS_SOURCES ECS_HEADERS)
    list(FIND ECS_GROUPED_FILES "${_ecs_file}" _ecs_idx)
    if(_ecs_idx EQUAL -1)
        list(APPEND ECS_UNGROUPED_FILES "${_ecs_file}")
    endif()
endforeach()
if(ECS_UNGROUPED_FILES)
    list(LENGTH ECS_UNGROUPED_FILES _ecs_ungrouped_count)
    message(WARNING
        "ULRE.ECS: ${_ecs_ungrouped_count} 个文件所在目录未登记到 ECS_GROUP_DIRS，"
        "已归入 IDE 分组 ECS\\Ungrouped：${ECS_UNGROUPED_FILES}")
    source_group("ECS\\Ungrouped" FILES ${ECS_UNGROUPED_FILES})
endif()

source_group("ECS\\Tests" FILES ${ECS_TEST_FILES})

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

# ── ECS 测试台架 ──────────────────────────────────────────────────────────────
# 统一收口：链接 ULRE::Engine、调试工作目录指向运行期目录、解决方案归入 ULRE/Tests/ECS，
# 避免每个测试目标重复三行样板。
#   EXTRA_SOURCES：额外参与编译的源文件；LIBS：额外链接的库。
function(add_ecs_test target_name source_file)
    cmake_parse_arguments(ECS_TEST "" "" "EXTRA_SOURCES;LIBS" ${ARGN})
    add_executable(${target_name} ${source_file} ${ECS_TEST_EXTRA_SOURCES})
    target_link_libraries(${target_name} PRIVATE ULRE::Engine ${ECS_TEST_LIBS})
    set_target_properties(${target_name} PROPERTIES FOLDER "ULRE/Tests/ECS")
    if(MSVC)
        set_target_properties(${target_name} PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    endif()
endfunction()

add_ecs_test(TestTransformFlatStorage tests/TestTransformFlatStorage.cpp)

add_ecs_test(TestCSMIncrementalPass tests/TestCSMIncrementalPass.cpp)

# 包围盒 SoA 存储 + 值类型句柄（组件 → BoundingBoxDataStorage/BoundingBoxAccessor 改造）
add_ecs_test(TestBoundingBoxStorage tests/TestBoundingBoxStorage.cpp)

# 渲染策略判定表（A1；A7a 起为收集/阴影能力的**唯一判据**）
add_ecs_test(TestRenderStrategyTable tests/TestRenderStrategyTable.cpp
    EXTRA_SOURCES support/pipeline/RenderStrategyParity.cpp)
# 测试目标强制开对拍（默认值 / 打开方式见 inc/hgl/common/RenderOptions.h §2）
target_compile_definitions(TestRenderStrategyTable PRIVATE ULRE_STRATEGY_PARITY_ENABLED=1)

# A3 材质变体表（静态键去重 + 预算制 fail-fast）
add_ecs_test(TestMaterialVariantTable tests/TestMaterialVariantTable.cpp)

# A4 材质运行期表（共享行 interned + CoW + refcount + 世代 + 每实例 slot）
add_ecs_test(TestMaterialRuntimeTable tests/TestMaterialRuntimeTable.cpp)

# TOML MaterialRecipe parser schema and diagnostics.
add_ecs_test(TestMaterialRecipeFile tests/TestMaterialRecipeFile.cpp)

# 材质数据块：授权行 → 物理行摊平、MDB1 二进制往返、坏输入拒绝。
add_ecs_test(TestMaterialDataBlockCodec tests/TestMaterialDataBlockCodec.cpp)

# 全局 SSBO 连续行区间与数据块整段上传（块→视图零拷贝、未就绪时 fail-closed、分配策略契约）。
add_ecs_test(TestGlobalSSBORowRange tests/TestGlobalSSBORowRange.cpp)

# 材质数据块资产层：三种文件形态 → 同一句柄、坏输入留在表里、无设备上传软失败。
add_ecs_test(TestMaterialDataBlockAsset tests/TestMaterialDataBlockAsset.cpp)

# 批量材质的行绑定入口：数据块行 → 池行号 → GlobalSSBOBinding 的组合契约，
# 以及 builder 源码里「先上传再判越界」的顺序契约。
add_ecs_test(TestRenderableBuilderDataRow tests/TestRenderableBuilderDataRow.cpp)

# 环境资产规约（`*.env.toml`）：五组字段往返 + `Presence` 三态 + 枚举拼写别名
# + `[[sh9.bands]]` 顺序 + 元数据 struct_meta/elem_kind 接线。
add_ecs_test(TestEnvironmentCodec tests/TestEnvironmentCodec.cpp)

# 环境资产表（`env:` URI）：身份 / 按名查询 / 缓存 / 扩展名与类型门禁
# + `ApplyEnvironmentSpec` 的 `Presence` 局部覆盖语义。
add_ecs_test(TestEnvironmentAssetURI tests/TestEnvironmentAssetURI.cpp)

# 配方差量继承（`parent`）：合并口径 + 注册表两阶段加载。
add_ecs_test(TestMaterialRecipeInheritance tests/TestMaterialRecipeInheritance.cpp)

# 示例用数据块文件（`pbr_10x10_grid.material_data.toml`）：形状 + 逐行数值与生成公式
# 一致 + 演示源码不再逐行硬编码。
add_ecs_test(TestPbrGridDataBlock tests/TestPbrGridDataBlock.cpp)

# 示例用纹理数组规约（`*.texarray.toml`）：六个文件按台账解析 + 缺省层名带父目录
# + 坏输入整张拒绝 + `texarray:` URI 映射 + 示例不再手工装配 2D 数组。
add_ecs_test(TestTextureArraySpec tests/TestTextureArraySpec.cpp)

# 手工建实体示例的行绑定入口：`GetRowBinding` / `GetGlobalSSBOBindingAt` /
# `WorkObject::GetMaterialDataBinding` 三层契约（失败不写 out、先上传再取行号）。
add_ecs_test(TestMaterialDataRowBinding tests/TestMaterialDataRowBinding.cpp)

# 阶段 A 台账门禁：示例的材质数据块文件集合 / 块名 / 行类型 / 行数 / 抽样行值。
add_ecs_test(TestExampleMaterialDataBlocks tests/TestExampleMaterialDataBlocks.cpp)

# TOML GeometryAsset parser schema and diagnostics.
add_ecs_test(TestGeometryAssetFile tests/TestGeometryAssetFile.cpp)

# TOML PrimitiveAsset parser (geometry + recipe binding).
add_ecs_test(TestPrimitiveAssetFile tests/TestPrimitiveAssetFile.cpp)

# Asset factory: self-registering type -> loader table + unregistered-type fallback.
add_ecs_test(TestAssetFactory tests/TestAssetFactory.cpp)

# Geometry preset library: every shape name used by examples must resolve to a TOML.
add_ecs_test(TestGeometryPreset tests/TestGeometryPreset.cpp)

# TOML SceneSpec parser: schema, references, type-specific keys, and parent cycles.
add_ecs_test(TestSceneFile tests/TestSceneFile.cpp)

# 统一序列化框架（TestSerializeArchive）、资产 URI（TestAssetURI）、MetaObject 生命周期
# （MetaObjectTest）三个用例已随实现一起下沉到 CMAssetsManage/test/，由
# use_cm_module(CMAssetsManage) 带进构建；ctest 名称与 LABELS 不变，本文件不再登记。

# Asset reflection codegen: TOML round-trip, metadata table, FlatBuffers round-trip.
# 纯数据往返，不需要设备/窗口，因此在 CI 里也能跑，用来挡住
# 「改了 C++ 结构但忘记重新生成」与「生成器改坏了语义」。
find_package(flatbuffers CONFIG QUIET)
if(flatbuffers_FOUND)
    add_ecs_test(TestAssetReflection tests/TestAssetReflection.cpp
        LIBS flatbuffers::flatbuffers)
else()
    message(STATUS "flatbuffers 未找到：跳过 TestAssetReflection（含 FlatBuffers 往返用例）")
endif()

# 材质配方规约（`*.recipe.toml`）的字段契约：`Presence<T>` 三态、枚举拼写、
# 字符串数组 presence、局部覆盖往返与 FlatBuffers 往返。
# 与 TestAssetReflection 一样是纯数据用例，用来挡住「改了 Spec 但忘记重新生成」
# 与「生成器改坏了 Presence 语义」。
if(flatbuffers_FOUND)
    add_ecs_test(TestMaterialRecipeCodec tests/TestMaterialRecipeCodec.cpp
        LIBS flatbuffers::flatbuffers)
else()
    message(STATUS "flatbuffers 未找到：跳过 TestMaterialRecipeCodec（含 FlatBuffers 往返用例）")
endif()

add_ecs_test(TestComponentRows tests/TestComponentRows.cpp)

add_ecs_test(TestVisibilityStorage tests/TestVisibilityStorage.cpp)

add_ecs_test(TestBoundingVolumeCull tests/TestBoundingVolumeCull.cpp)

# 诊断探针（T0 量化基线 + T4 旋转提取证据）：只打印数值，不做通过/失败判定
add_ecs_test(ProbeTransformDiagnostics tests/ProbeTransformDiagnostics.cpp)

# 批 1.3 C5 判据探针：一次性告警闩必须**每实例**（进程级 static ⇒ 第二个世界静默）。
# 判据（外部）：跑完 grep -c "切片起点未 4 行对齐" ⇒ 期望 2（两个实例各 1）。
add_ecs_test(ProbeWarnLatchPerWorld tests/ProbeWarnLatchPerWorld.cpp)

# S1/S2 判据：同类型系统替换必须摘旧实例；裸行池 Row() 必须验活跃（+ 两个池语义对齐 + 析构顺序钉）
add_ecs_test(TestSystemReplaceAndPoolLiveness tests/TestSystemReplaceAndPoolLiveness.cpp)

# §2 #11 判据：PrepareFrame 必须是每帧一次的真守卫（同帧三次调用只做一次准备）
add_ecs_test(TestFramePrepareOncePerFrame tests/TestFramePrepareOncePerFrame.cpp)

# §3-②/⑥ 判据：相机纯读（Resolve）与认领（Ensure）分离（+ 实体级相机模板编译期禁死）
add_ecs_test(TestCameraResolveAndClaim tests/TestCameraResolveAndClaim.cpp)

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
        NAME TestMaterialDataBlockCodec
        COMMAND TestMaterialDataBlockCodec
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestMaterialDataBlockCodec
        PROPERTIES LABELS "ecs;material_data" TIMEOUT 120)

    add_test(
        NAME TestGlobalSSBORowRange
        COMMAND TestGlobalSSBORowRange
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestGlobalSSBORowRange
        PROPERTIES LABELS "ecs;material_data" TIMEOUT 120)

    add_test(
        NAME TestMaterialDataBlockAsset
        COMMAND TestMaterialDataBlockAsset
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestMaterialDataBlockAsset
        PROPERTIES LABELS "ecs;material_data;asset" TIMEOUT 120)

    add_test(
        NAME TestRenderableBuilderDataRow
        COMMAND TestRenderableBuilderDataRow
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestRenderableBuilderDataRow
        PROPERTIES LABELS "ecs;material_data;asset" TIMEOUT 120)

    add_test(
        NAME TestMaterialDataRowBinding
        COMMAND TestMaterialDataRowBinding
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestMaterialDataRowBinding
        PROPERTIES LABELS "ecs;material_data;example" TIMEOUT 120)

    add_test(
        NAME TestExampleMaterialDataBlocks
        COMMAND TestExampleMaterialDataBlocks
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestExampleMaterialDataBlocks
        PROPERTIES LABELS "ecs;material_data;example" TIMEOUT 120)

    add_test(
        NAME TestPbrGridDataBlock
        COMMAND TestPbrGridDataBlock
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestPbrGridDataBlock
        PROPERTIES LABELS "ecs;material_data;example" TIMEOUT 120)

    add_test(
        NAME TestTextureArraySpec
        COMMAND TestTextureArraySpec
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestTextureArraySpec
        PROPERTIES LABELS "ecs;texture;example" TIMEOUT 120)

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
        NAME TestMaterialRecipeInheritance
        COMMAND TestMaterialRecipeInheritance
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestMaterialRecipeInheritance
        PROPERTIES LABELS "material;recipe")

    add_test(
        NAME TestEnvironmentCodec
        COMMAND TestEnvironmentCodec
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestEnvironmentCodec
        PROPERTIES LABELS "environment;asset")

    add_test(
        NAME TestEnvironmentAssetURI
        COMMAND TestEnvironmentAssetURI
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestEnvironmentAssetURI
        PROPERTIES LABELS "environment;asset")

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
        NAME TestAssetFactory
        COMMAND TestAssetFactory
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestAssetFactory
        PROPERTIES LABELS "asset;factory")

    add_test(
        NAME TestGeometryPreset
        COMMAND TestGeometryPreset
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestGeometryPreset
        PROPERTIES LABELS "geometry;preset;asset")

    add_test(
        NAME TestSceneFile
        COMMAND TestSceneFile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestSceneFile
        PROPERTIES LABELS "scene;asset")

    add_test(
        NAME TestAssetReflection
        COMMAND TestAssetReflection
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestAssetReflection
        PROPERTIES LABELS "asset;reflection;codegen")

    if(flatbuffers_FOUND)
        add_test(
            NAME TestMaterialRecipeCodec
            COMMAND TestMaterialRecipeCodec
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(
            TestMaterialRecipeCodec
            PROPERTIES LABELS "asset;recipe;codegen")
    endif()

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
