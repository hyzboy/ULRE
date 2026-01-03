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

add_cm_library(ULRE.ECS "ULRE" ${ECS_SOURCE})
