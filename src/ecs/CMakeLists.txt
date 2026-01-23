SET(ECS_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/ecs)

SET(ECS_SOURCE  ${ECS_INCLUDE_PATH}/Object.h
                ${ECS_INCLUDE_PATH}/Component.h
                ${ECS_INCLUDE_PATH}/Entity.h
                ${ECS_INCLUDE_PATH}/System.h
                ${ECS_INCLUDE_PATH}/Context.h
                ${ECS_INCLUDE_PATH}/TransformComponent.h
                ${ECS_INCLUDE_PATH}/TransformDataStorage.h
                ${ECS_INCLUDE_PATH}/BoundingBoxComponent.h
                ${ECS_INCLUDE_PATH}/BoundingBoxDataStorage.h
                ${ECS_INCLUDE_PATH}/RenderPrimitiveSystem.h
                ${ECS_INCLUDE_PATH}/RenderItem.h
                ${ECS_INCLUDE_PATH}/PrimitiveRenderItem.h
                ${ECS_INCLUDE_PATH}/MaterialPipelineKey.h
                ${ECS_INCLUDE_PATH}/MaterialBatch.h
                ${ECS_INCLUDE_PATH}/RenderableComponent.h
                ${ECS_INCLUDE_PATH}/PrimitiveComponent.h
                ${ECS_INCLUDE_PATH}/CameraComponent.h

                ${ECS_INCLUDE_PATH}/InputSystem.h
                InputSystem.cpp

                ECSTransformAssignmentBuffer.h
                ECSMaterialInstanceAssignmentBuffer.h

                ECSTransformAssignmentBuffer.cpp
                ECSMaterialInstanceAssignmentBuffer.cpp

                ECSPipelineMaterialRenderer.cpp
                ECSPipelineMaterialRenderer.h

                Object.cpp
                Component.cpp
                Entity.cpp
                System.cpp
                Context.cpp
                TransformComponent.cpp
                BoundingBoxComponent.cpp
                PrimitiveComponent.cpp
                RenderItem.cpp
                PrimitiveRenderItem.cpp
                MaterialBatch.cpp
                RenderPrimitiveSystem.cpp)

# --- Source groups (按功能分类，将 .h/.cpp 配对放在同一组) ----------------------
source_group("ECS\\Core" FILES
    ${ECS_INCLUDE_PATH}/Object.h
    Object.cpp
    ${ECS_INCLUDE_PATH}/Component.h
    Component.cpp
    ${ECS_INCLUDE_PATH}/Entity.h
    Entity.cpp
    ${ECS_INCLUDE_PATH}/System.h
    System.cpp
    ${ECS_INCLUDE_PATH}/Context.h
    Context.cpp
)

source_group("ECS\\Transform" FILES
    ${ECS_INCLUDE_PATH}/TransformComponent.h
    TransformComponent.cpp
    ${ECS_INCLUDE_PATH}/TransformDataStorage.h
    ECSTransformAssignmentBuffer.h
    ECSTransformAssignmentBuffer.cpp
)

source_group("ECS\\BoundingBox" FILES
    ${ECS_INCLUDE_PATH}/BoundingBoxComponent.h
    BoundingBoxComponent.cpp
    ${ECS_INCLUDE_PATH}/BoundingBoxDataStorage.h
)

source_group("ECS\\Rendering" FILES
    ${ECS_INCLUDE_PATH}/RenderItem.h
    RenderItem.cpp
    ${ECS_INCLUDE_PATH}/PrimitiveRenderItem.h
    PrimitiveRenderItem.cpp
    ${ECS_INCLUDE_PATH}/PrimitiveComponent.h
    PrimitiveComponent.cpp
    ${ECS_INCLUDE_PATH}/RenderPrimitiveSystem.h
    RenderPrimitiveSystem.cpp
    ${ECS_INCLUDE_PATH}/RenderableComponent.h
)

source_group("ECS\\Material" FILES
    ${ECS_INCLUDE_PATH}/MaterialPipelineKey.h
    ${ECS_INCLUDE_PATH}/MaterialBatch.h
    MaterialBatch.cpp
    ECSPipelineMaterialRenderer.h
    ECSPipelineMaterialRenderer.cpp
    ECSMaterialInstanceAssignmentBuffer.h
    ECSMaterialInstanceAssignmentBuffer.cpp
)

source_group("ECS\\Camera" FILES
    ${ECS_INCLUDE_PATH}/CameraComponent.h
)

source_group("ECS\\Input" FILES
    ${ECS_INCLUDE_PATH}/InputSystem.h
    InputSystem.cpp
)

# ---------------------------------------------------------------------------

add_cm_library(ULRE.ECS "ULRE" ${ECS_SOURCE})
