#include "BillboardIconECSBase.h"
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/ecs/systems/render/TextureMaterialBindingSystem.h>
#include<hgl/mtl/MaterialLibrary.h>
#include <iostream>
#include <memory>

// ---------------------------------------------------------------------------

static Color4f white_color(1, 1, 1, 1);

// Unit quad: centered, Position2D + TexCoord
static const float kBaseQuadPosition[8] = {
    -0.5f, -0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,  0.5f
};
static const float kBaseQuadTexCoord[8] = {
    0.0f, 0.0f,  1.0f, 0.0f,
    1.0f, 1.0f,  0.0f, 1.0f
};
static const uint16_t kBaseQuadIndices[6] = {0, 1, 2, 0, 2, 3};

static const mtl::MaterialRecipe kBillboardBaseCfg {
    .id            = "billboard_icon_base",
    .preset        = mtl::MaterialPreset::UnlitTexture,
    .dim           = mtl::MaterialRecipe::Dim::D3,
    .prim          = PrimitiveType::Triangles,
    .vertex_input  = mtl::VertexInputProfile::PositionTexCoord2D,
    .vertex_policy = mtl::VertexTransformPolicy::BillboardAxisLocked,
    .pipeline      = GraphicsPipelinePreset::Alpha3D,
    .color_sources = {
        graph::ColorSource::MakeSampler2D(mtl::SamplerSlot::BaseColor),
    },
};

// ---------------------------------------------------------------------------
// BillboardIconECSBase implementation
// ---------------------------------------------------------------------------

BillboardIconECSBase::~BillboardIconECSBase()
{
}

void BillboardIconECSBase::ConfigureQuadPipelineMode()
{
}

#ifdef SHOW_PLANE_GRID
bool BillboardIconECSBase::CreatePlaneGrid()
{
    auto* render_context = GetRenderContext();
    if (!render_context) return false;

    auto* graphics_context = render_context->GetGraphicsContext();
    if (!graphics_context) return false;

    GraphicsGeometryFactory geometry_factory(graphics_context);

    using namespace inline_geometry;

    GeometryVertexFormat gvf;
    gvf.Set(VAN::Position,  VF_V2F);
    gvf.Set(VAN::Luminance, VF_V1UN8);

    auto pc = geometry_factory.CreateCreater(gvf);
    if (!pc) return false;

    PlaneGridCreateInfo pgci;
    pgci.grid_size.Set(500, 500);
    pgci.sub_count.Set(5, 5);
    pgci.lum    = 128;
    pgci.sub_lum = 192;

    geom_plane_grid = CreatePlaneGrid2D(pc.get(), &pgci);
    if (!geom_plane_grid) return false;

    if (!geometry_factory.RegisterGeometry(geom_plane_grid)) return false;

    return true;
}
#endif

bool BillboardIconECSBase::CreatePrimitives()
{
#ifdef SHOW_PLANE_GRID
    // Plane grid entity
    grid_entity = ecs_context->CreateEntity<Entity>("PlaneGrid");

    auto grid_transform = grid_entity->AddComponent<TransformComponent>(Mobility::Static);
    grid_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    grid_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    grid_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
    grid_transform->SetMovable(false);

    auto grid_primitive = grid_entity->AddComponent<PrimitiveComponent>();
    grid_primitive->SetUnresolvedGeometry(geom_plane_grid);
    grid_primitive->SetMaterialRecipe(RegisterMaterialRecipe(kPlaneGridCfg), &white_color, sizeof(white_color));
    grid_primitive->SetVisible(true);
#endif
    // 100 spiral billboard entities — cycle through icon textures
    {
        constexpr int   kBillboardCount = 100;
        constexpr float kAngleStep      = 0.45f;
        constexpr float kRadiusStart    = 2.0f;
        constexpr float kRadiusStep     = 0.6f;
        constexpr float kCenterY        = 5.0f;

        const char* prefix = GetEntityPrefix();

        // Create shared unit quad geometry
        Geometry* geom_unit_quad = CreateGeometry("BillboardBase_UnitQuad",
                                                   4, 6, IndexType::U16,
                                                   {{VAN::Position, VF_V2F, kBaseQuadPosition},
                                                    {VAN::TexCoord, VF_V2F, kBaseQuadTexCoord}},
                                                   kBaseQuadIndices);
        if (!geom_unit_quad) return false;

        auto mat_handle = RegisterMaterialRecipe(kBillboardBaseCfg);
        auto texture_binding_system = ecs_context->GetSystem<TextureMaterialBindingSystem>();

        for (int i = 0; i < kBillboardCount; ++i)
        {
            const float angle  = kAngleStep * static_cast<float>(i);
            const float radius = kRadiusStart + kRadiusStep * angle;

            const float x = std::cos(angle) * radius;
            const float y = kCenterY + std::sin(angle) * radius;
            const float z = 0.0f;

            const std::string name = std::string(prefix) + std::to_string(i);
            Entity* billboard_entity = ecs_context->CreateEntity<Entity>(name.c_str());
            if (!billboard_entity) return false;

            auto transform = billboard_entity->AddComponent<TransformComponent>(Mobility::Static);
            transform->SetLocalPosition(glm::vec3(x, y, z));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(8.0f, 8.0f, 1.0f));  // world size via scale
            transform->SetMovable(false);

            auto prim = billboard_entity->AddComponent<PrimitiveComponent>();
            prim->SetUnresolvedGeometry(geom_unit_quad);
            prim->SetMaterialRecipe(mat_handle);
            prim->SetVisible(true);

            if (texture_binding_system)
                texture_binding_system->SubmitTextureBindingRequest(
                    billboard_entity->GetID(),
                    GetIconTextures(i),
                    GetDomainTag());
        }
    }

    return true;
}

bool BillboardIconECSBase::EnsureRenderSystems()
{
    return ecs_context != nullptr;
}

bool BillboardIconECSBase::InitializeECS()
{
    ecs_context = GetECSContext();
    if (!ecs_context) return false;

    ConfigureQuadPipelineMode();

    if (!EnsureRenderSystems()) return false;

#ifdef SHOW_PLANE_GRID
    if (!CreatePlaneGrid())    return false;
#endif
    if (!CreatePrimitives())   return false;

    return true;
}

bool BillboardIconECSBase::InitializeCamera()
{
    if (!ecs_context || !ecs_context->EnsureCameraSystem())
        return false;

    camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
    auto camera   = camera_entity->AddComponent<CameraComponent>();

    camera->control_mode    = CameraComponent::ControlMode::ViewModel;
    camera->target          = math::Vector3f(0.0f, 5.0f, 0.0f);
    camera->distance        = 50.0f;
    camera->yaw             = 45.0f;
    camera->pitch           = -15.0f;
    camera->is_main_camera  = true;
    camera->matrix_dirty    = true;

    camera->camera_data    = GetCamera();
    camera->camera_info    = const_cast<graph::CameraInfo*>(GetCameraInfo());
    camera->viewport_info  = GetViewportInfo();

    return true;
}

bool BillboardIconECSBase::Init()
{
    SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

    if (!InitializeECS())      return false;
    if (!InitializeCamera())   return false;

    return true;
}

void BillboardIconECSBase::Tick(double delta_time)
{
    WorkObject::Tick(delta_time);
}

