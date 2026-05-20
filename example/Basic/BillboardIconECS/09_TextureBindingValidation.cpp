// Phase 4E — TextureBinding 新路径最小验证示例
//
// 验证目标：
//   unit quad geometry + PrimitiveComponent + 显式 TextureBindingTask 可独立驱动贴图绑定。
//   两个实体使用共享 unit quad mesh，尺寸通过 Transform.scale 表达。
//
// 两个实体：
//   1. "NonDomainQuad"  — non-domain 路径，domain_tag 为空
//   2. "DomainQuad"     — domain 路径，SetDomainTag("texture_binding_test")

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/color/Color.h>

// ECS
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/TextureMaterialBindingSystem.h>

// plane-grid shared resources
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<iostream>

#include "IconGradient.h"

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

static Color4f white_color(1, 1, 1, 1);

// Unit quad vertex data: Position2D + TexCoord
static const float kTBVQuadPosition[8] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.5f,  0.5f,
    -0.5f,  0.5f
};
static const float kTBVQuadTexCoord[8] = {
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f
};
static const uint16_t kTBVQuadIndices[6] = {0, 1, 2, 0, 2, 3};

class TextureBindingValidationApp : public WorkObject
{
private:

    ECSContext* ecs_context = nullptr;

    Entity* camera_entity         = nullptr;
    Entity* non_domain_entity     = nullptr;
    Entity* domain_entity         = nullptr;
    Entity* grid_entity           = nullptr;

    Geometry* geom_plane_grid     = nullptr;
    Geometry* geom_unit_quad      = nullptr;

    inline static const mtl::MaterialRecipe kPlaneGridCfg {
        .id            = "tbv_plane_grid",
        .preset        = mtl::MaterialPreset::VertexLuminance,
        .prim          = PrimitiveType::Lines,
        .vertex_policy = mtl::VertexTransformPolicy::Quad2D,
        .shading_model = mtl::SurfaceShadingModel::VertexLuminance,
        .schema        = mtl::ShaderDataSchema::Color4f,
        .has_explicit_schema = true,
    };

    inline static const mtl::MaterialRecipe kTexturedQuadCfg {
        .id            = "tbv_textured_quad",
        .preset        = mtl::MaterialPreset::UnlitTexture,
        .dim           = mtl::MaterialRecipe::Dim::D3,
        .prim          = PrimitiveType::Triangles,
        .default_render_state = { .blend = RenderAlphaMode::Transparent },
        .color_sources = {
            graph::ColorSource::MakeSampler2D(mtl::SamplerSlot::BaseColor),
        },
    };

private:

    bool CreatePlaneGrid()
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
        return ecs_context != nullptr;
    }

    bool InitializeECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context) return false;

        std::cout << "\n[TBV] === ECS INITIALIZATION START ===" << std::endl;

        if (!EnsureRenderSystems()) return false;

        // Create shared unit quad geometry
        {
            geom_unit_quad = WorkObject::CreateGeometry("TBV_UnitQuad",
                                                        4, 6, IndexType::U16,
                                                        {{VAN::Position, VF_V2F, kTBVQuadPosition},
                                                         {VAN::TexCoord, VF_V2F, kTBVQuadTexCoord}},
                                                        kTBVQuadIndices);
            if (!geom_unit_quad)
            {
                std::cout << "[TBV] Failed to create unit quad geometry" << std::endl;
                return false;
            }
        }

        // ── Plane grid entity ─────────────────────────────────────────────────
        {
            grid_entity = ecs_context->CreateEntity<Entity>("PlaneGrid");

            auto t = grid_entity->AddComponent<TransformComponent>(Mobility::Static);
            t->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            t->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            t->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            t->SetMovable(false);

            auto p = grid_entity->AddComponent<PrimitiveComponent>();
            p->SetUnresolvedGeometry(geom_plane_grid);
            p->SetMaterialRecipe(RegisterMaterialRecipe(kPlaneGridCfg), &white_color, sizeof(white_color));
            p->SetVisible(true);
        }

        auto texture_binding_system = ecs_context->GetSystem<TextureMaterialBindingSystem>();
        if (!texture_binding_system) return false;

        // ── Non-domain quad ───────────────────────────────────────────────────
        // Validates: explicit TextureBindingTask submit, domain_tag empty → single texture path.
        {
            non_domain_entity = ecs_context->CreateEntity<Entity>("NonDomainQuad");

            auto t = non_domain_entity->AddComponent<TransformComponent>(Mobility::Static);
            t->SetLocalPosition(glm::vec3(-8.0f, 8.0f, 0.0f));
            t->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            t->SetLocalScale(glm::vec3(8.0f, 8.0f, 1.0f));  // size via scale
            t->SetMovable(false);

            auto p = non_domain_entity->AddComponent<PrimitiveComponent>();
            p->SetUnresolvedGeometry(geom_unit_quad);
            p->SetMaterialRecipe(RegisterMaterialRecipe(kTexturedQuadCfg));
            p->SetVisible(true);

            texture_binding_system->SubmitTextureBindingRequest(non_domain_entity->GetID(), kIconTextures[3]);

            std::cout << "[TBV] NonDomainQuad created, texture=res/image/gradient/3.Tex2D" << std::endl;
        }

        // ── Domain quad ───────────────────────────────────────────────────────
        // Validates: explicit TextureBindingTask submit with domain_tag → domain / texture-array path.
        {
            domain_entity = ecs_context->CreateEntity<Entity>("DomainQuad");

            auto t = domain_entity->AddComponent<TransformComponent>(Mobility::Static);
            t->SetLocalPosition(glm::vec3(8.0f, 8.0f, 0.0f));
            t->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            t->SetLocalScale(glm::vec3(8.0f, 8.0f, 1.0f));  // size via scale
            t->SetMovable(false);

            auto p = domain_entity->AddComponent<PrimitiveComponent>();
            p->SetUnresolvedGeometry(geom_unit_quad);
            p->SetMaterialRecipe(RegisterMaterialRecipe(kTexturedQuadCfg));
            p->SetVisible(true);

            texture_binding_system->SubmitTextureBindingRequest(domain_entity->GetID(),
                                                                kIconTextures[10],
                                                                "texture_binding_test");

            std::cout << "[TBV] DomainQuad created, texture=res/image/gradient/10.Tex2D, domain_tag=texture_binding_test" << std::endl;
        }

        std::cout << "[TBV] === ECS INITIALIZATION COMPLETE ===" << std::endl;

        return true;
    }

    bool InitializeCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera   = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode   = CameraComponent::ControlMode::ViewModel;
        camera->target         = math::Vector3f(0.0f, 8.0f, 0.0f);
        camera->distance       = 30.0f;
        camera->yaw            = 0.0f;
        camera->pitch          = -10.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty   = true;

        camera->camera_data    = GetCamera();
        camera->camera_info    = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera->viewport_info  = GetViewportInfo();

        return true;
    }

public:

    bool Init() override
    {
        std::cout << "\n===== TextureBinding Validation (Phase 4E) =====\n" << std::endl;

        SetClearColor(Color4f(0.15f, 0.15f, 0.15f, 1.0f));

        if (!CreatePlaneGrid())   return false;
        if (!InitializeECS())     return false;
        if (!InitializeCamera())  return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
    }
};//class TextureBindingValidationApp

int os_main(int argc, os_char** argv)
{
    return RunFramework<TextureBindingValidationApp>(
        OS_TEXT("TextureBinding Validation (Phase 4E)"), argc, argv, 1280, 720);
}
