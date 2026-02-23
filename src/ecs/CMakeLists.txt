SET(ECS_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/ecs)

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
    systems/render/RenderSystemCore.cpp
)

set(ECS_COMPONENT_FILES
    ${ECS_INCLUDE_PATH}/components/SubWorldComponent.h
    components/SubWorldComponent.cpp
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

set(ECS_SYS_10_RenderPreBeginFrame_QuadResourcePrepareSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/QuadResourcePrepareSystem.h
    systems/render/QuadResourcePrepareSystem.cpp
)

set(ECS_SYS_11_RenderPreBeginFrame_QuadMaterialBindingSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/QuadMaterialBindingSystem.h
    systems/render/QuadMaterialBindingSystem.cpp
    ${ECS_INCLUDE_PATH}/components/QuadComponent.h
    components/QuadComponent.cpp
)

set(ECS_SYS_14_RenderBufferUpload_RenderBufferUploadSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderBufferUploadSystem.h
    systems/render/RenderBufferUploadSystem.cpp
)

set(ECS_SYS_15_RenderPostBeginFrame_RenderFrameBusinessSyncSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderFrameBusinessSyncSystem.h
    systems/render/RenderFrameBusinessSyncSystem.cpp
)

set(ECS_SYS_16_RenderCollect_RenderPrimitiveCollectSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveCollectSystem.h
    systems/render/RenderPrimitiveCollectSystem.cpp
    ${ECS_INCLUDE_PATH}/components/PrimitiveComponent.h
    components/PrimitiveComponent.cpp
    ${ECS_INCLUDE_PATH}/components/RenderableComponent.h
    ${ECS_INCLUDE_PATH}/components/StaticMeshComponent.h
    components/StaticMeshComponent.cpp
    ${ECS_INCLUDE_PATH}/components/BillboardComponent.h
    components/BillboardComponent.cpp
    ${ECS_INCLUDE_PATH}/components/StaticMesh.h
    components/StaticMesh.cpp
)

set(ECS_SYS_17_RenderCollect_RenderPrimitiveCullSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveCullSystem.h
    systems/render/RenderPrimitiveCullSystem.cpp
)

set(ECS_SYS_18_RenderCollect_TextCollectSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/TextCollectSystem.h
    systems/render/TextCollectSystem.cpp
    ${ECS_INCLUDE_PATH}/components/TextComponent.h
)

set(ECS_SYS_18B_RenderCollect_LineCollectSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/LineCollectSystem.h
    systems/render/LineCollectSystem.cpp
)

set(ECS_SYS_19_RenderBatch_RenderPrimitiveSortSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveSortSystem.h
    systems/render/RenderPrimitiveSortSystem.cpp
)

set(ECS_SYS_20_RenderBatch_RenderPrimitiveBatchBuildSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveBatchBuildSystem.h
    systems/render/RenderPrimitiveBatchBuildSystem.cpp
)

set(ECS_SYS_21_RenderBatch_RenderPrimitiveBatchFinalizeSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveBatchFinalizeSystem.h
    systems/render/RenderPrimitiveBatchFinalizeSystem.cpp
)

set(ECS_SYS_22_RenderBatch_TextBuildSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/TextBuildSystem.h
    systems/render/TextBuildSystem.cpp
)

set(ECS_SYS_23_RenderBatch_TextResourceSyncSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/TextResourceSyncSystem.h
    systems/render/TextResourceSyncSystem.cpp
)

set(ECS_SYS_24_RenderDrawSubmit_RenderPrimitiveSubmitSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveSubmitSystem.h
    systems/render/RenderPrimitiveSubmitSystem.cpp
)

set(ECS_SYS_25_RenderDrawSubmit_TextRenderSubmitSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/TextRenderSubmitSystem.h
    systems/render/TextRenderSubmitSystem.cpp
)

set(ECS_SYS_26_RenderPostProcess_LineRenderSystem_FILES
    ${ECS_INCLUDE_PATH}/systems/render/LineRenderSystem.h
    systems/render/LineRenderSystem.cpp
    ${ECS_INCLUDE_PATH}/components/LinesComponent.h
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
    ${ECS_INCLUDE_PATH}/support/PrimitiveBatchPipeline.h
    ${ECS_INCLUDE_PATH}/support/TextRenderPipeline.h
    support/VisibilityDataStorage.cpp
    support/TransformAssignmentBuffer.cpp
    support/MaterialInstanceAssignmentBuffer.cpp
    support/PipelineMaterialRenderer.cpp
    support/PrimitiveBatchPipeline.cpp
    support/TextRenderPipeline.cpp
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
    ${ECS_SYS_06B_TickTransform_LineBoundsUpdateSystem_FILES}
    ${ECS_SYS_07_RenderSwapchainNextImage_SwapchainAcquireSystem_FILES}
    ${ECS_SYS_08_RenderPreBeginFrame_RenderTargetSystem_FILES}
    ${ECS_SYS_09_RenderPreBeginFrame_EnvironmentSystem_FILES}
    ${ECS_SYS_10_RenderPreBeginFrame_QuadResourcePrepareSystem_FILES}
    ${ECS_SYS_11_RenderPreBeginFrame_QuadMaterialBindingSystem_FILES}
    ${ECS_SYS_14_RenderBufferUpload_RenderBufferUploadSystem_FILES}
    ${ECS_SYS_15_RenderPostBeginFrame_RenderFrameBusinessSyncSystem_FILES}
    ${ECS_SYS_16_RenderCollect_RenderPrimitiveCollectSystem_FILES}
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
    ${ECS_SYS_26_RenderPostProcess_LineRenderSystem_FILES}
    ${ECS_SYS_26B_RenderStat_LineStatsSystem_FILES}
    ${ECS_SYS_27_RenderSubmit_SwapchainSubmitSystem_FILES}
    ${ECS_SUPPORT_FILES}
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
source_group("ECS\\Systems\\07 RenderSwapchainNextImage_SwapchainAcquireSystem" FILES ${ECS_SYS_07_RenderSwapchainNextImage_SwapchainAcquireSystem_FILES})
source_group("ECS\\Systems\\08 RenderPreBeginFrame_RenderTargetSystem" FILES ${ECS_SYS_08_RenderPreBeginFrame_RenderTargetSystem_FILES})
source_group("ECS\\Systems\\09 RenderPreBeginFrame_EnvironmentSystem" FILES ${ECS_SYS_09_RenderPreBeginFrame_EnvironmentSystem_FILES})
source_group("ECS\\Systems\\10 RenderPreBeginFrame_QuadResourcePrepareSystem" FILES ${ECS_SYS_10_RenderPreBeginFrame_QuadResourcePrepareSystem_FILES})
source_group("ECS\\Systems\\11 RenderPreBeginFrame_QuadMaterialBindingSystem" FILES ${ECS_SYS_11_RenderPreBeginFrame_QuadMaterialBindingSystem_FILES})
source_group("ECS\\Systems\\14 RenderBufferUpload_RenderBufferUploadSystem" FILES ${ECS_SYS_14_RenderBufferUpload_RenderBufferUploadSystem_FILES})
source_group("ECS\\Systems\\15 RenderPostBeginFrame_RenderFrameBusinessSyncSystem" FILES ${ECS_SYS_15_RenderPostBeginFrame_RenderFrameBusinessSyncSystem_FILES})
source_group("ECS\\Systems\\16 RenderCollect_RenderPrimitiveCollectSystem" FILES ${ECS_SYS_16_RenderCollect_RenderPrimitiveCollectSystem_FILES})
source_group("ECS\\Systems\\17 RenderCollect_RenderPrimitiveCullSystem" FILES ${ECS_SYS_17_RenderCollect_RenderPrimitiveCullSystem_FILES})
source_group("ECS\\Systems\\18 RenderCollect_TextCollectSystem" FILES ${ECS_SYS_18_RenderCollect_TextCollectSystem_FILES})
source_group("ECS\\Systems\\19 RenderBatch_RenderPrimitiveSortSystem" FILES ${ECS_SYS_19_RenderBatch_RenderPrimitiveSortSystem_FILES})
source_group("ECS\\Systems\\20 RenderBatch_RenderPrimitiveBatchBuildSystem" FILES ${ECS_SYS_20_RenderBatch_RenderPrimitiveBatchBuildSystem_FILES})
source_group("ECS\\Systems\\21 RenderBatch_RenderPrimitiveBatchFinalizeSystem" FILES ${ECS_SYS_21_RenderBatch_RenderPrimitiveBatchFinalizeSystem_FILES})
source_group("ECS\\Systems\\22 RenderBatch_TextBuildSystem" FILES ${ECS_SYS_22_RenderBatch_TextBuildSystem_FILES})
source_group("ECS\\Systems\\23 RenderBatch_TextResourceSyncSystem" FILES ${ECS_SYS_23_RenderBatch_TextResourceSyncSystem_FILES})
source_group("ECS\\Systems\\24 RenderDrawSubmit_RenderPrimitiveSubmitSystem" FILES ${ECS_SYS_24_RenderDrawSubmit_RenderPrimitiveSubmitSystem_FILES})
source_group("ECS\\Systems\\25 RenderDrawSubmit_TextRenderSubmitSystem" FILES ${ECS_SYS_25_RenderDrawSubmit_TextRenderSubmitSystem_FILES})
source_group("ECS\\Systems\\26 RenderPostProcess_LineRenderSystem" FILES ${ECS_SYS_26_RenderPostProcess_LineRenderSystem_FILES})
source_group("ECS\\Systems\\26B RenderStat_LineStatsSystem" FILES ${ECS_SYS_26B_RenderStat_LineStatsSystem_FILES})
source_group("ECS\\Systems\\27 RenderSubmit_SwapchainSubmitSystem" FILES ${ECS_SYS_27_RenderSubmit_SwapchainSubmitSystem_FILES})
source_group("ECS\\Support" FILES ${ECS_SUPPORT_FILES})
source_group("ECS\\Serialization" FILES ${ECS_SERIALIZATION_FILES})

add_cm_library(ULRE.ECS "ULRE" ${ECS_INCLUDE_PATH}
${ECS_SOURCE})
