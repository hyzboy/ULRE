#pragma once

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/color/Color.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/BillboardComponent.h>
#include<hgl/ecs/components/QuadComponent.h>
#include<hgl/ecs/components/FacingTransformComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/QuadMaterialBindingSystem.h>
#include<hgl/ecs/systems/transform/FacingTransformSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<cmath>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

/**
 * Shared base for all billboard-icon ECS examples.
 *
 * Every example shares: a plane-grid entity, 100 spiral billboard entities using
 * the freepik icon set, a standard camera, and the three ECS render systems
 * (QuadResourcePrepareSystem, QuadMaterialBindingSystem, FacingTransformSystem).
 *
 * Derived classes only need to implement GetEntityPrefix() to tag their billboard
 * entity names, and optionally override Init()/Tick() for example-specific setup
 * or pipeline customisation.
 */
class BillboardIconECSBase : public WorkObject
{
protected:

    ECSContext* ecs_context = nullptr;

    Entity* grid_entity   = nullptr;
    Entity* camera_entity = nullptr;

    // PlaneGrid resources
    MaterialTemplate*         mtl_plane_grid      = nullptr;
    MaterialInstance* mi_plane_grid       = nullptr;
    Geometry*         geom_plane_grid     = nullptr;
    Primitive*        prim_plane_grid     = nullptr;

    // Derived class supplies the prefix used in billboard entity names,
    // e.g. "MaskedBillboard_", "DitherBillboard_".
    virtual const char* GetEntityPrefix() const = 0;
    virtual void ConfigureQuadPipelineMode();

    virtual const os_char* GetIconTextures(int) const = 0;

    // Domain tag for texture-array batching.  All billboards sharing the
    // same tag are packed into one Texture2DArray → one draw call.
    // Default returns GetEntityPrefix(); override for a custom tag.
    virtual const char* GetDomainTag() const { return GetEntityPrefix(); }

    bool InitPlaneGridResources();
    bool CreateGeometryAndPrimitives();
    bool EnsureRenderSystems();
    bool InitializeECS();
    bool InitializeCamera();

public:

    ~BillboardIconECSBase();

    bool Init() override;
    void Tick(double delta_time) override;
};
