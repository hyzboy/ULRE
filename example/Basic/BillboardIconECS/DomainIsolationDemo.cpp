// R8 — domain_id 语义化演示：双图标集 Billboard 阵列
//
// 两组 Billboard 螺旋各使用不同的图标集（Freepik / Gradient），
// PlaneGrid 材质通过 domain_id 区分批次，验证 ResourceDomain 隔离端到端正确。

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/color/Color.h>

// ECS headers
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
#include<iostream>
#include<memory>
#include<cmath>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

// ── Freepik icon set (50 icons) ─────────────────────────────────────────────

static const os_char* kFreepikIcons[] =
{
    OS_TEXT("res/image/icon/freepik/001-online resume.Tex2D"),
    OS_TEXT("res/image/icon/freepik/002-salary.Tex2D"),
    OS_TEXT("res/image/icon/freepik/003-application.Tex2D"),
    OS_TEXT("res/image/icon/freepik/004-job interview.Tex2D"),
    OS_TEXT("res/image/icon/freepik/005-investment.Tex2D"),
    OS_TEXT("res/image/icon/freepik/006-job seeker.Tex2D"),
    OS_TEXT("res/image/icon/freepik/007-file.Tex2D"),
    OS_TEXT("res/image/icon/freepik/008-Cooperation.Tex2D"),
    OS_TEXT("res/image/icon/freepik/009-CV.Tex2D"),
    OS_TEXT("res/image/icon/freepik/010-personal data.Tex2D"),
    OS_TEXT("res/image/icon/freepik/011-job interview.Tex2D"),
    OS_TEXT("res/image/icon/freepik/012-calendar.Tex2D"),
    OS_TEXT("res/image/icon/freepik/013-home.Tex2D"),
    OS_TEXT("res/image/icon/freepik/014-location.Tex2D"),
    OS_TEXT("res/image/icon/freepik/015-photo.Tex2D"),
    OS_TEXT("res/image/icon/freepik/016-file.Tex2D"),
    OS_TEXT("res/image/icon/freepik/017-book.Tex2D"),
    OS_TEXT("res/image/icon/freepik/018-profile.Tex2D"),
    OS_TEXT("res/image/icon/freepik/019-employee.Tex2D"),
    OS_TEXT("res/image/icon/freepik/020-file.Tex2D"),
    OS_TEXT("res/image/icon/freepik/021-best employee.Tex2D"),
    OS_TEXT("res/image/icon/freepik/022-achievement.Tex2D"),
    OS_TEXT("res/image/icon/freepik/023-badge.Tex2D"),
    OS_TEXT("res/image/icon/freepik/024-job opportunities.Tex2D"),
    OS_TEXT("res/image/icon/freepik/025-skill.Tex2D"),
    OS_TEXT("res/image/icon/freepik/026-working.Tex2D"),
    OS_TEXT("res/image/icon/freepik/027-trophy.Tex2D"),
    OS_TEXT("res/image/icon/freepik/028-CV.Tex2D"),
    OS_TEXT("res/image/icon/freepik/029-headhunter.Tex2D"),
    OS_TEXT("res/image/icon/freepik/030-CV.Tex2D"),
    OS_TEXT("res/image/icon/freepik/031-best employee.Tex2D"),
    OS_TEXT("res/image/icon/freepik/032-chart.Tex2D"),
    OS_TEXT("res/image/icon/freepik/033-headhunter.Tex2D"),
    OS_TEXT("res/image/icon/freepik/034-certificate.Tex2D"),
    OS_TEXT("res/image/icon/freepik/035-job offer.Tex2D"),
    OS_TEXT("res/image/icon/freepik/036-check.Tex2D"),
    OS_TEXT("res/image/icon/freepik/037-graduated.Tex2D"),
    OS_TEXT("res/image/icon/freepik/038-profile.Tex2D"),
    OS_TEXT("res/image/icon/freepik/039-photo.Tex2D"),
    OS_TEXT("res/image/icon/freepik/040-envelope.Tex2D"),
    OS_TEXT("res/image/icon/freepik/041-curriculum vitae.Tex2D"),
    OS_TEXT("res/image/icon/freepik/042-headhunting.Tex2D"),
    OS_TEXT("res/image/icon/freepik/043-portfolio.Tex2D"),
    OS_TEXT("res/image/icon/freepik/044-chart.Tex2D"),
    OS_TEXT("res/image/icon/freepik/045-email.Tex2D"),
    OS_TEXT("res/image/icon/freepik/046-portfolio.Tex2D"),
    OS_TEXT("res/image/icon/freepik/047-contract.Tex2D"),
    OS_TEXT("res/image/icon/freepik/048-office chair.Tex2D"),
    OS_TEXT("res/image/icon/freepik/049-office building.Tex2D"),
    OS_TEXT("res/image/icon/freepik/050-profile.Tex2D"),
};
static constexpr int kFreepikCount = 50;

// ── Gradient icon set (23 icons) ────────────────────────────────────────────

static const os_char* kGradientIcons[] =
{
    OS_TEXT("res/image/gradient/0.Tex2D"),
    OS_TEXT("res/image/gradient/1.Tex2D"),
    OS_TEXT("res/image/gradient/2.Tex2D"),
    OS_TEXT("res/image/gradient/3.Tex2D"),
    OS_TEXT("res/image/gradient/4.Tex2D"),
    OS_TEXT("res/image/gradient/5.Tex2D"),
    OS_TEXT("res/image/gradient/6.Tex2D"),
    OS_TEXT("res/image/gradient/7.Tex2D"),
    OS_TEXT("res/image/gradient/8.Tex2D"),
    OS_TEXT("res/image/gradient/9.Tex2D"),
    OS_TEXT("res/image/gradient/10.Tex2D"),
    OS_TEXT("res/image/gradient/11.Tex2D"),
    OS_TEXT("res/image/gradient/12.Tex2D"),
    OS_TEXT("res/image/gradient/13.Tex2D"),
    OS_TEXT("res/image/gradient/14.Tex2D"),
    OS_TEXT("res/image/gradient/15.Tex2D"),
    OS_TEXT("res/image/gradient/16.Tex2D"),
    OS_TEXT("res/image/gradient/17.Tex2D"),
    OS_TEXT("res/image/gradient/18.Tex2D"),
    OS_TEXT("res/image/gradient/19.Tex2D"),
    OS_TEXT("res/image/gradient/20.Tex2D"),
    OS_TEXT("res/image/gradient/21.Tex2D"),
    OS_TEXT("res/image/gradient/22.Tex2D"),
};
static constexpr int kGradientCount = 23;

// Explicit ResourceDomain IDs used by this demo.
// Numeric strings are parsed directly as domain_id by MaterialRecipeRegistry.
static constexpr const char *kFreepikDomainID  = "1001";
static constexpr const char *kGradientDomainID = "1002";
static constexpr const char *kGridDomainID     = "2001";

// ── App ─────────────────────────────────────────────────────────────────────

static Color4f white_color(1, 1, 1, 1);

class DomainIsolationApp : public WorkObject
{
private:

    ECSContext* ecs_context = nullptr;

    Entity* grid_entity   = nullptr;
    Entity* camera_entity = nullptr;

    // PlaneGrid resources
    Geometry*         geom_plane_grid = nullptr;

    inline static const mtl::MaterialRecipe kPlaneGridCfg {
        .id        = "domain_demo_plane_grid",
        .domain_id = kGridDomainID,
        .preset    = mtl::MaterialPreset::VertexLuminance2D,
        .prim      = PrimitiveType::Lines,
        .pipeline  = GraphicsPipelinePreset::Solid3D,
    };

private:

    bool InitPlaneGridResources()
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
        pgci.lum     = 128;
        pgci.sub_lum = 192;

        geom_plane_grid = CreatePlaneGrid2D(pc.get(), &pgci);
        if (!geom_plane_grid) return false;

        if (!geometry_factory.RegisterGeometry(geom_plane_grid)) return false;

        return true;
    }

    bool EnsureRenderSystems()
    {
        if (!ecs_context) return false;

        auto quad_prepare = ecs_context->GetSystem<QuadResourcePrepareSystem>();
        if (!quad_prepare)
        {
            quad_prepare = ecs_context->RegisterRenderSystem<QuadResourcePrepareSystem>();
            quad_prepare->SetWorld(ecs_context);
            if (ecs_context->IsActive())
            {
                quad_prepare->OnDependenciesReady();
                quad_prepare->Initialize();
            }
        }

        auto quad_binding = ecs_context->GetSystem<QuadMaterialBindingSystem>();
        if (!quad_binding)
        {
            quad_binding = ecs_context->RegisterRenderSystem<QuadMaterialBindingSystem>();
            quad_binding->SetWorld(ecs_context);
            if (ecs_context->IsActive())
            {
                quad_binding->OnDependenciesReady();
                quad_binding->Initialize();
            }
        }

        auto facing = ecs_context->GetSystem<FacingTransformSystem>();
        if (!facing)
        {
            facing = ecs_context->RegisterTickSystem<FacingTransformSystem>();
            facing->SetWorld(ecs_context);
            facing->SetCameraInfo(GetCameraInfo());
            if (ecs_context->IsActive())
            {
                facing->OnDependenciesReady();
                facing->Initialize();
            }
        }

        return quad_prepare && quad_binding && facing;
    }

    /// 创建一组螺旋排列的 billboard 实体
    void CreateSpiralBillboards(
        const char*     prefix,
        const char*     domain_tag,
        const os_char** icon_list,
        int             icon_count,
        int             billboard_count,
        float           center_x,
        float           center_y)
    {
        constexpr float kAngleStep   = 0.45f;
        constexpr float kRadiusStart = 2.0f;
        constexpr float kRadiusStep  = 0.6f;

        for (int i = 0; i < billboard_count; ++i)
        {
            const float angle  = kAngleStep * static_cast<float>(i);
            const float radius = kRadiusStart + kRadiusStep * angle;

            const float x = center_x + std::cos(angle) * radius;
            const float y = center_y + std::sin(angle) * radius;

            const std::string name = std::string(prefix) + std::to_string(i);
            Entity* e = ecs_context->CreateEntity<Entity>(name.c_str());
            if (!e) continue;

            auto transform = e->AddComponent<TransformComponent>(Mobility::Static);
            transform->SetLocalPosition(glm::vec3(x, y, 0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            auto billboard = e->AddComponent<BillboardComponent>();
            billboard->SetVisible(true);
            billboard->SetFixedPixelSize(false);
            billboard->SetWorldSize(8.0f, 8.0f);
            billboard->SetFrontFace(VK_FRONT_FACE_CLOCKWISE);
            billboard->SetTexture(icon_list[i % icon_count]);
            billboard->SetDomainTag(domain_tag);
        }
    }

    bool InitializeECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context) return false;

        QuadResourcePrepareSystem::SetFixedSizeForWorld(ecs_context, false);    // dynamic world-space billboards

        if (!EnsureRenderSystems()) return false;

        // PlaneGrid entity
        {
            grid_entity = ecs_context->CreateEntity<Entity>("PlaneGrid");

            auto transform = grid_entity->AddComponent<TransformComponent>(Mobility::Static);
            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            auto prim = grid_entity->AddComponent<PrimitiveComponent>();
            prim->SetUnresolvedGeometry(geom_plane_grid);
            prim->SetMaterialRecord(&kPlaneGridCfg, &white_color, sizeof(white_color));
            prim->SetVisible(true);
        }

        // 左侧螺旋：Freepik 图标（50 个 billboard）
        CreateSpiralBillboards("Freepik_", kFreepikDomainID, kFreepikIcons, kFreepikCount, 50, -30.0f, 5.0f);

        // 右侧螺旋：Gradient 图标（50 个 billboard）
        CreateSpiralBillboards("Gradient_", kGradientDomainID, kGradientIcons, kGradientCount, 50, 30.0f, 5.0f);

        return true;
    }

    bool InitializeCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera   = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode   = CameraComponent::ControlMode::ViewModel;
        camera->target         = math::Vector3f(0.0f, 5.0f, 0.0f);
        camera->distance       = 80.0f;
        camera->yaw            = 0.0f;
        camera->pitch          = -15.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty   = true;

        camera->camera_data   = GetCamera();
        camera->camera_info   = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:

    ~DomainIsolationApp()
    {
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if (!InitPlaneGridResources())      return false;
        if (!InitializeECS())               return false;
        if (!InitializeCamera())            return false;

        // R8 验证：双螺旋已创建
        {
            printf("[R8 DomainIsolation] Freepik spiral: 50 billboards (left)\n");
            printf("[R8 DomainIsolation] Gradient spiral: 50 billboards (right)\n");
            printf("[R8 DomainIsolation] ResourceDomains: freepik=%s gradient=%s grid=%s\n",
                   kFreepikDomainID, kGradientDomainID, kGridDomainID);
        }

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<DomainIsolationApp>(
        OS_TEXT("R8 Domain Isolation Demo - Dual Icon Billboard Arrays"),
        argc, argv, 1280, 720);
}

