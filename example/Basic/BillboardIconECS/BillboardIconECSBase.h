#pragma once

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/graph/module/GeometryManager.h>
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

//#define SHOW_PLANE_GRID

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

    Entity* camera_entity = nullptr;

#ifdef SHOW_PLANE_GRID
    Entity* grid_entity   = nullptr;
    // PlaneGrid resources
    Geometry*         geom_plane_grid     = nullptr;

    inline static const mtl::MaterialRecipe kPlaneGridCfg {
        .id            = "billboard_icon_plane_grid",
        .preset        = mtl::MaterialPreset::VertexLuminance2D,
        .prim          = PrimitiveType::Lines,
        .vertex_input  = mtl::VertexInputProfile::PositionLuminance2D,
        .vertex_policy = mtl::VertexTransformPolicy::Quad2D,
        .shading_model = mtl::SurfaceShadingModel::VertexLuminance,
        .schema        = mtl::ShaderDataSchema::Color4f,
        .has_explicit_schema = true,
        .pipeline      = GraphicsPipelinePreset::Solid3D,
    };

    bool CreatePlaneGrid();
#endif // SHOW_PLANE_GRID

    // Derived class supplies the prefix used in billboard entity names,
    // e.g. "MaskedBillboard_", "DitherBillboard_".
    virtual const char* GetEntityPrefix() const = 0;
    virtual void ConfigureQuadPipelineMode();

    virtual const os_char* GetIconTextures(int) const = 0;

    // Domain tag for texture-array batching.  All billboards sharing the
    // same tag are packed into one Texture2DArray → one draw call.
    // Default returns GetEntityPrefix(); override for a custom tag.
    virtual const char* GetDomainTag() const { return GetEntityPrefix(); }

    bool CreatePrimitives();
    bool EnsureRenderSystems();
    bool InitializeECS();
    bool InitializeCamera();

public:

    ~BillboardIconECSBase();

    bool Init() override;
    void Tick(double delta_time) override;
};
