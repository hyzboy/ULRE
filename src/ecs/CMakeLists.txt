SET(ECS_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/ecs)

option(ULRE_ECS_DEBUG_API "Enable ECS debug API surface" OFF)
option(ULRE_ENABLE_CPU_CULL_PARITY_TEST "注册 CPU 剔除参考台架（每 60 帧 GPU 读回对拍）；默认关，生产管线不付那笔 stall" OFF)

set(ECS_CORE_FILES
    ${ECS_INCLUDE_PATH}/core/Object.h
    ${ECS_INCLUDE_PATH}/core/ComponentRow.h
    ${ECS_INCLUDE_PATH}/core/Entity.h
    ${ECS_INCLUDE_PATH}/core/EntityHandle.h
    ${ECS_INCLUDE_PATH}/core/EntityManager.h
    ${ECS_INCLUDE_PATH}/core/System.h
    ${ECS_INCLUDE_PATH}/core/Context.h
    ${ECS_INCLUDE_PATH}/core/RenderGraph.h
    ${ECS_INCLUDE_PATH}/core/RenderPassRequest.h
    ${ECS_INCLUDE_PATH}/core/ScenePipelineMode.h
    ${ECS_INCLUDE_PATH}/core/SystemGroup.h
    ${ECS_INCLUDE_PATH}/core/DefaultSystems.h
    ${ECS_INCLUDE_PATH}/core/MaterialBatch.h
    ${ECS_INCLUDE_PATH}/core/ShaderProgramPipelineKey.h
    ${ECS_INCLUDE_PATH}/core/RenderItem.h
    ${ECS_INCLUDE_PATH}/core/PrimitiveRenderItem.h
    ${ECS_INCLUDE_PATH}/core/RenderFrameCache.h
    ${ECS_INCLUDE_PATH}/systems/render/RenderSystemCore.h
    core/Object.cpp
    core/ComponentRow.cpp
    core/Entity.cpp
    core/EntityManager.cpp
    core/System.cpp
    core/Context.cpp
    core/ContextRender.cpp
    core/ContextGPU.cpp
    core/ContextSystems.cpp
    core/RenderFrameCache.cpp
    support/RenderResource.cpp
    support/CameraInfoStorage.cpp
    support/CameraSlotGuard.cpp
    support/TransformAccessor.cpp
    core/RenderGraph.cpp
    core/SystemGroup.cpp
    core/DefaultSystems.cpp
    core/MaterialBatch.cpp
    core/PrimitiveRenderItem.cpp
    systems/render/RenderSystemCore.cpp
)

set(ECS_SYS_00_RenderSystemCore_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderSystemCore.h
    systems/render/RenderSystemCore.cpp
)

set(ECS_SYS_01_TickInput_InputSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/tick/InputSystem.h
    systems/tick/InputSystem.cpp
)

set(ECS_SYS_02_TickTransform_TransformSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/tick/TransformSystem.h
    systems/tick/TransformSystem.cpp
)

set(ECS_SYS_05_TickCamera_CameraSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/tick/CameraSystem.h
    ${ECS_INCLUDE_PATH}/systems/tick/CameraInputMapping.h
    systems/tick/CameraSystem.cpp
    systems/tick/CameraInputMapping.cpp
    ${ECS_INCLUDE_PATH}/components/CameraComponent.h
    components/CameraComponent.cpp
)

set(ECS_SYS_06B_TickTransform_LineBoundsUpdateSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/tick/LineBoundsUpdateSystem.h
    systems/tick/LineBoundsUpdateSystem.cpp
)

set(ECS_SYS_07_RenderSwapchainNextImage_SwapchainNextImageSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/SwapchainNextImageSystem.h
    systems/render/SwapchainNextImageSystem.cpp
)

set(ECS_SYS_08_RenderPreBeginFrame_RenderTargetSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderTargetSystem.h
    systems/render/RenderTargetSystem.cpp
)

set(ECS_SYS_09A_RenderPreBeginFrame_MainLightShadowSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/MainLightShadowSystem.h
    systems/render/MainLightShadowSystem.cpp
)

set(ECS_SYS_09C_RenderPreBeginFrame_LightSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/LightSystem.h
    systems/render/LightSystem.cpp
)

set(ECS_SYS_09_RenderPreBeginFrame_EnvironmentSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/EnvironmentSystem.h
    systems/render/EnvironmentSystem.cpp
)

set(ECS_SYS_09B_RenderPreBeginFrame_ColorPaletteSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/ColorPaletteSystem.h
    systems/render/ColorPaletteSystem.cpp
)

set(ECS_SYS_14_RenderBufferUpload_RenderBufferUploadSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderBufferUploadSystem.h
    systems/render/RenderBufferUploadSystem.cpp
)

set(ECS_SYS_13B_RenderBufferCommit_ViewUBOCommitSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/ViewUBOCommitSystem.h
    systems/render/ViewUBOCommitSystem.cpp
)


set(ECS_SYS_15B_RenderFrameSync_RenderSceneUBOSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderSceneUBOSystem.h
    systems/render/RenderSceneUBOSystem.cpp
)


set(ECS_SYS_16_RenderCollect_RenderPrimitiveCollectSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveCollectSystem.h
    systems/render/RenderPrimitiveCollectSystem.cpp
    ${ECS_INCLUDE_PATH}/components/MaterialData.h
    components/MaterialData.cpp
    ${ECS_INCLUDE_PATH}/components/GeometryData.h
    components/GeometryData.cpp
    ${ECS_INCLUDE_PATH}/components/ShadowProxy.h
    components/ShadowProxy.cpp
)

set(ECS_SYS_26B_RenderStat_LineStatsSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/LineStatsSystem.h
    systems/render/LineStatsSystem.cpp
)

set(ECS_SYS_27_RenderSubmit_SwapchainSubmitSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/SwapchainSubmitSystem.h
    systems/render/SwapchainSubmitSystem.cpp
)

set(ECS_SUPPORT_FILES
    ${ECS_INCLUDE_PATH}/support/TransformDataStorage.h
    ${ECS_INCLUDE_PATH}/support/TransformID.h
    ${ECS_INCLUDE_PATH}/support/ComponentPool.h
    ${ECS_INCLUDE_PATH}/support/EntityGPUTable.h
    ${ECS_INCLUDE_PATH}/support/EntityRenderIDTable.h
    ${ECS_INCLUDE_PATH}/support/DirtyRowSet.h
    ${ECS_INCLUDE_PATH}/support/GpuMirrorBuffer.h
    ${ECS_INCLUDE_PATH}/support/LightRowTable.h
    ${ECS_INCLUDE_PATH}/support/GPUCullSystem.h
    ${ECS_INCLUDE_PATH}/support/CPUCullReferenceSystem.h
    ${ECS_INCLUDE_PATH}/support/TransformAccessor.h
    ${ECS_INCLUDE_PATH}/support/CameraInfoStorage.h
    ${ECS_INCLUDE_PATH}/support/PerfTimer.h
    ${ECS_INCLUDE_PATH}/support/CameraSlotGuard.h
    ${ECS_INCLUDE_PATH}/support/RenderItemDataStorage.h
    ${ECS_INCLUDE_PATH}/support/DrawItemIDStorage.h
    ${ECS_INCLUDE_PATH}/support/DrawItemCompaction.h
    ${ECS_INCLUDE_PATH}/support/VisibilityDataStorage.h
    ${ECS_INCLUDE_PATH}/support/MaterialVariantTable.h
    ${ECS_INCLUDE_PATH}/support/MaterialRuntimeTable.h
    ${ECS_INCLUDE_PATH}/support/PrimitiveState.h
    ${ECS_INCLUDE_PATH}/support/RenderStrategyTable.h
    ${ECS_INCLUDE_PATH}/support/RenderStrategyParity.h
    ${ECS_INCLUDE_PATH}/support/BoundingBoxDataStorage.h
    ${ECS_INCLUDE_PATH}/support/BoundingBoxAccessor.h
    ${ECS_INCLUDE_PATH}/support/PipelineMaterialRenderer.h
    ${ECS_INCLUDE_PATH}/support/TransformAssignmentBuffer.h
    ${ECS_INCLUDE_PATH}/support/RenderPipelineBase.h
    ${ECS_INCLUDE_PATH}/support/RenderPipelineSystem.h
    ${ECS_INCLUDE_PATH}/support/PrimitiveBatchPipeline.h
    ${ECS_INCLUDE_PATH}/support/TextRenderPipeline.h
    support/RenderPipelineSystem.cpp
    support/RenderItemDataStorage.cpp
    support/DrawItemIDStorage.cpp
    support/DrawItemCompaction.cpp
    support/VisibilityDataStorage.cpp
    support/MaterialVariantTable.cpp
    support/MaterialRuntimeTable.cpp
    support/PrimitiveState.cpp
    support/RenderStrategyParity.cpp
    support/TransformAssignmentBuffer.cpp
    support/PipelineMaterialRenderer.cpp
    support/PrimitiveBatchPipeline.cpp
    support/GPUCullSystem.cpp
    support/CPUCullReferenceSystem.cpp
    support/text/TextRenderPipeline.cpp
)

set(ECS_SUPPORT_PRIMITIVE_FILES
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveRenderPipeline.h
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveCullSystem.h
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveSortSystem.h
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveBuildSystem.h
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveRenderSystem.h
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveOverlayRenderSystem.h
    support/primitive/PrimitiveRenderPipeline.cpp
    support/primitive/PrimitiveCullSystem.cpp
    support/primitive/PrimitiveSortSystem.cpp
    support/primitive/PrimitiveBuildSystem.cpp
    support/primitive/PrimitiveRenderSystem.cpp
    support/primitive/PrimitiveOverlayRenderSystem.cpp
)

set(ECS_SUPPORT_TEXT_FILES
    ${ECS_INCLUDE_PATH}/support/text/TextCollectSystem.h
    ${ECS_INCLUDE_PATH}/support/text/TextBuildSystem.h
    ${ECS_INCLUDE_PATH}/support/text/TextSyncSystem.h
    ${ECS_INCLUDE_PATH}/support/text/TextRenderSystem.h
    support/text/TextCollectSystem.cpp
    support/text/TextBuildSystem.cpp
    support/text/TextSyncSystem.cpp
    support/text/TextRenderSystem.cpp
)

set(ECS_SUPPORT_LINE_FILES
    ${ECS_INCLUDE_PATH}/support/line/LineRenderPipeline.h
    ${ECS_INCLUDE_PATH}/support/line/LineCollectSystem.h
    ${ECS_INCLUDE_PATH}/support/line/LineBuildSystem.h
    ${ECS_INCLUDE_PATH}/support/line/LineRenderSystem.h
    support/line/LineRenderPipeline.cpp
    support/line/LineCollectSystem.cpp
    support/line/LineBuildSystem.cpp
    support/line/LineRenderSystem.cpp
)

set(ECS_SOURCE
    ${ECS_CORE_FILES}
    ${ECS_SYS_00_RenderSystemCore_FILES}
    ${ECS_SYS_01_TickInput_InputSystem_FILES}
    ${ECS_SYS_02_TickTransform_TransformSystem_FILES}
    ${ECS_SYS_05_TickCamera_CameraSystem_FILES}
    ${ECS_SYS_06B_TickTransform_LineBoundsUpdateSystem_FILES}
    ${ECS_SYS_07_RenderSwapchainNextImage_SwapchainNextImageSystem_FILES}
    ${ECS_SYS_08_RenderPreBeginFrame_RenderTargetSystem_FILES}
    ${ECS_SYS_09_RenderPreBeginFrame_EnvironmentSystem_FILES}
    ${ECS_SYS_09A_RenderPreBeginFrame_MainLightShadowSystem_FILES}
    ${ECS_SYS_09B_RenderPreBeginFrame_ColorPaletteSystem_FILES}
    ${ECS_SYS_09C_RenderPreBeginFrame_LightSystem_FILES}

    ${ECS_SYS_14_RenderBufferUpload_RenderBufferUploadSystem_FILES}
    ${ECS_SYS_13B_RenderBufferCommit_ViewUBOCommitSystem_FILES}
    ${ECS_SYS_15B_RenderFrameSync_RenderSceneUBOSystem_FILES}
    ${ECS_SYS_16_RenderCollect_RenderPrimitiveCollectSystem_FILES}
    ${ECS_SYS_26B_RenderStat_LineStatsSystem_FILES}
    ${ECS_SYS_27_RenderSubmit_SwapchainSubmitSystem_FILES}
    ${ECS_SUPPORT_FILES}
    ${ECS_SUPPORT_PRIMITIVE_FILES}
    ${ECS_SUPPORT_TEXT_FILES}
    ${ECS_SUPPORT_LINE_FILES}
)

source_group("ECS\\Core" FILES ${ECS_CORE_FILES})
source_group("ECS\\Systems\\00 RenderSystemCore" FILES ${ECS_SYS_00_RenderSystemCore_FILES})
source_group("ECS\\Systems\\01 TickInput_InputSystem" FILES ${ECS_SYS_01_TickInput_InputSystem_FILES})
source_group("ECS\\Systems\\02 TickTransform_TransformSystem" FILES ${ECS_SYS_02_TickTransform_TransformSystem_FILES})
source_group("ECS\\Systems\\05 TickCamera_CameraSystem" FILES ${ECS_SYS_05_TickCamera_CameraSystem_FILES})
source_group("ECS\\Systems\\07 RenderSwapchainNextImage_SwapchainNextImageSystem" FILES ${ECS_SYS_07_RenderSwapchainNextImage_SwapchainNextImageSystem_FILES})
source_group("ECS\\Systems\\08 RenderPreBeginFrame_RenderTargetSystem" FILES ${ECS_SYS_08_RenderPreBeginFrame_RenderTargetSystem_FILES})
source_group("ECS\\Systems\\09 RenderPreBeginFrame_EnvironmentSystem" FILES ${ECS_SYS_09_RenderPreBeginFrame_EnvironmentSystem_FILES})
source_group("ECS\\Systems\\09A RenderPreBeginFrame_MainLightShadowSystem" FILES ${ECS_SYS_09A_RenderPreBeginFrame_MainLightShadowSystem_FILES})
source_group("ECS\\Systems\\09B RenderPreBeginFrame_ColorPaletteSystem" FILES ${ECS_SYS_09B_RenderPreBeginFrame_ColorPaletteSystem_FILES})
source_group("ECS\\Systems\\09C RenderPreBeginFrame_LightSystem" FILES ${ECS_SYS_09C_RenderPreBeginFrame_LightSystem_FILES})
source_group("ECS\\Systems\\13B RenderBufferCommit_ViewUBOCommitSystem" FILES ${ECS_SYS_13B_RenderBufferCommit_ViewUBOCommitSystem_FILES})
source_group("ECS\\Systems\\14 RenderBufferUpload_RenderBufferUploadSystem" FILES ${ECS_SYS_14_RenderBufferUpload_RenderBufferUploadSystem_FILES})
source_group("ECS\\Systems\\15B RenderFrameSync_RenderSceneUBOSystem" FILES ${ECS_SYS_15B_RenderFrameSync_RenderSceneUBOSystem_FILES})
source_group("ECS\\Systems\\16 RenderCollect_RenderPrimitiveCollectSystem" FILES ${ECS_SYS_16_RenderCollect_RenderPrimitiveCollectSystem_FILES})
source_group("ECS\\Systems\\26B RenderStat_LineStatsSystem" FILES ${ECS_SYS_26B_RenderStat_LineStatsSystem_FILES})
source_group("ECS\\Systems\\27 RenderSubmit_SwapchainSubmitSystem" FILES ${ECS_SYS_27_RenderSubmit_SwapchainSubmitSystem_FILES})
source_group("ECS\\Support" FILES ${ECS_SUPPORT_FILES})
source_group("ECS\\Support\\Primitive" FILES ${ECS_SUPPORT_PRIMITIVE_FILES})
source_group("ECS\\Support\\Text" FILES ${ECS_SUPPORT_TEXT_FILES})
source_group("ECS\\Support\\Line" FILES ${ECS_SUPPORT_LINE_FILES})

add_cm_library(ULRE.ECS "ULRE" ${ECS_INCLUDE_PATH}
${ECS_SOURCE})

if(ULRE_ECS_DEBUG_API)
    target_compile_definitions(ULRE.ECS PUBLIC ULRE_ECS_DEBUG_API=1)
else()
    target_compile_definitions(ULRE.ECS PUBLIC ULRE_ECS_DEBUG_API=0)
endif()

# 台架门（2026-10-09 补）：`DefaultSystems.cpp` 早就把 CPUCullReferenceSystem 放进
# `#if defined(ULRE_ENABLE_CPU_CULL_PARITY_TEST)`，但这个宏此前全仓无处定义 ⇒ 台架永远编不进来。
if(ULRE_ENABLE_CPU_CULL_PARITY_TEST)
    target_compile_definitions(ULRE.ECS PRIVATE ULRE_ENABLE_CPU_CULL_PARITY_TEST=1)
endif()

add_executable(TestTransformFlatStorage support/TestTransformFlatStorage.cpp)
target_link_libraries(TestTransformFlatStorage PRIVATE ${ULRE})
if(MSVC)
    set_target_properties(TestTransformFlatStorage PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

add_executable(TestRenderItemDataStorage support/TestRenderItemDataStorage.cpp)
target_link_libraries(TestRenderItemDataStorage PRIVATE ${ULRE})
if(MSVC)
    set_target_properties(TestRenderItemDataStorage PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

add_executable(TestCSMIncrementalPass support/TestCSMIncrementalPass.cpp)
target_link_libraries(TestCSMIncrementalPass PRIVATE ${ULRE})
if(MSVC)
    set_target_properties(TestCSMIncrementalPass PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# 包围盒 SoA 存储 + 值类型句柄（组件 → BoundingBoxDataStorage/BoundingBoxAccessor 改造）
add_executable(TestBoundingBoxStorage support/TestBoundingBoxStorage.cpp)
target_link_libraries(TestBoundingBoxStorage PRIVATE ${ULRE})
if(MSVC)
    set_target_properties(TestBoundingBoxStorage PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# 渲染策略判定表（A1；A7a 起为收集/阴影能力的**唯一判据**）
add_executable(TestRenderStrategyTable support/TestRenderStrategyTable.cpp support/RenderStrategyParity.cpp)
target_link_libraries(TestRenderStrategyTable PRIVATE ${ULRE})
target_compile_definitions(TestRenderStrategyTable PRIVATE ULRE_STRATEGY_PARITY_ENABLED=1)
if(MSVC)
    set_target_properties(TestRenderStrategyTable PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# A3 材质变体表（静态键去重 + 预算制 fail-fast）
add_executable(TestMaterialVariantTable support/TestMaterialVariantTable.cpp)
target_link_libraries(TestMaterialVariantTable PRIVATE ${ULRE})
if(MSVC)
    set_target_properties(TestMaterialVariantTable PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# A4 材质运行期表（共享行 interned + CoW + refcount + 世代 + 每实例 slot）
add_executable(TestMaterialRuntimeTable support/TestMaterialRuntimeTable.cpp)
target_link_libraries(TestMaterialRuntimeTable PRIVATE ${ULRE})
if(MSVC)
    set_target_properties(TestMaterialRuntimeTable PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

add_executable(TestComponentMask support/TestComponentMask.cpp)
target_link_libraries(TestComponentMask PRIVATE ${ULRE})
if(MSVC)
    set_target_properties(TestComponentMask PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

add_executable(TestVisibilityStorage support/TestVisibilityStorage.cpp)
target_link_libraries(TestVisibilityStorage PRIVATE ${ULRE})
if(MSVC)
    set_target_properties(TestVisibilityStorage PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

add_executable(TestBoundingVolumeCull support/TestBoundingVolumeCull.cpp)
target_link_libraries(TestBoundingVolumeCull PRIVATE ${ULRE})
if(MSVC)
    set_target_properties(TestBoundingVolumeCull PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# 诊断探针（T0 量化基线 + T4 旋转提取证据）：只打印数值，不做通过/失败判定
add_executable(ProbeTransformDiagnostics support/ProbeTransformDiagnostics.cpp)
target_link_libraries(ProbeTransformDiagnostics PRIVATE ${ULRE})
if(MSVC)
    set_target_properties(ProbeTransformDiagnostics PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

# 批 1.3 C5 判据探针：一次性告警闩必须**每实例**（进程级 static ⇒ 第二个世界静默）。
# 判据（外部）：跑完 grep -c "切片起点未 4 行对齐" ⇒ 期望 2（两个实例各 1）。
add_executable(ProbeWarnLatchPerWorld support/ProbeWarnLatchPerWorld.cpp)
target_link_libraries(ProbeWarnLatchPerWorld PRIVATE ${ULRE})
if(MSVC)
    set_target_properties(ProbeWarnLatchPerWorld PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
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
        NAME TestRenderItemDataStorage
        COMMAND TestRenderItemDataStorage)
    set_tests_properties(
        TestRenderItemDataStorage
        PROPERTIES LABELS "ecs;render_item")

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
        NAME TestComponentMask
        COMMAND TestComponentMask)
    set_tests_properties(
        TestComponentMask
        PROPERTIES LABELS "ecs;component_mask")

    add_test(
        NAME TestVisibilityStorage
        COMMAND TestVisibilityStorage)
    set_tests_properties(
        TestVisibilityStorage
        PROPERTIES LABELS "ecs;visibility")
endif()
