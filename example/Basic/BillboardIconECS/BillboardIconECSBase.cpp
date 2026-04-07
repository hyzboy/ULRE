#include "BillboardIconECSBase.h"
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include <iostream>
#include <memory>

// ---------------------------------------------------------------------------

static Color4f white_color(1, 1, 1, 1);

// ---------------------------------------------------------------------------
// BillboardIconECSBase implementation
// ---------------------------------------------------------------------------

BillboardIconECSBase::~BillboardIconECSBase()
{
}

void BillboardIconECSBase::ConfigureQuadPipelineMode()
{
    QuadResourcePrepareSystem::SetPresetForWorld(ecs_context, GraphicsPipelinePreset::Solid3D);
    QuadResourcePrepareSystem::SetFixedSizeForWorld(ecs_context, false);    // dynamic world-space billboards
}

bool BillboardIconECSBase::InitPlaneGridResources()
{
    if (mi_plane_grid) return true;

    static const mtl::MaterialAssetRecord kPlaneGridCfg {
        .id       = "billboard_icon_plane_grid",
        .preset   = mtl::MaterialPreset::VertexLuminance2D,
        .prim     = PrimitiveType::Lines,
        .pipeline = GraphicsPipelinePreset::Solid3D,
        .mi_vil_overrides = {
            { VAN::Luminance, VF_V1UN8 },
        },
    };
    mi_plane_grid = AcquireMI(kPlaneGridCfg,&white_color, sizeof(white_color));
    if (!mi_plane_grid) return false;

    mtl_plane_grid = mi_plane_grid->GetMaterial();

    return true;
}

bool BillboardIconECSBase::CreateGeometryAndPrimitives()
{
    auto* render_context = GetRenderContext();
    if (!render_context) return false;

    auto* graphics_context = render_context->GetGraphicsContext();
    if (!graphics_context) return false;

    GraphicsGeometryFactory geometry_factory(graphics_context);

    using namespace inline_geometry;

    auto pc = geometry_factory.CreateCreater(mi_plane_grid);
    if (!pc) return false;

    PlaneGridCreateInfo pgci;
    pgci.grid_size.Set(500, 500);
    pgci.sub_count.Set(5, 5);
    pgci.lum    = 128;
    pgci.sub_lum = 192;

    geom_plane_grid = CreatePlaneGrid2D(pc.get(), &pgci);
    if (!geom_plane_grid) return false;

    if (!geometry_factory.RegisterGeometry(geom_plane_grid)) return false;

    prim_plane_grid = geometry_factory.CreatePrimitive(geom_plane_grid, mi_plane_grid->ToSlot());
    if (!prim_plane_grid) return false;

    return true;
}

bool BillboardIconECSBase::EnsureRenderSystems()
{
    if (!ecs_context) return false;

    auto quad_prepare_system = ecs_context->GetSystem<QuadResourcePrepareSystem>();
    if (!quad_prepare_system)
    {
        quad_prepare_system = ecs_context->RegisterRenderSystem<QuadResourcePrepareSystem>();
        quad_prepare_system->SetWorld(ecs_context);
        if (ecs_context->IsActive())
        {
            quad_prepare_system->OnDependenciesReady();
            quad_prepare_system->Initialize();
        }
    }

    auto quad_binding_system = ecs_context->GetSystem<QuadMaterialBindingSystem>();
    if (!quad_binding_system)
    {
        quad_binding_system = ecs_context->RegisterRenderSystem<QuadMaterialBindingSystem>();
        quad_binding_system->SetWorld(ecs_context);
        if (ecs_context->IsActive())
        {
            quad_binding_system->OnDependenciesReady();
            quad_binding_system->Initialize();
        }
    }

    auto facing_system = ecs_context->GetSystem<FacingTransformSystem>();
    if (!facing_system)
    {
        facing_system = ecs_context->RegisterTickSystem<FacingTransformSystem>();
        facing_system->SetWorld(ecs_context);
        facing_system->SetCameraInfo(GetCameraInfo());
        if (ecs_context->IsActive())
        {
            facing_system->OnDependenciesReady();
            facing_system->Initialize();
        }
    }

    return quad_prepare_system && quad_binding_system && facing_system;
}

bool BillboardIconECSBase::InitializeECS()
{
    ecs_context = GetECSContext();
    if (!ecs_context) return false;

    ConfigureQuadPipelineMode();

    if (!EnsureRenderSystems()) return false;

    // Plane grid entity
    {
        grid_entity = ecs_context->CreateEntity<Entity>("PlaneGrid");

        auto grid_transform = grid_entity->AddComponent<TransformComponent>(Mobility::Static);
        grid_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        grid_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        grid_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        grid_transform->SetMovable(false);

        auto grid_primitive = grid_entity->AddComponent<PrimitiveComponent>();
        grid_primitive->SetPrimitive(prim_plane_grid);
        grid_primitive->SetVisible(true);
    }

    // 100 spiral billboard entities — cycle through icon textures
    {
        constexpr int   kBillboardCount = 100;
        constexpr float kAngleStep      = 0.45f;
        constexpr float kRadiusStart    = 2.0f;
        constexpr float kRadiusStep     = 0.6f;
        constexpr float kCenterY        = 5.0f;

        const char* prefix = GetEntityPrefix();

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
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            auto billboard = billboard_entity->AddComponent<BillboardComponent>();
            billboard->SetVisible(true);
            billboard->SetFixedPixelSize(false);
            billboard->SetWorldSize(8.0f, 8.0f);
            billboard->SetFrontFace(VK_FRONT_FACE_CLOCKWISE);
            billboard->SetTexture(GetIconTextures(i));
            billboard->SetDomainTag(GetDomainTag());
        }
    }

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

    if (!InitPlaneGridResources())    return false;
    if (!CreateGeometryAndPrimitives()) return false;
    if (!InitializeECS())             return false;
    if (!InitializeCamera())          return false;

    return true;
}

void BillboardIconECSBase::Tick(double delta_time)
{
    WorkObject::Tick(delta_time);
}

