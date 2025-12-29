SET(ECS_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/ecs)

SET(ECS_SOURCE  ${ECS_INCLUDE_PATH}/Object.h
                ${ECS_INCLUDE_PATH}/Component.h
                ${ECS_INCLUDE_PATH}/Entity.h
                ${ECS_INCLUDE_PATH}/System.h
                ${ECS_INCLUDE_PATH}/World.h
                ${ECS_INCLUDE_PATH}/TransformComponent.h
                Object.cpp
                Component.cpp
                Entity.cpp
                System.cpp
                World.cpp
                TransformComponent.cpp)

add_cm_library(ULRE.ECS "ULRE" ${ECS_SOURCE})
