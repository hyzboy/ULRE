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
    ${ECS_INCLUDE_PATH}/core/World.h
    ${ECS_INCLUDE_PATH}/core/DefaultSystems.h
    ${ECS_INCLUDE_PATH}/core/ComponentRecords.h
    ${ECS_INCLUDE_PATH}/core/MaterialBatch.h
    ${ECS_INCLUDE_PATH}/core/MaterialPipelineKey.h
    ${ECS_INCLUDE_PATH}/core/RenderItem.h
    ${ECS_INCLUDE_PATH}/core/PrimitiveRenderItem.h
    core/Object.cpp
    core/Component.cpp
    core/Entity.cpp
    core/EntityManager.cpp
    core/EntityQuery.cpp
    core/System.cpp
    core/SystemProfiler.cpp
    core/Context.cpp
    core/World.cpp
    core/DefaultSystems.cpp
    core/MaterialBatch.cpp
    core/RenderItem.cpp
    core/PrimitiveRenderItem.cpp
)

set(ECS_COMPONENT_FILES
    ${ECS_INCLUDE_PATH}/components/TransformComponent.h
    ${ECS_INCLUDE_PATH}/components/BoundingBoxComponent.h
    ${ECS_INCLUDE_PATH}/components/VisibilityComponent.h
    ${ECS_INCLUDE_PATH}/components/PrimitiveComponent.h
    ${ECS_INCLUDE_PATH}/components/BillboardComponent.h
    ${ECS_INCLUDE_PATH}/components/StaticMeshComponent.h
    ${ECS_INCLUDE_PATH}/components/CameraComponent.h
    ${ECS_INCLUDE_PATH}/components/LinesComponent.h
    ${ECS_INCLUDE_PATH}/components/QuadComponent.h
    ${ECS_INCLUDE_PATH}/components/FacingTransformComponent.h
    ${ECS_INCLUDE_PATH}/components/TextComponent.h
    ${ECS_INCLUDE_PATH}/components/RenderableComponent.h
    ${ECS_INCLUDE_PATH}/components/SubWorldComponent.h
    ${ECS_INCLUDE_PATH}/components/StaticMesh.h
    components/TransformComponent.cpp
    components/BoundingBoxComponent.cpp
    components/VisibilityComponent.cpp
    components/PrimitiveComponent.cpp
    components/BillboardComponent.cpp
    components/StaticMeshComponent.cpp
    components/CameraComponent.cpp
    components/QuadComponent.cpp
    components/FacingTransformComponent.cpp
    components/StaticMesh.cpp
    components/SubWorldComponent.cpp
)

set(ECS_SYSTEM_TICK_FILES
    ${ECS_INCLUDE_PATH}/systems/tick/TransformSystem.h
    ${ECS_INCLUDE_PATH}/systems/tick/BoundingBoxUpdateSystem.h
    ${ECS_INCLUDE_PATH}/systems/tick/VisibilitySystem.h
    ${ECS_INCLUDE_PATH}/systems/tick/InputSystem.h
    ${ECS_INCLUDE_PATH}/systems/tick/CameraSystem.h
    ${ECS_INCLUDE_PATH}/systems/tick/CameraInputMapping.h
    systems/tick/TransformSystem.cpp
    systems/tick/BoundingBoxUpdateSystem.cpp
    systems/tick/VisibilitySystem.cpp
    systems/tick/InputSystem.cpp
    systems/tick/CameraSystem.cpp
    systems/tick/CameraInputMapping.cpp
)

set(ECS_SYSTEM_RENDER_FILES
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveCollectSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveCullSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveSortSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveBatchBuildSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveBatchFinalizeSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/RenderPrimitiveSubmitSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/RenderBufferCommitSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/RenderBufferUploadSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/RenderSystemCore.h
    ${ECS_INCLUDE_PATH}/systems/render/RenderTargetSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/EnvironmentSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/LineRenderSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/TextCollectSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/TextBuildSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/TextResourceSyncSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/TextRenderSubmitSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/QuadResourcePrepareSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/QuadMaterialBindingSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/RenderFrameBusinessSyncSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/SwapchainNextImageSystem.h
    ${ECS_INCLUDE_PATH}/systems/render/SwapchainSubmitSystem.h
    systems/render/RenderPrimitiveCollectSystem.cpp
    systems/render/RenderPrimitiveCullSystem.cpp
    systems/render/RenderPrimitiveSortSystem.cpp
    systems/render/RenderPrimitiveBatchBuildSystem.cpp
    systems/render/RenderPrimitiveBatchFinalizeSystem.cpp
    systems/render/RenderPrimitiveSubmitSystem.cpp
    systems/render/RenderBufferCommitSystem.cpp
    systems/render/RenderBufferUploadSystem.cpp
    systems/render/RenderSystemCore.cpp
    systems/render/RenderTargetSystem.cpp
    systems/render/EnvironmentSystem.cpp
    systems/render/LineRenderSystem.cpp
    systems/render/TextCollectSystem.cpp
    systems/render/TextBuildSystem.cpp
    systems/render/TextResourceSyncSystem.cpp
    systems/render/TextRenderSubmitSystem.cpp
    systems/render/QuadResourcePrepareSystem.cpp
    systems/render/QuadMaterialBindingSystem.cpp
    systems/render/RenderFrameBusinessSyncSystem.cpp
    systems/render/SwapchainNextImageSystem.cpp
    systems/render/SwapchainSubmitSystem.cpp
)

set(ECS_SYSTEM_TRANSFORM_FILES
    ${ECS_INCLUDE_PATH}/systems/transform/FacingTransformSystem.h
    systems/transform/FacingTransformSystem.cpp
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
    ${ECS_SYSTEM_TICK_FILES}
    ${ECS_SYSTEM_RENDER_FILES}
    ${ECS_SYSTEM_TRANSFORM_FILES}
    ${ECS_SUPPORT_FILES}
    ${ECS_SERIALIZATION_FILES}
)

source_group("ECS\\Core" FILES ${ECS_CORE_FILES})
source_group("ECS\\Components" FILES ${ECS_COMPONENT_FILES})
source_group("ECS\\Systems\\Tick" FILES ${ECS_SYSTEM_TICK_FILES})
source_group("ECS\\Systems\\Render" FILES ${ECS_SYSTEM_RENDER_FILES})
source_group("ECS\\Systems\\Transform" FILES ${ECS_SYSTEM_TRANSFORM_FILES})
source_group("ECS\\Support" FILES ${ECS_SUPPORT_FILES})
source_group("ECS\\Serialization" FILES ${ECS_SERIALIZATION_FILES})

add_cm_library(ULRE.ECS "ULRE" ${ECS_INCLUDE_PATH}
${ECS_SOURCE})
