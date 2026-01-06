SET(ECS_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/ecs)

# ============================================================================
# Core ECS Framework
# ============================================================================
SET(ECS_CORE_HEADERS
    ${ECS_INCLUDE_PATH}/Object.h
    ${ECS_INCLUDE_PATH}/Component.h
    ${ECS_INCLUDE_PATH}/Entity.h
    ${ECS_INCLUDE_PATH}/System.h
    ${ECS_INCLUDE_PATH}/Context.h
)

SET(ECS_CORE_SOURCE
    Object.cpp
    Component.cpp
    Entity.cpp
    System.cpp
    Context.cpp
)

# ============================================================================
# Transform Component System
# ============================================================================
SET(ECS_TRANSFORM_HEADERS
    ${ECS_INCLUDE_PATH}/TransformComponent.h
    ${ECS_INCLUDE_PATH}/TransformDataStorage.h
)

SET(ECS_TRANSFORM_SOURCE
    TransformComponent.cpp
)

# ============================================================================
# Bounding Box Component System
# ============================================================================
SET(ECS_BOUNDINGBOX_HEADERS
    ${ECS_INCLUDE_PATH}/BoundingBoxComponent.h
    ${ECS_INCLUDE_PATH}/BoundingBoxDataStorage.h
)

SET(ECS_BOUNDINGBOX_SOURCE
    BoundingBoxComponent.cpp
)

# ============================================================================
# Renderable Component System
# ============================================================================
SET(ECS_RENDERABLE_HEADERS
    ${ECS_INCLUDE_PATH}/RenderableComponent.h
    ${ECS_INCLUDE_PATH}/PrimitiveComponent.h
)

SET(ECS_RENDERABLE_SOURCE
    PrimitiveComponent.cpp
)

# ============================================================================
# Render Items & Batching
# ============================================================================
SET(ECS_RENDERITEM_HEADERS
    ${ECS_INCLUDE_PATH}/RenderItem.h
    ${ECS_INCLUDE_PATH}/PrimitiveRenderItem.h
    ${ECS_INCLUDE_PATH}/MaterialPipelineKey.h
)

SET(ECS_RENDERITEM_SOURCE
    RenderItem.cpp
    PrimitiveRenderItem.cpp
)

# ============================================================================
# Material Batching & Rendering
# ============================================================================
SET(ECS_MATERIALBATCH_HEADERS
    ${ECS_INCLUDE_PATH}/MaterialBatch.h
)

SET(ECS_MATERIALBATCH_SOURCE
    MaterialBatch.cpp
    ECSTransformAssignmentBuffer.h
    ECSTransformAssignmentBuffer.cpp
    ECSMaterialInstanceAssignmentBuffer.h
    ECSMaterialInstanceAssignmentBuffer.cpp
    ECSPipelineMaterialRenderer.h
    ECSPipelineMaterialRenderer.cpp
)

# ============================================================================
# Render Systems
# ============================================================================
SET(ECS_RENDERSYSTEM_HEADERS
    ${ECS_INCLUDE_PATH}/RenderPrimitiveSystem.h
)

SET(ECS_RENDERSYSTEM_SOURCE
    RenderPrimitiveSystem.cpp
)

# ============================================================================
# Combine All Sources
# ============================================================================
SET(ECS_SOURCE
    ${ECS_CORE_HEADERS}
    ${ECS_CORE_SOURCE}
    ${ECS_TRANSFORM_HEADERS}
    ${ECS_TRANSFORM_SOURCE}
    ${ECS_BOUNDINGBOX_HEADERS}
    ${ECS_BOUNDINGBOX_SOURCE}
    ${ECS_RENDERABLE_HEADERS}
    ${ECS_RENDERABLE_SOURCE}
    ${ECS_RENDERITEM_HEADERS}
    ${ECS_RENDERITEM_SOURCE}
    ${ECS_MATERIALBATCH_HEADERS}
    ${ECS_MATERIALBATCH_SOURCE}
    ${ECS_RENDERSYSTEM_HEADERS}
    ${ECS_RENDERSYSTEM_SOURCE}
)

# ============================================================================
# Create Library & Setup Source Groups
# ============================================================================
add_cm_library(ULRE.ECS "ULRE" ${ECS_SOURCE})

# Source group organization for Visual Studio
source_group("Core\\Public" FILES ${ECS_CORE_HEADERS})
source_group("Core\\Private" FILES ${ECS_CORE_SOURCE})

source_group("Transform\\Public" FILES ${ECS_TRANSFORM_HEADERS})
source_group("Transform\\Private" FILES ${ECS_TRANSFORM_SOURCE})

source_group("BoundingBox\\Public" FILES ${ECS_BOUNDINGBOX_HEADERS})
source_group("BoundingBox\\Private" FILES ${ECS_BOUNDINGBOX_SOURCE})

source_group("Renderable\\Public" FILES ${ECS_RENDERABLE_HEADERS})
source_group("Renderable\\Private" FILES ${ECS_RENDERABLE_SOURCE})

source_group("RenderItem\\Public" FILES ${ECS_RENDERITEM_HEADERS})
source_group("RenderItem\\Private" FILES ${ECS_RENDERITEM_SOURCE})

source_group("MaterialBatch\\Public" FILES ${ECS_MATERIALBATCH_HEADERS})
source_group("MaterialBatch\\Private" FILES ${ECS_MATERIALBATCH_SOURCE})

source_group("RenderSystem\\Public" FILES ${ECS_RENDERSYSTEM_HEADERS})
source_group("RenderSystem\\Private" FILES ${ECS_RENDERSYSTEM_SOURCE})
