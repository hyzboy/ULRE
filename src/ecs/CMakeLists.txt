SET(ECS_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/ecs)

option(ULRE_ECS_DEBUG_API "Enable ECS debug API surface" ON)

set(ECS_CORE_FILES
    ${ECS_INCLUDE_PATH}/core/Object.h
    ${ECS_INCLUDE_PATH}/core/Component.h
    ${ECS_INCLUDE_PATH}/core/Entity.h
    ${ECS_INCLUDE_PATH}/core/EntityHandle.h
    ${ECS_INCLUDE_PATH}/core/EntityManager.h
    ${ECS_INCLUDE_PATH}/core/EntityQuery.h
    ${ECS_INCLUDE_PATH}/core/System.h
    ${ECS_INCLUDE_PATH}/core/SystemProfiler.h
    ${ECS_INCLUDE_PATH}/core/Context.h
    ${ECS_INCLUDE_PATH}/core/RenderGraph.h
    ${ECS_INCLUDE_PATH}/core/SystemGroup.h
    ${ECS_INCLUDE_PATH}/core/World.h
    ${ECS_INCLUDE_PATH}/core/DefaultSystems.h
    ${ECS_INCLUDE_PATH}/core/ComponentRecords.h
    ${ECS_INCLUDE_PATH}/core/MaterialBatch.h
    ${ECS_INCLUDE_PATH}/core/MaterialPipelineKey.h
    ${ECS_INCLUDE_PATH}/core/RenderItem.h
    ${ECS_INCLUDE_PATH}/core/PrimitiveRenderItem.h
    ${ECS_INCLUDE_PATH}/core/AssetPrimitiveRenderItem.h
    ${ECS_INCLUDE_PATH}/systems/render/RenderSystemCore.h
    core/Object.cpp
    core/Component.cpp
    core/Entity.cpp
    core/EntityManager.cpp
    core/EntityQuery.cpp
    core/System.cpp
    core/SystemProfiler.cpp
    core/Context.cpp
    core/RenderGraph.cpp
    core/SystemGroup.cpp
    core/World.cpp
    core/DefaultSystems.cpp
    core/MaterialBatch.cpp
    core/RenderItem.cpp
    core/PrimitiveRenderItem.cpp
    core/AssetPrimitiveRenderItem.cpp
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
    ${ECS_INCLUDE_PATH}/components/TransformComponent.h
    components/TransformComponent.cpp
)

set(ECS_SYS_03_TickTransform_BoundingBoxUpdateSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/tick/BoundingBoxUpdateSystem.h
    systems/tick/BoundingBoxUpdateSystem.cpp
    ${ECS_INCLUDE_PATH}/components/BoundingBoxComponent.h
    components/BoundingBoxComponent.cpp
)

set(ECS_SYS_04_TickTransform_VisibilitySystem_FILES
    ${ECS_INCLUDE_PATH}/systems/tick/VisibilitySystem.h
    systems/tick/VisibilitySystem.cpp
    ${ECS_INCLUDE_PATH}/components/VisibilityComponent.h
    components/VisibilityComponent.cpp
)

set(ECS_SYS_05_TickCamera_CameraSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/tick/CameraSystem.h
    ${ECS_INCLUDE_PATH}/systems/tick/CameraInputMapping.h
    systems/tick/CameraSystem.cpp
    systems/tick/CameraInputMapping.cpp
    ${ECS_INCLUDE_PATH}/components/CameraComponent.h
    components/CameraComponent.cpp
)

set(ECS_SYS_06_TickPostCamera_FacingTransformSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/transform/FacingTransformSystem.h
    systems/transform/FacingTransformSystem.cpp
    ${ECS_INCLUDE_PATH}/components/FacingTransformComponent.h
    components/FacingTransformComponent.cpp
)

set(ECS_SYS_06A_TickPostCamera_BillboardScaleSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/transform/BillboardScaleSystem.h
    systems/transform/BillboardScaleSystem.cpp
    ${ECS_INCLUDE_PATH}/components/BillboardScaleComponent.h
    components/BillboardScaleComponent.cpp
)

set(ECS_SYS_06B_TickTransform_LineBoundsUpdateSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/tick/LineBoundsUpdateSystem.h
    systems/tick/LineBoundsUpdateSystem.cpp
)

set(ECS_SYS_07_RenderSwapchainNextImage_SwapchainAcquireSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/SwapchainNextImageSystem.h
    systems/render/SwapchainNextImageSystem.cpp
)

set(ECS_SYS_08_RenderPreBeginFrame_RenderTargetSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderTargetSystem.h
    systems/render/RenderTargetSystem.cpp
)

set(ECS_SYS_09_RenderPreBeginFrame_EnvironmentSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/EnvironmentSystem.h
    systems/render/EnvironmentSystem.cpp
)

set(ECS_SYS_10_RenderResourceSetup_QuadResourcePrepareSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/QuadResourcePrepareSystem.h
    systems/render/QuadResourcePrepareSystem.cpp
)

set(ECS_SYS_10B_RenderMaterialBind_QuadMeshPrepareSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/QuadMeshPrepareSystem.h
    systems/render/QuadMeshPrepareSystem.cpp
    ${ECS_INCLUDE_PATH}/components/QuadMeshComponent.h
    components/QuadMeshComponent.cpp
)

set(ECS_SYS_10C_RenderMaterialBind_TextureMaterialBindingSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/TextureMaterialBindingSystem.h
    systems/render/TextureMaterialBindingSystem.cpp
)

set(ECS_SYS_11_RenderMaterialBind_QuadMaterialBindingSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/QuadMaterialBindingSystem.h
    systems/render/QuadMaterialBindingSystem.cpp
    ${ECS_INCLUDE_PATH}/components/QuadComponent.h
    components/QuadComponent.cpp
)

set(ECS_SYS_11B_RenderMaterialBind_MaterialResolveSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/MaterialResolveSystem.h
    systems/render/MaterialResolveSystem.cpp
    systems/render/internal/MaterialResolveInternals.h
    systems/render/internal/MaterialResolveInternals.cpp
    systems/render/internal/MaterialResolvePlanBuilder.h
    systems/render/internal/MaterialResolvePlanBuilder.cpp
)

set(ECS_SYS_11C_RenderMaterialBind_PrimitiveBindingCommitSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/PrimitiveBindingCommitSystem.h
    systems/render/PrimitiveBindingCommitSystem.cpp
)

set(ECS_SYS_14_RenderBufferUpload_RenderBufferUploadSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderBufferUploadSystem.h
    systems/render/RenderBufferUploadSystem.cpp
)

set(ECS_SYS_15_RenderFrameSync_RenderFrameBusinessSyncSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderFrameBusinessSyncSystem.h
    systems/render/RenderFrameBusinessSyncSystem.cpp
)

set(ECS_SYS_15B_RenderFrameSync_RenderDescriptorBindingSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderDescriptorBindingSystem.h
    systems/render/RenderDescriptorBindingSystem.cpp
)

if(ULRE_ECS_DEBUG_API)
    list(APPEND ECS_CORE_FILES
        core/ContextDebug.cpp
    )

    list(APPEND ECS_SYS_15B_RenderFrameSync_RenderDescriptorBindingSystem_FILES
        systems/render/RenderDescriptorBindingSystemDebugAPI.cpp
    )
endif()

set(ECS_SYS_16_RenderCollect_RenderPrimitiveCollectSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveCollectSystem.h
    systems/render/RenderPrimitiveCollectSystem.cpp
    ${ECS_INCLUDE_PATH}/components/PrimitiveComponent.h
    components/PrimitiveComponent.cpp
    ${ECS_INCLUDE_PATH}/components/RenderableComponent.h
    ${ECS_INCLUDE_PATH}/components/BillboardComponent.h
    components/BillboardComponent.cpp
)

set(ECS_SYS_16B_RenderCollect_AssetInstanceCollectSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/AssetInstanceCollectSystem.h
    systems/render/AssetInstanceCollectSystem.cpp
    ${ECS_INCLUDE_PATH}/components/AssetInstanceComponent.h
    components/AssetInstanceComponent.cpp
)

set(ECS_SYS_17_RenderCollect_RenderPrimitiveCullSystem_FILES
    # REMOVED: replaced by support/primitive/PrimitiveCullSystem
)

set(ECS_SYS_18_RenderCollect_TextCollectSystem_FILES
    # REMOVED: replaced by support/text/TextCollectSystem
)

set(ECS_SYS_18B_RenderCollect_LineCollectSystem_FILES
    # REMOVED: replaced by support/line/LineCollectSystem
)

set(ECS_SYS_19_RenderBatch_RenderPrimitiveSortSystem_FILES
    # REMOVED: replaced by support/primitive/PrimitiveSortSystem
)

set(ECS_SYS_20_RenderBatch_RenderPrimitiveBatchBuildSystem_FILES
    # REMOVED: replaced by support/primitive/PrimitiveBuildSystem
)

set(ECS_SYS_21_RenderBatch_RenderPrimitiveBatchFinalizeSystem_FILES
    # REMOVED: replaced by support/primitive/PrimitiveBuildSystem
)

set(ECS_SYS_22_RenderBatch_TextBuildSystem_FILES
    # REMOVED: replaced by support/text/TextBuildSystem
)

set(ECS_SYS_23_RenderBatch_TextResourceSyncSystem_FILES
    # REMOVED: replaced by support/text/TextSyncSystem
)

set(ECS_SYS_24_RenderDrawSubmit_RenderPrimitiveSubmitSystem_FILES
    # REMOVED: replaced by support/primitive/PrimitiveRenderSystem
)

set(ECS_SYS_25_RenderDrawSubmit_TextRenderSubmitSystem_FILES
    # REMOVED: replaced by support/text/TextRenderSystem
)

set(ECS_SYS_26_RenderBatch_LineRenderSystem_FILES
    # REMOVED: replaced by support/line/LineRenderSystem
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
    ${ECS_INCLUDE_PATH}/support/VisibilityDataStorage.h
    ${ECS_INCLUDE_PATH}/support/BoundingBoxDataStorage.h
    ${ECS_INCLUDE_PATH}/support/PipelineMaterialRenderer.h
    ${ECS_INCLUDE_PATH}/support/TransformAssignmentBuffer.h
    ${ECS_INCLUDE_PATH}/support/MaterialInstanceAssignmentBuffer.h
    ${ECS_INCLUDE_PATH}/support/TextHelper.h
    ${ECS_INCLUDE_PATH}/support/RenderPipelineBase.h
    ${ECS_INCLUDE_PATH}/support/RenderPipelineSystem.h
    ${ECS_INCLUDE_PATH}/support/RenderPipelineGroup.h
    ${ECS_INCLUDE_PATH}/support/PrimitiveBatchPipeline.h
    ${ECS_INCLUDE_PATH}/support/TextRenderPipeline.h
    support/RenderPipelineSystem.cpp
    support/RenderPipelineGroup.cpp
    support/VisibilityDataStorage.cpp
    support/TransformAssignmentBuffer.cpp
    support/MaterialInstanceAssignmentBuffer.cpp
    support/PipelineMaterialRenderer.cpp
    support/PrimitiveBatchPipeline.cpp
    support/TextRenderPipeline.cpp
)

set(ECS_SUPPORT_PRIMITIVE_FILES
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveRenderPipeline.h
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveCullSystem.h
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveSortSystem.h
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveBuildSystem.h
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveRenderSystem.h
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveOverlayRenderSystem.h
    ${ECS_INCLUDE_PATH}/support/primitive/PrimitiveRenderPipelineGroup.h
    support/primitive/PrimitiveRenderPipeline.cpp
    support/primitive/PrimitiveCullSystem.cpp
    support/primitive/PrimitiveSortSystem.cpp
    support/primitive/PrimitiveBuildSystem.cpp
    support/primitive/PrimitiveRenderSystem.cpp
    support/primitive/PrimitiveOverlayRenderSystem.cpp
    support/primitive/PrimitiveRenderPipelineGroup.cpp
)

set(ECS_SUPPORT_TEXT_FILES
    ${ECS_INCLUDE_PATH}/support/text/TextRenderPipelineAdapter.h
    ${ECS_INCLUDE_PATH}/support/text/TextCollectSystem.h
    ${ECS_INCLUDE_PATH}/support/text/TextBuildSystem.h
    ${ECS_INCLUDE_PATH}/support/text/TextSyncSystem.h
    ${ECS_INCLUDE_PATH}/support/text/TextRenderSystem.h
    ${ECS_INCLUDE_PATH}/support/text/TextRenderPipelineGroup.h
    support/text/TextRenderPipelineAdapter.cpp
    support/text/TextCollectSystem.cpp
    support/text/TextBuildSystem.cpp
    support/text/TextSyncSystem.cpp
    support/text/TextRenderSystem.cpp
    support/text/TextRenderPipelineGroup.cpp
)

set(ECS_SUPPORT_LINE_FILES
    ${ECS_INCLUDE_PATH}/support/line/LineRenderPipeline.h
    ${ECS_INCLUDE_PATH}/support/line/LineCollectSystem.h
    ${ECS_INCLUDE_PATH}/support/line/LineBuildSystem.h
    ${ECS_INCLUDE_PATH}/support/line/LineRenderSystem.h
    ${ECS_INCLUDE_PATH}/support/line/LineRenderPipelineGroup.h
    support/line/LineRenderPipeline.cpp
    support/line/LineCollectSystem.cpp
    support/line/LineBuildSystem.cpp
    support/line/LineRenderSystem.cpp
    support/line/LineRenderPipelineGroup.cpp
)

set(ECS_SUPPORT_BILLBOARD_FILES
    ${ECS_INCLUDE_PATH}/support/billboard/BillboardRenderPipelineGroup.h
    support/billboard/BillboardRenderPipelineGroup.cpp
)

set(ECS_SUPPORT_TERRAIN_FILES
    ${ECS_INCLUDE_PATH}/support/terrain/TerrainTileBuffer.h
    ${ECS_INCLUDE_PATH}/support/terrain/TerrainRenderPipeline.h
    ${ECS_INCLUDE_PATH}/support/terrain/TerrainRenderPipelineGroup.h
    ${ECS_INCLUDE_PATH}/support/terrain/TerrainCollectSystem.h
    ${ECS_INCLUDE_PATH}/support/terrain/TerrainBuildSystem.h
    ${ECS_INCLUDE_PATH}/support/terrain/TerrainRenderSystem.h
    support/terrain/TerrainTileBuffer.cpp
    support/terrain/TerrainRenderPipeline.cpp
    support/terrain/TerrainRenderPipelineGroup.cpp
    support/terrain/TerrainCollectSystem.cpp
    support/terrain/TerrainBuildSystem.cpp
    support/terrain/TerrainRenderSystem.cpp
)

set(ECS_SERIALIZATION_FILES
    serialization/ContextSerialization.cpp
    serialization/RenderableComponentSerialization.cpp
)

set(ECS_SOURCE
    ${ECS_CORE_FILES}
    ${ECS_COMPONENT_FILES}
    ${ECS_SYS_00_RenderSystemCore_FILES}
    ${ECS_SYS_01_TickInput_InputSystem_FILES}
    ${ECS_SYS_02_TickTransform_TransformSystem_FILES}
    ${ECS_SYS_03_TickTransform_BoundingBoxUpdateSystem_FILES}
    ${ECS_SYS_04_TickTransform_VisibilitySystem_FILES}
    ${ECS_SYS_05_TickCamera_CameraSystem_FILES}
    ${ECS_SYS_06_TickPostCamera_FacingTransformSystem_FILES}
    ${ECS_SYS_06A_TickPostCamera_BillboardScaleSystem_FILES}
    ${ECS_SYS_06B_TickTransform_LineBoundsUpdateSystem_FILES}
    ${ECS_SYS_07_RenderSwapchainNextImage_SwapchainAcquireSystem_FILES}
    ${ECS_SYS_08_RenderPreBeginFrame_RenderTargetSystem_FILES}
    ${ECS_SYS_09_RenderPreBeginFrame_EnvironmentSystem_FILES}
    ${ECS_SYS_10_RenderResourceSetup_QuadResourcePrepareSystem_FILES}
    ${ECS_SYS_10B_RenderMaterialBind_QuadMeshPrepareSystem_FILES}
    ${ECS_SYS_10C_RenderMaterialBind_TextureMaterialBindingSystem_FILES}
    ${ECS_SYS_11_RenderMaterialBind_QuadMaterialBindingSystem_FILES}
    ${ECS_SYS_11B_RenderMaterialBind_MaterialResolveSystem_FILES}
    ${ECS_SYS_11C_RenderMaterialBind_PrimitiveBindingCommitSystem_FILES}
    ${ECS_SYS_14_RenderBufferUpload_RenderBufferUploadSystem_FILES}
    ${ECS_SYS_15_RenderFrameSync_RenderFrameBusinessSyncSystem_FILES}
    ${ECS_SYS_15B_RenderFrameSync_RenderDescriptorBindingSystem_FILES}
    ${ECS_SYS_16_RenderCollect_RenderPrimitiveCollectSystem_FILES}
    ${ECS_SYS_16B_RenderCollect_AssetInstanceCollectSystem_FILES}
    ${ECS_SYS_17_RenderCollect_RenderPrimitiveCullSystem_FILES}
    ${ECS_SYS_18_RenderCollect_TextCollectSystem_FILES}
    ${ECS_SYS_18B_RenderCollect_LineCollectSystem_FILES}
    ${ECS_SYS_19_RenderBatch_RenderPrimitiveSortSystem_FILES}
    ${ECS_SYS_20_RenderBatch_RenderPrimitiveBatchBuildSystem_FILES}
    ${ECS_SYS_21_RenderBatch_RenderPrimitiveBatchFinalizeSystem_FILES}
    ${ECS_SYS_22_RenderBatch_TextBuildSystem_FILES}
    ${ECS_SYS_23_RenderBatch_TextResourceSyncSystem_FILES}
    ${ECS_SYS_24_RenderDrawSubmit_RenderPrimitiveSubmitSystem_FILES}
    ${ECS_SYS_25_RenderDrawSubmit_TextRenderSubmitSystem_FILES}
    ${ECS_SYS_26_RenderBatch_LineRenderSystem_FILES}
    ${ECS_SYS_26B_RenderStat_LineStatsSystem_FILES}
    ${ECS_SYS_27_RenderSubmit_SwapchainSubmitSystem_FILES}
    ${ECS_SUPPORT_FILES}
    ${ECS_SUPPORT_PRIMITIVE_FILES}
    ${ECS_SUPPORT_TEXT_FILES}
    ${ECS_SUPPORT_LINE_FILES}
    ${ECS_SUPPORT_BILLBOARD_FILES}
    ${ECS_SUPPORT_TERRAIN_FILES}
    ${ECS_SERIALIZATION_FILES}
)

source_group("ECS\\Core" FILES ${ECS_CORE_FILES})
source_group("ECS\\Components" FILES ${ECS_COMPONENT_FILES})
source_group("ECS\\Systems\\00 RenderSystemCore" FILES ${ECS_SYS_00_RenderSystemCore_FILES})
source_group("ECS\\Systems\\01 TickInput_InputSystem" FILES ${ECS_SYS_01_TickInput_InputSystem_FILES})
source_group("ECS\\Systems\\02 TickTransform_TransformSystem" FILES ${ECS_SYS_02_TickTransform_TransformSystem_FILES})
source_group("ECS\\Systems\\03 TickTransform_BoundingBoxUpdateSystem" FILES ${ECS_SYS_03_TickTransform_BoundingBoxUpdateSystem_FILES})
source_group("ECS\\Systems\\04 TickTransform_VisibilitySystem" FILES ${ECS_SYS_04_TickTransform_VisibilitySystem_FILES})
source_group("ECS\\Systems\\05 TickCamera_CameraSystem" FILES ${ECS_SYS_05_TickCamera_CameraSystem_FILES})
source_group("ECS\\Systems\\06 TickPostCamera_FacingTransformSystem" FILES ${ECS_SYS_06_TickPostCamera_FacingTransformSystem_FILES})
source_group("ECS\\Systems\\06A TickPostCamera_BillboardScaleSystem" FILES ${ECS_SYS_06A_TickPostCamera_BillboardScaleSystem_FILES})
source_group("ECS\\Systems\\07 RenderSwapchainNextImage_SwapchainAcquireSystem" FILES ${ECS_SYS_07_RenderSwapchainNextImage_SwapchainAcquireSystem_FILES})
source_group("ECS\\Systems\\08 RenderPreBeginFrame_RenderTargetSystem" FILES ${ECS_SYS_08_RenderPreBeginFrame_RenderTargetSystem_FILES})
source_group("ECS\\Systems\\09 RenderPreBeginFrame_EnvironmentSystem" FILES ${ECS_SYS_09_RenderPreBeginFrame_EnvironmentSystem_FILES})
source_group("ECS\\Systems\\10 RenderResourceSetup_QuadResourcePrepareSystem" FILES ${ECS_SYS_10_RenderResourceSetup_QuadResourcePrepareSystem_FILES})
source_group("ECS\\Systems\\10B RenderMaterialBind_QuadMeshPrepareSystem" FILES ${ECS_SYS_10B_RenderMaterialBind_QuadMeshPrepareSystem_FILES})
source_group("ECS\\Systems\\10C RenderMaterialBind_TextureMaterialBindingSystem" FILES ${ECS_SYS_10C_RenderMaterialBind_TextureMaterialBindingSystem_FILES})
source_group("ECS\\Systems\\11 RenderMaterialBind_QuadMaterialBindingSystem" FILES ${ECS_SYS_11_RenderMaterialBind_QuadMaterialBindingSystem_FILES})
source_group("ECS\\Systems\\11B RenderMaterialBind_MaterialResolveSystem" FILES ${ECS_SYS_11B_RenderMaterialBind_MaterialResolveSystem_FILES})
source_group("ECS\\Systems\\11C RenderMaterialBind_PrimitiveBindingCommitSystem" FILES ${ECS_SYS_11C_RenderMaterialBind_PrimitiveBindingCommitSystem_FILES})
source_group("ECS\\Systems\\14 RenderBufferUpload_RenderBufferUploadSystem" FILES ${ECS_SYS_14_RenderBufferUpload_RenderBufferUploadSystem_FILES})
source_group("ECS\\Systems\\15 RenderFrameSync_RenderFrameBusinessSyncSystem" FILES ${ECS_SYS_15_RenderFrameSync_RenderFrameBusinessSyncSystem_FILES})
source_group("ECS\\Systems\\15B RenderFrameSync_RenderDescriptorBindingSystem" FILES ${ECS_SYS_15B_RenderFrameSync_RenderDescriptorBindingSystem_FILES})
source_group("ECS\\Systems\\16 RenderCollect_RenderPrimitiveCollectSystem" FILES ${ECS_SYS_16_RenderCollect_RenderPrimitiveCollectSystem_FILES})
source_group("ECS\\Systems\\16B RenderCollect_AssetInstanceCollectSystem" FILES ${ECS_SYS_16B_RenderCollect_AssetInstanceCollectSystem_FILES})
source_group("ECS\\Systems\\17 RenderCollect_RenderPrimitiveCullSystem" FILES ${ECS_SYS_17_RenderCollect_RenderPrimitiveCullSystem_FILES})
source_group("ECS\\Systems\\18 RenderCollect_TextCollectSystem" FILES ${ECS_SYS_18_RenderCollect_TextCollectSystem_FILES})
source_group("ECS\\Systems\\19 RenderBatch_RenderPrimitiveSortSystem" FILES ${ECS_SYS_19_RenderBatch_RenderPrimitiveSortSystem_FILES})
source_group("ECS\\Systems\\20 RenderBatch_RenderPrimitiveBatchBuildSystem" FILES ${ECS_SYS_20_RenderBatch_RenderPrimitiveBatchBuildSystem_FILES})
source_group("ECS\\Systems\\21 RenderBatch_RenderPrimitiveBatchFinalizeSystem" FILES ${ECS_SYS_21_RenderBatch_RenderPrimitiveBatchFinalizeSystem_FILES})
source_group("ECS\\Systems\\22 RenderBatch_TextBuildSystem" FILES ${ECS_SYS_22_RenderBatch_TextBuildSystem_FILES})
source_group("ECS\\Systems\\23 RenderBatch_TextResourceSyncSystem" FILES ${ECS_SYS_23_RenderBatch_TextResourceSyncSystem_FILES})
source_group("ECS\\Systems\\24 RenderDrawSubmit_RenderPrimitiveSubmitSystem" FILES ${ECS_SYS_24_RenderDrawSubmit_RenderPrimitiveSubmitSystem_FILES})
source_group("ECS\\Systems\\25 RenderDrawSubmit_TextRenderSubmitSystem" FILES ${ECS_SYS_25_RenderDrawSubmit_TextRenderSubmitSystem_FILES})
source_group("ECS\\Systems\\26 RenderBatch_LineRenderSystem" FILES ${ECS_SYS_26_RenderBatch_LineRenderSystem_FILES})
source_group("ECS\\Systems\\26B RenderStat_LineStatsSystem" FILES ${ECS_SYS_26B_RenderStat_LineStatsSystem_FILES})
source_group("ECS\\Systems\\27 RenderSubmit_SwapchainSubmitSystem" FILES ${ECS_SYS_27_RenderSubmit_SwapchainSubmitSystem_FILES})
source_group("ECS\\Support" FILES ${ECS_SUPPORT_FILES})
source_group("ECS\\Support\\Primitive" FILES ${ECS_SUPPORT_PRIMITIVE_FILES})
source_group("ECS\\Support\\Text" FILES ${ECS_SUPPORT_TEXT_FILES})
source_group("ECS\\Support\\Line" FILES ${ECS_SUPPORT_LINE_FILES})
source_group("ECS\\Support\\Billboard" FILES ${ECS_SUPPORT_BILLBOARD_FILES})
source_group("ECS\\Support\\Terrain" FILES ${ECS_SUPPORT_TERRAIN_FILES})
source_group("ECS\\Serialization" FILES ${ECS_SERIALIZATION_FILES})

add_cm_library(ULRE.ECS "ULRE" ${ECS_INCLUDE_PATH}
${ECS_SOURCE})

if(ULRE_ECS_DEBUG_API)
    target_compile_definitions(ULRE.ECS PUBLIC ULRE_ECS_DEBUG_API=1)
else()
    target_compile_definitions(ULRE.ECS PUBLIC ULRE_ECS_DEBUG_API=0)
endif()
