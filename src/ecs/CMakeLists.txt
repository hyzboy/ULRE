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
    "core/system:Core\\System"
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
    "support/spatial:Support\\Spatial"
    "support/transform:Support\\Transform"
    "support/assembly:Support\\Assembly"
    "support/storage:Support\\Storage"
    "support/material:Support\\Material"
    "support/render:Support\\Render"
    "support/pipeline:Support\\Pipeline"
    "support/line:Support\\Line"
    "support/primitive:Support\\Primitive"
    "support/text:Support\\Text"
    "support/hierarchy2d:Support\\Hierarchy2D"
    "support/debug:Support\\Debug")

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

# ============================================================================
# ── 分层：ULRE.ECS.Core / ULRE.ECS.Render / ULRE.ECS（umbrella）───────────────
#
# 判定口径（可机械复核）：**文件的 include 闭包内是否出现 hgl/graph/、hgl/vk/
# 或 vulkan/ 头**（hgl/graph/CameraInfo.h 除外——它由 CMMath 提供，是基础库类型）。
#   · 闭包干净 ⇒ Core：ECS 内核（实体/组件池/系统调度/存储/基础 tick），
#     不允许任何图形后端或场景图语义渗入。
#   · 其余 ⇒ Render：需要 SceneGraph/Vulkan 的收集、批处理、管线、GPU 表。
#
# 为什么是显式清单而不是 glob 分流：新增文件若两边都没登记，configure 直接报错
# （见下方 fail-fast），逼迫作者做出分层判断；glob 会把新文件静默吞进 Render，
# 分层就会随时间腐化。
# ============================================================================
set(ECS_CORE_SOURCES
    "components/Layer2DComponent.cpp"
    "components/Transform2DComponent.cpp"
    "core/ECSCoreContext.cpp"
    "core/entity/ComponentRow.cpp"
    "core/entity/Entity.cpp"
    "core/entity/EntityManager.cpp"
    "core/entity/Object.cpp"
    "core/system/System.cpp"
    "core/system/SystemGroup.cpp"
    "core/system/SystemOrderPlanner.cpp"
    "core/system/SystemRegistry.cpp"
    "support/cull/BatchLayoutStaging.cpp"
    "support/material/MaterialVariantTable.cpp"
    "support/pipeline/RenderStrategyParity.cpp"
    "support/spatial/WorldSpatialState.cpp"
    "support/storage/FrameProfiler.cpp"
    "systems/tick/CameraInputMapping.cpp"
    "systems/tick/InputSystem.cpp"
)

set(ECS_CORE_HEADERS
    "components/CameraControlMode.h"
    "components/GPUInstancedModelBatchComponent.h"
    "components/Layer2DComponent.h"
    "components/LinesComponent.h"
    "components/ModelCollisionProxyComponent.h"
    "components/RigidBodyComponent.h"
    "components/ShadowProxy.h"
    "components/SimpleLinesComponent.h"
    "components/Sprite2DComponent.h"
    "components/Transform2DComponent.h"
    "core/AdaptiveRenderGraphCache.h"
    "core/DefaultSystems.h"
    "core/ECSCoreContext.h"
    "core/MainFrameTopologyState.h"
    "core/RenderPassScopeState.h"
    "core/entity/ComponentRow.h"
    "core/entity/ECSEntityObserver.h"
    "core/entity/Entity.h"
    "core/entity/EntityGPU.h"
    "core/entity/EntityHandle.h"
    "core/entity/EntityManager.h"
    "core/entity/Object.h"
    "core/render/RenderGraph.h"
    "core/render/ScenePipelineMode.h"
    "core/system/System.h"
    "core/system/SystemGroup.h"
    "core/system/SystemGroupActivity.h"
    "core/system/SystemOrderPlanner.h"
    "core/system/SystemRegistry.h"
    "core/system/SystemScheduleState.h"
    "support/cull/BatchLayoutStaging.h"
    "support/cull/BoundingBoxAccessor.h"
    "support/cull/BoundingBoxDataStorage.h"
    "support/cull/BoundingVolumeCull.h"
    "support/cull/VisibilityDataStorage.h"
    "support/debug/ProfOverlaySystem.h"
    "support/hierarchy2d/CullAndSort2DSystem.h"
    "support/line/LineBufferCommon.h"
    "support/line/LineBuildSystem.h"
    "support/line/LineCollectSystem.h"
    "support/line/LineRenderSystem.h"
    "support/line/SimpleLineBuildSystem.h"
    "support/line/SimpleLineCollectSystem.h"
    "support/line/SimpleLineRenderSystem.h"
    "support/material/MaterialVariantTable.h"
    "support/pipeline/RenderPassSlot.h"
    "support/pipeline/RenderPipelineBase.h"
    "support/pipeline/RenderPipelineSystem.h"
    "support/pipeline/RenderResource.h"
    "support/pipeline/RenderStrategyParity.h"
    "support/pipeline/RenderStrategyTable.h"
    "support/pipeline/TemplateFactCache.h"
    "support/primitive/PrimitiveBuildSystem.h"
    "support/spatial/WorldSpatialState.h"
    "support/primitive/PrimitiveCullSystem.h"
    "support/primitive/PrimitiveOverlayRenderSystem.h"
    "support/primitive/PrimitiveRenderSystem.h"
    "support/primitive/PrimitiveSortSystem.h"
    "support/primitive/PrimitiveUI2DRenderSystem.h"
    "support/storage/CollectCacheState.h"
    "support/storage/ComponentPool.h"
    "support/storage/ComponentPoolRegistry.h"
    "support/storage/ComponentTypeTable.h"
    "support/storage/DirtyRowSet.h"
    "support/storage/EntityGPUTable.h"
    "support/storage/FrameProfiler.h"
    "support/storage/MainFrameTopologySnapshot.h"
    "support/storage/PerfTimer.h"
    "support/storage/PrimitiveTopologyLedger.h"
    "support/text/TextBuildSystem.h"
    "support/text/TextCollectSystem.h"
    "support/text/TextOverlayRenderSystem.h"
    "support/text/TextRenderSystem.h"
    "support/text/TextSyncSystem.h"
    "support/transform/TransformAccessor.h"
    "support/transform/TransformDataStorage.h"
    "support/transform/TransformID.h"
    "support/transform/TransformSlotTable.h"
    "systems/render/buffer/RenderBufferUploadSystem.h"
    "systems/render/buffer/RenderSceneUBOSystem.h"
    "systems/render/buffer/ViewUBOCommitSystem.h"
    "systems/render/collect/LineStatsSystem.h"
    "systems/render/collect/SimpleLineStatsSystem.h"
    "systems/render/frame/RenderTargetSystem.h"
    "systems/render/frame/SwapchainNextImageSystem.h"
    "systems/render/frame/SwapchainSubmitSystem.h"
    "systems/render/streaming/GeometryStreamingSystem.h"
    "systems/render/streaming/ModelLoadPolicy.h"
    "systems/tick/CameraInputMapping.h"
    "systems/tick/InputSystem.h"
    "systems/tick/LineBoundsUpdateSystem.h"
    "systems/tick/SimpleLineBoundsUpdateSystem.h"
    "systems/tick/Transform2DSystem.h"
)

set(ECS_RENDER_SOURCES
    "components/CameraComponent.cpp"
    "components/GPUModelRefComponent.cpp"
    "components/GeometryData.cpp"
    "components/MaterialData.cpp"
    "components/ModelCollisionProxyComponent.cpp"
    "components/RigidBodyComponent.cpp"
    "components/ShadowProxy.cpp"
    "core/Context.cpp"
    "core/ContextGPU.cpp"
    "core/ContextRender.cpp"
    "core/ContextSystems.cpp"
    "core/DefaultSystems.cpp"
    "core/FrameLaneWaitSet.cpp"
    "core/render/MaterialBatch.cpp"
    "core/render/PrimitiveRenderItem.cpp"
    "core/render/RenderFrameCache.cpp"
    "core/render/RenderGraph.cpp"
    "support/assembly/ModelAssemblyStage.cpp"
    "support/assembly/StaticMeshModelInstance.cpp"
    "support/cull/CPUCullReferenceSystem.cpp"
    "support/cull/GPUCullSystem.cpp"
    "support/cull/ModelGroupRoutingTables.cpp"
    "support/cull/VisibilityDataStorage.cpp"
    "support/debug/DebugViewController.cpp"
    "support/debug/DebugViewPass.cpp"
    "support/debug/ProfOverlaySystem.cpp"
    "support/hierarchy2d/CullAndSort2DSystem.cpp"
    "support/hierarchy2d/Sprite2DSyncSystem.cpp"
    "support/line/LineBufferCommon.cpp"
    "support/line/LineBuildSystem.cpp"
    "support/line/LineCollectSystem.cpp"
    "support/line/LineRenderPipeline.cpp"
    "support/line/LineRenderSystem.cpp"
    "support/line/SimpleLineBuildSystem.cpp"
    "support/line/SimpleLineCollectSystem.cpp"
    "support/line/SimpleLineRenderPipeline.cpp"
    "support/line/SimpleLineRenderSystem.cpp"
    "support/material/MaterialRuntimeTable.cpp"
    "support/material/ModelTemplateBuilder.cpp"
    "support/material/ModelTemplateMaterialTable.cpp"
    "support/pipeline/MaterialBatchPlanCache.cpp"
    "support/pipeline/PipelineMaterialRenderer.cpp"
    "support/pipeline/PrimitiveState.cpp"
    "support/pipeline/RenderBatchPipeline.cpp"
    "support/pipeline/RenderPipelineSystem.cpp"
    "support/pipeline/RenderResource.cpp"
    "support/pipeline/RenderableBuilder.cpp"
    "support/pipeline/TemplateFactCache.cpp"
    "support/render/WorldRenderData.cpp"
    "support/render/WorldGPUResources.cpp"
    "support/primitive/PrimitiveBuildSystem.cpp"
    "support/primitive/PrimitiveCullSystem.cpp"
    "support/primitive/PrimitiveOverlayRenderSystem.cpp"
    "support/primitive/PrimitiveRenderPipeline.cpp"
    "support/primitive/PrimitiveRenderSystem.cpp"
    "support/primitive/PrimitiveSortSystem.cpp"
    "support/primitive/PrimitiveUI2DRenderSystem.cpp"
    "support/storage/CameraInfoStorage.cpp"
    "support/storage/CameraSlotGuard.cpp"
    "support/storage/MainFrameTopologySnapshot.cpp"
    "support/storage/PrimitiveTopologyLedger.cpp"
    "support/text/TextBuildSystem.cpp"
    "support/text/TextCollectSystem.cpp"
    "support/text/TextOverlayRenderSystem.cpp"
    "support/text/TextRenderPipeline.cpp"
    "support/text/TextRenderSystem.cpp"
    "support/text/TextSyncSystem.cpp"
    "support/transform/TransformAccessor.cpp"
    "support/transform/TransformAssignmentBuffer.cpp"
    "systems/render/buffer/RenderBufferUploadSystem.cpp"
    "systems/render/buffer/RenderSceneUBOSystem.cpp"
    "systems/render/buffer/ViewUBOCommitSystem.cpp"
    "systems/render/collect/LineStatsSystem.cpp"
    "systems/render/collect/RenderCollectInternal.cpp"
    "systems/render/collect/RenderCollectSystem.cpp"
    "systems/render/collect/RenderCollect_CacheReplay.cpp"
    "systems/render/collect/RenderCollect_PrimitiveNormalize.cpp"
    "systems/render/collect/SimpleLineStatsSystem.cpp"
    "systems/render/frame/RenderSystemCore.cpp"
    "systems/render/frame/RenderTargetSystem.cpp"
    "systems/render/frame/SwapchainNextImageSystem.cpp"
    "systems/render/frame/SwapchainSubmitSystem.cpp"
    "systems/render/light_shadow/ColorPaletteSystem.cpp"
    "systems/render/light_shadow/EnvironmentSystem.cpp"
    "systems/render/light_shadow/LightSystem.cpp"
    "systems/render/light_shadow/MainLightShadowSystem.cpp"
    "systems/render/light_shadow/SkyEnvironmentConfig.cpp"
    "systems/render/streaming/GeometryStreamingSystem.cpp"
    "systems/render/streaming/ModelStreamingSystem.cpp"
    "systems/render/streaming/TextureStreamingSystem.cpp"
    "systems/tick/CameraSystem.cpp"
    "systems/tick/JoltPhysicsSystem.cpp"
    "systems/tick/LineBoundsUpdateSystem.cpp"
    "systems/tick/SimpleLineBoundsUpdateSystem.cpp"
    "systems/tick/Transform2DSystem.cpp"
    "systems/tick/TransformSystem.cpp"
)

set(ECS_RENDER_HEADERS
    "components/CameraComponent.h"
    "components/GPUModelRefComponent.h"
    "components/GeometryData.h"
    "components/ModelRefComponent.h"
    "components/TextComponent.h"
    "core/Context.h"
    "core/FrameLaneWaitSet.h"
    "core/render/MaterialBatch.h"
    "core/render/ModelTemplateRenderItem.h"
    "core/render/PrimitiveRenderItem.h"
    "core/render/RenderFrameCache.h"
    "core/render/RenderItem.h"
    "core/render/RenderPassRequest.h"
    "core/render/ShaderProgramPipelineKey.h"
    "support/assembly/ModelAssemblyStage.h"
    "support/assembly/StaticMeshModelInstance.h"
    "support/cull/CPUCullReferenceSystem.h"
    "support/cull/GPUCullSystem.h"
    "support/cull/ModelGroupRoutingTables.h"
    "support/debug/DebugViewController.h"
    "support/debug/DebugViewPass.h"
    "support/hierarchy2d/Sprite2DSyncSystem.h"
    "support/line/LineRenderPipeline.h"
    "support/line/SimpleLineRenderPipeline.h"
    "support/material/MaterialAuthoringSideTable.h"
    "support/material/MaterialRuntimePipelineTable.h"
    "support/material/MaterialRuntimeTable.h"
    "support/material/MaterializationEpochState.h"
    "support/material/ModelTemplateBuilder.h"
    "support/material/ModelTemplateMaterialTable.h"
    "support/pipeline/MaterialBatchPlanCache.h"
    "support/pipeline/PipelineMaterialRenderer.h"
    "support/pipeline/PrimitiveState.h"
    "support/pipeline/RenderBatchPipeline.h"
    "support/pipeline/RenderableBuilder.h"
    "support/primitive/PrimitiveRenderPipeline.h"
    "support/render/WorldRenderData.h"
    "support/render/WorldGPUResources.h"
    "support/storage/CameraInfoStorage.h"
    "support/storage/CameraSlotGuard.h"
    "support/storage/EntityRenderIDTable.h"
    "support/storage/LightRowTable.h"
    "support/text/TextRenderPipeline.h"
    "support/transform/TransformAssignmentBuffer.h"
    "systems/render/collect/RenderCollectSystem.h"
    "systems/render/frame/RenderSystemCore.h"
    "systems/render/light_shadow/ColorPaletteSystem.h"
    "systems/render/light_shadow/EnvironmentSystem.h"
    "systems/render/light_shadow/LightSystem.h"
    "systems/render/light_shadow/MainLightShadowSystem.h"
    "systems/render/light_shadow/SkyEnvironmentConfig.h"
    "systems/render/streaming/ModelStreamingSystem.h"
    "systems/render/streaming/TextureStreamingSystem.h"
    "systems/tick/CameraSystem.h"
    "systems/tick/JoltPhysicsSystem.h"
    "systems/tick/TransformSystem.h"
)

# ── 按清单把 glob 结果分流（保持既有 IDE 分组/测试收集逻辑不变）──
set(ECS_CORE_SOURCES_ABS)
set(ECS_RENDER_SOURCES_ABS)
foreach(_f IN LISTS ECS_SOURCES)
    file(RELATIVE_PATH _rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_f}")
    if(_rel IN_LIST ECS_CORE_SOURCES)
        list(APPEND ECS_CORE_SOURCES_ABS "${_f}")
    else()
        list(APPEND ECS_RENDER_SOURCES_ABS "${_f}")
    endif()
endforeach()

set(ECS_CORE_HEADERS_ABS)
set(ECS_RENDER_HEADERS_ABS)
foreach(_f IN LISTS ECS_HEADERS)
    file(RELATIVE_PATH _rel "${ECS_INCLUDE_PATH}" "${_f}")
    if(_rel IN_LIST ECS_CORE_HEADERS)
        list(APPEND ECS_CORE_HEADERS_ABS "${_f}")
    else()
        list(APPEND ECS_RENDER_HEADERS_ABS "${_f}")
    endif()
endforeach()

# fail-fast：清单里登记了但磁盘上不存在的条目（改名/删除后忘记同步）直接报错。
foreach(_rel IN LISTS ECS_CORE_SOURCES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_rel}")
        message(FATAL_ERROR "ULRE.ECS: ECS_CORE_SOURCES 登记的源文件不存在：${_rel}（改名后请同步清单）")
    endif()
endforeach()
foreach(_rel IN LISTS ECS_CORE_HEADERS)
    if(NOT EXISTS "${ECS_INCLUDE_PATH}/${_rel}")
        message(FATAL_ERROR "ULRE.ECS: ECS_CORE_HEADERS 登记的头文件不存在：${_rel}（改名后请同步清单）")
    endif()
endforeach()

list(LENGTH ECS_CORE_SOURCES _core_src_count)
list(LENGTH ECS_CORE_SOURCES_ABS _core_src_found)
if(NOT _core_src_count EQUAL _core_src_found)
    message(FATAL_ERROR
        "ULRE.ECS: ECS_CORE_SOURCES 有 ${_core_src_count} 项登记，但只分流到 ${_core_src_found} 个文件"
        "（清单路径必须相对于 src/ecs 且与实际文件一致）")
endif()

# Core：零图形依赖的内核。
add_cm_library(ULRE.ECS.Core "ULRE" ${ECS_INCLUDE_PATH}
    ${ECS_CORE_SOURCES_ABS} ${ECS_CORE_HEADERS_ABS})

# Render：ECS 的图形集成侧（收集/批处理/管线/GPU 表/相机 GPU 行）。
add_cm_library(ULRE.ECS.Render "ULRE" ${ECS_INCLUDE_PATH}
    ${ECS_RENDER_SOURCES_ABS} ${ECS_RENDER_HEADERS_ABS})

# Umbrella：既有消费方（WorldAssembly、根聚合、测试）继续链接 ULRE.ECS。
add_library(ULRE.ECS INTERFACE)
target_link_libraries(ULRE.ECS INTERFACE ULRE.ECS.Core ULRE.ECS.Render)

find_package(Jolt CONFIG REQUIRED)
# CMCore 是 PUBLIC：Core 的公开头直接用 AnsiString/ValueArray，且实现用 hgl::logger。
# 独立链接 ULRE.ECS.Core 的消费方（如 TestECSCoreBoundary）不应再自己猜这些依赖。
target_link_libraries(ULRE.ECS.Core PUBLIC Jolt::Jolt CMCore CMMath ULRE.Debug)
# Render 的图形集成依赖显式声明：ECS 不再是「靠根目标平铺链接才编得过」的库。
target_link_libraries(ULRE.ECS.Render
    PUBLIC
        ULRE.ECS.Core
        ULRE.SceneGraph
        ULRE.Vulkan
        ULRE.ShaderMaterialData)

# 只在「强制打开」时下发；不下发时由 `inc/hgl/common/RenderOptions.h` §2 按 NDEBUG 决定（Debug 1 / Release 0）。
# ⚠ 不要在这里下发 `=0`：那会压过台帐的 Debug 默认值，让 Debug 也丢掉校验面（2026-10-13 统一时删掉的分支）。
if(ULRE_ECS_DEBUG_API)
    target_compile_definitions(ULRE.ECS.Core   PUBLIC ULRE_ECS_DEBUG_API=1)
    target_compile_definitions(ULRE.ECS.Render PUBLIC ULRE_ECS_DEBUG_API=1)
endif()

# CPU 剔除参考台架（每 60 帧 GPU 读回对拍；读回 stall ⇒ 默认关，生产不付这笔钱）。
# 台帐默认值见 `inc/hgl/common/RenderOptions.h` §2（`ULRE_ENABLE_CPU_CULL_PARITY_TEST`，默认 0）。
# 判定一律写 `#if ULRE_ENABLE_CPU_CULL_PARITY_TEST` —— 禁 `defined()`（值为 0 时它同样为真）。
if(ULRE_ENABLE_CPU_CULL_PARITY_TEST)
    target_compile_definitions(ULRE.ECS.Core   PRIVATE ULRE_ENABLE_CPU_CULL_PARITY_TEST=1)
    target_compile_definitions(ULRE.ECS.Render PRIVATE ULRE_ENABLE_CPU_CULL_PARITY_TEST=1)
endif()

# ── ECS 测试可执行目标开关 ────────────────────────────────────────────────────
# 历史上 `add_executable` **无条件**执行（只有 `add_test` 包在 BUILD_TESTING 里）⇒
# 即便 `-DBUILD_TESTING=OFF`，本目录仍会生成 60+ 个测试 exe 并进入 ALL_BUILD，
# 是「日常构建/增量链接慢」的主要来源之一。
#
# 默认 ON（与既有行为一致，ctest 直接可用）；日常只编引擎时用
#   cmake -S . -B build -DULRE_BUILD_ECS_TEST_TARGETS=OFF
# 即可让这些目标彻底不出现在解决方案与 ALL_BUILD 里。
option(ULRE_BUILD_ECS_TEST_TARGETS "构建 ECS 测试可执行目标（关闭 = 不参与 ALL_BUILD，可显著缩短日常构建）" ON)

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

# 纯 CPU/数据结构单测：不链接 ULRE::Engine，只链接 CMCore 与被测源。
function(add_ulre_unit_test target_name)
    cmake_parse_arguments(UNIT_TEST "" "" "EXTRA_SOURCES;LIBS" ${ARGN})
    add_executable(${target_name} ${UNIT_TEST_UNPARSED_ARGUMENTS} ${UNIT_TEST_EXTRA_SOURCES})
    target_link_libraries(${target_name} PRIVATE CMCore ${UNIT_TEST_LIBS})
    set_target_properties(${target_name} PROPERTIES FOLDER "ULRE/Tests/ECS/Unit")
endfunction()

if(BUILD_TESTING AND ULRE_BUILD_ECS_TEST_TARGETS)
add_ulre_unit_test(TestBindlessSlotAllocator tests/TestBindlessSlotAllocator.cpp)

# Core 层边界闸门：**只**链接 ULRE.ECS.Core（不链接 ULRE::Engine）。
# Core 头一旦重新引入 hgl/graph、hgl/vk 或 vulkan 头，本目标编不过；
# Core 实现若把符号落在 Render 侧，本目标链接失败。
add_executable(TestECSCoreBoundary tests/TestECSCoreBoundary.cpp)
target_link_libraries(TestECSCoreBoundary PRIVATE ULRE.ECS.Core)
set_target_properties(TestECSCoreBoundary PROPERTIES FOLDER "ULRE/Tests/ECS")

add_ecs_test(TestTransformFlatStorage tests/TestTransformFlatStorage.cpp)

add_ecs_test(TestFrameDebugger tests/TestFrameDebugger.cpp)
add_ecs_test(TestDebugBridgeLifecycle tests/TestDebugBridgeLifecycle.cpp)
# 该测试直接包含 DebugBridgeProtocol.h（→ CMUtil JsonTool.h → nlohmann/json.hpp），
# 与根 CMakeLists 的 toml11/nlohmann_json 显式认领惯例一致，须自行链接 nlohmann_json。
target_link_libraries(TestDebugBridgeLifecycle PRIVATE nlohmann_json::nlohmann_json)
add_ecs_test(TestDebugViewLifecycle tests/TestDebugViewLifecycle.cpp)

add_ecs_test(TestCSMIncrementalPass tests/TestCSMIncrementalPass.cpp)
add_ecs_test(TestGraphicsContextShutdownCallbacks tests/TestGraphicsContextShutdownCallbacks.cpp)

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

# 矩形(RECT)文本排版：折行 / 单词边界 / 省略号 / 高度裁剪 / 纯测量 / 组件接口。
add_ecs_test(TestTextRectLayout tests/TestTextRectLayout.cpp)

# 单色点阵字体源（VGA8.F16 8x16、FM-T-437.F08 8x8 等）：参数化宽高加载与度量。
add_ecs_test(TestMonoBitmapFontSource tests/TestMonoBitmapFontSource.cpp)

# 环境资产规约（`*.env.toml`）：五组字段往返 + `Presence` 三态 + 枚举拼写别名
# + `[[sh9.bands]]` 顺序 + 元数据 struct_meta/elem_kind 接线。
add_ecs_test(TestEnvironmentCodec tests/TestEnvironmentCodec.cpp)

# 环境资产表（`env:` URI）：身份 / 按名查询 / 缓存 / 扩展名与类型门禁
# + `ApplyEnvironmentSpec` 的 `Presence` 局部覆盖语义。
add_ecs_test(TestEnvironmentAssetURI tests/TestEnvironmentAssetURI.cpp)

# 配方差量继承（`parent`）：合并口径 + 注册表两阶段加载。
add_ecs_test(TestMaterialRecipeInheritance tests/TestMaterialRecipeInheritance.cpp)

# 材质异步解析落地层（Tier 2 工作线程解析 → Tier 3 主线程提交）：
# 挂接/摘除契约 + 跨任务父子继承推进 + 坏输入留诊断不崩。
add_ecs_test(TestMaterialAsyncCompiler tests/TestMaterialAsyncCompiler.cpp)

# 材质装载车道（池 + 编译器 + 泵送节奏封装）：懒启动幂等 / 整目录投递 /
# 门禁语义 / 收尾 Drain 不丢成品 / 无注册表收尾 / 收尾后重启。
add_ecs_test(TestMaterialAsyncLoadLane tests/TestMaterialAsyncLoadLane.cpp)

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

# TOML LevelSpec parser: schema, references, type-specific keys, and parent cycles.
add_ecs_test(TestLevelFile tests/TestLevelFile.cpp)

# 阶段二：多材质场景装载的资产级材质下标透传 + 实例化行选择逻辑（纯数据用例）。
add_ecs_test(TestStaticMeshMultiMaterial tests/TestStaticMeshMultiMaterial.cpp)

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

# EntityID 世代宽度判据：同槽位跨 2^16 复用后旧句柄必须失效（uint32 世代，结构体仍 8 字节）
add_ecs_test(TestEntityHandleGeneration tests/TestEntityHandleGeneration.cpp)

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

# 步骤4 判据：帧提交等待列表（车道累积/提升/取走清空/主帧收尾/超限截断）
add_ecs_test(TestFrameLaneWaitSet tests/TestFrameLaneWaitSet.cpp)

# 步骤7 判据：模型展开路由表（一级/二级索引规则、pass 分区隔离、容量边界、越界丢弃）
add_ecs_test(TestModelGroupRoutingTables tests/TestModelGroupRoutingTables.cpp)

# 步骤7 判据：批分区暂存（pass 隔离、行字段、容量边界、批号越界丢弃）
add_ecs_test(TestBatchLayoutStaging tests/TestBatchLayoutStaging.cpp)

# 步骤9 判据：上传任务调度（id 分配、撤销三态、完成提取、staging 背压、队列/在飞/注销）
add_ecs_test(TestUploadTaskScheduler tests/TestUploadTaskScheduler.cpp)

# §3-②/⑥ 判据：相机纯读（Resolve）与认领（Ensure）分离（+ 实体级相机模板编译期禁死）
add_ecs_test(TestCameraResolveAndClaim tests/TestCameraResolveAndClaim.cpp)

# 阶段三判据：资产装载顺序图（后缀规范化 / 引用计数 / 拓扑闭包 / 有环终止）
add_ecs_test(TestAssetLoadGraph tests/TestAssetLoadGraph.cpp)

# 阶段三判据：不装载也能抽出场景/配方依赖（内存内合成 MDL2 包与配方文本）
add_ecs_test(TestModelDependencyExtractor tests/TestModelDependencyExtractor.cpp)

# 阶段三判据：模型包 + 配方 → 任务单（去重 / 拓扑序 / 配方缺失降级 / 坏包拒绝）
add_ecs_test(TestModelLoadPlan tests/TestModelLoadPlan.cpp)

# 阶段四判据：异步场景句柄的状态机与句柄表去重（不碰设备）
add_ecs_test(TestModelAsset tests/TestModelAsset.cpp)

# 阶段四判据：流送系统在无设备时的守卫与外部设定（不碰设备）
add_ecs_test(TestModelStreamingSystem tests/TestModelStreamingSystem.cpp)

# 落地A判据：ModelRefComponent 自动装配的请求段守卫与终态（策略拦截/防悬垂/不重试；不碰设备）
add_ecs_test(TestModelAutoInstantiate tests/TestModelAutoInstantiate.cpp)

# 阶段五判据：三段渐进激活（骨架 → 几何 → 材质）的顺序守约与骨架包围盒
add_ecs_test(TestStaticMeshModelProgressive tests/TestStaticMeshModelProgressive.cpp)

# 阶段五 Step 5.2 判据：必需纹理的「在途 / 永久失败 / 就绪」三值结论（占位保底的门禁规则）
add_ecs_test(TestMaterialTextureFallback tests/TestMaterialTextureFallback.cpp)

# 阶段四判据：包裹取流统一进 LoadedFileManager（索引优先解析、挂载根查询、字节源取流、失败与合并语义）
add_ecs_test(TestAssetURIByteSource tests/TestAssetURIByteSource.cpp)

# 落地T2判据：GPU 模型引用/批次组件语义 + 动静 TRS 接入约定（不碰设备）
add_ecs_test(TestGPUModelComponents tests/TestGPUModelComponents.cpp)

# 落地T5.1判据：模板/部件行布局、常量口径、GLSL 镜像逐字段对拍（不碰设备）
add_ecs_test(TestGPUModelTemplate tests/TestGPUModelTemplate.cpp)

# 落地T5.2判据：逐部件展开的 CPU 参考实现 vs 着色器口径（矩阵顺序/LOD 选级/阴影代理，不碰设备）
add_ecs_test(TestGPUModelExpansionParity tests/TestGPUModelExpansionParity.cpp)

# 落地T5.3判据：模板实例化的聚合账与 ICB 命令语义（50 实例共享桶组，不碰设备）
add_ecs_test(TestGPUModelExpansionAggregation tests/TestGPUModelExpansionAggregation.cpp)

# 阶段八判据：10,000 实例复合模型压力测试基准（内存对比/CPU更新加速/ICB命令聚合/GPU展开段吞吐仿真）
add_ecs_test(TestGPUModelStress tests/TestGPUModelStress.cpp)

# Stage 1 多材质模型模板（Step 1.4）：材质表生命周期、载体项与源码契约
add_ecs_test(TestMultiMaterialModelTemplate tests/TestMultiMaterialModelTemplate.cpp)

# JoltPhysics 物理引擎对接与 ModelCollisionProxyComponent 集成测试
add_ecs_test(TestJoltPhysicsIntegration tests/TestJoltPhysicsIntegration.cpp
    LIBS Jolt::Jolt)

# 范例 1~4 场景层（PhysicsScenarios）的无头数值验证：堆叠/斜坡滑滚/运动学平台/射线与传感器
add_ecs_test(TestPhysicsScenarios tests/TestPhysicsScenarios.cpp
    EXTRA_SOURCES tests/PhysicsScenarios.cpp
    LIBS Jolt::Jolt)

if(BUILD_TESTING)
    add_test(NAME TestFrameDebugger COMMAND TestFrameDebugger
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(TestFrameDebugger PROPERTIES LABELS "debug;capture")
    add_test(NAME TestDebugBridgeLifecycle COMMAND TestDebugBridgeLifecycle
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(TestDebugBridgeLifecycle PROPERTIES LABELS "debug;bridge" TIMEOUT 20)
    add_test(NAME TestDebugViewLifecycle COMMAND TestDebugViewLifecycle
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(TestDebugViewLifecycle PROPERTIES LABELS "debug;view")

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
        NAME TestGraphicsContextShutdownCallbacks
        COMMAND TestGraphicsContextShutdownCallbacks)
    set_tests_properties(
        TestGraphicsContextShutdownCallbacks
        PROPERTIES LABELS "ecs;graphics")

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
        NAME TestMaterialAsyncCompiler
        COMMAND TestMaterialAsyncCompiler
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestMaterialAsyncCompiler
        PROPERTIES LABELS "material;recipe;async" TIMEOUT 120)

    add_test(
        NAME TestMaterialAsyncLoadLane
        COMMAND TestMaterialAsyncLoadLane
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestMaterialAsyncLoadLane
        PROPERTIES LABELS "material;recipe;asset;async" TIMEOUT 120)

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
        NAME TestLevelFile
        COMMAND TestLevelFile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestLevelFile
        PROPERTIES LABELS "scene;asset")

    add_test(
        NAME TestStaticMeshMultiMaterial
        COMMAND TestStaticMeshMultiMaterial
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestStaticMeshMultiMaterial
        PROPERTIES LABELS "scene;asset;material")

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
        NAME TestEntityHandleGeneration
        COMMAND TestEntityHandleGeneration)
    set_tests_properties(
        TestEntityHandleGeneration
        PROPERTIES LABELS "ecs;entity" TIMEOUT 120)

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
        NAME TestFrameLaneWaitSet
        COMMAND TestFrameLaneWaitSet)
    set_tests_properties(
        TestFrameLaneWaitSet
        PROPERTIES LABELS "ecs;render" TIMEOUT 60)

    add_test(
        NAME TestModelGroupRoutingTables
        COMMAND TestModelGroupRoutingTables)
    set_tests_properties(
        TestModelGroupRoutingTables
        PROPERTIES LABELS "ecs;render;cull" TIMEOUT 60)

    add_test(
        NAME TestBatchLayoutStaging
        COMMAND TestBatchLayoutStaging)
    set_tests_properties(
        TestBatchLayoutStaging
        PROPERTIES LABELS "ecs;render;cull" TIMEOUT 60)

    add_test(
        NAME TestUploadTaskScheduler
        COMMAND TestUploadTaskScheduler)
    set_tests_properties(
        TestUploadTaskScheduler
        PROPERTIES LABELS "graphics;upload" TIMEOUT 60)

    add_test(
        NAME TestCameraResolveAndClaim
        COMMAND TestCameraResolveAndClaim)
    set_tests_properties(
        TestCameraResolveAndClaim
        PROPERTIES LABELS "ecs;camera" TIMEOUT 60)

    add_test(
        NAME TestAssetLoadGraph
        COMMAND TestAssetLoadGraph)
    set_tests_properties(
        TestAssetLoadGraph
        PROPERTIES LABELS "ecs;asset" TIMEOUT 60)

    add_test(
        NAME TestModelDependencyExtractor
        COMMAND TestModelDependencyExtractor)
    set_tests_properties(
        TestModelDependencyExtractor
        PROPERTIES LABELS "ecs;asset" TIMEOUT 60)

    add_test(
        NAME TestModelLoadPlan
        COMMAND TestModelLoadPlan)
    set_tests_properties(
        TestModelLoadPlan
        PROPERTIES LABELS "ecs;asset" TIMEOUT 60)

    add_test(
        NAME TestModelAsset
        COMMAND TestModelAsset)
    set_tests_properties(
        TestModelAsset
        PROPERTIES LABELS "ecs;asset" TIMEOUT 60)

    add_test(
        NAME TestModelStreamingSystem
        COMMAND TestModelStreamingSystem)
    set_tests_properties(
        TestModelStreamingSystem
        PROPERTIES LABELS "ecs;asset;streaming" TIMEOUT 60)

    add_test(
        NAME TestStaticMeshModelProgressive
        COMMAND TestStaticMeshModelProgressive)
    set_tests_properties(
        TestStaticMeshModelProgressive
        PROPERTIES LABELS "ecs;scene;streaming" TIMEOUT 60)

    add_test(
        NAME TestMaterialTextureFallback
        COMMAND TestMaterialTextureFallback)
    set_tests_properties(
        TestMaterialTextureFallback
        PROPERTIES LABELS "ecs;material;streaming" TIMEOUT 60)

    add_test(
        NAME TestAssetURIByteSource
        COMMAND TestAssetURIByteSource
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestAssetURIByteSource
        PROPERTIES LABELS "asset;pack;async" TIMEOUT 120)

    add_test(
        NAME TestGPUModelComponents
        COMMAND TestGPUModelComponents
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestGPUModelComponents
        PROPERTIES LABELS "ecs;gpu-model" TIMEOUT 60)

    add_test(
        NAME TestGPUModelTemplate
        COMMAND TestGPUModelTemplate
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestGPUModelTemplate
        PROPERTIES LABELS "ecs;gpu-model" TIMEOUT 60)

    add_test(
        NAME TestGPUModelExpansionParity
        COMMAND TestGPUModelExpansionParity
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestGPUModelExpansionParity
        PROPERTIES LABELS "ecs;gpu-model" TIMEOUT 60)

    add_test(
        NAME TestGPUModelExpansionAggregation
        COMMAND TestGPUModelExpansionAggregation
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestGPUModelExpansionAggregation
        PROPERTIES LABELS "ecs;gpu-model" TIMEOUT 60)

    add_test(
        NAME TestGPUModelStress
        COMMAND TestGPUModelStress
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestGPUModelStress
        PROPERTIES LABELS "ecs;gpu-model" TIMEOUT 120)

    add_test(
        NAME TestMultiMaterialModelTemplate
        COMMAND TestMultiMaterialModelTemplate
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestMultiMaterialModelTemplate
        PROPERTIES LABELS "ecs;gpu-model" TIMEOUT 60)

    add_test(
        NAME TestJoltPhysicsIntegration
        COMMAND TestJoltPhysicsIntegration
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestJoltPhysicsIntegration
        PROPERTIES LABELS "ecs;physics;jolt" TIMEOUT 60)

    add_test(
        NAME TestPhysicsScenarios
        COMMAND TestPhysicsScenarios
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        TestPhysicsScenarios
        PROPERTIES LABELS "ecs;physics;jolt" TIMEOUT 120)
endif()

endif()  # BUILD_TESTING AND ULRE_BUILD_ECS_TEST_TARGETS
