// Billboard ECS Example - Billboard as VertexTransformPolicy
//
// Billboard is no longer a special primitive type. It is a regular mesh (unit quad)
// with VertexTransformPolicy::BillboardAxisLocked or BillboardCameraFacing applied
// in the vertex shader. Size is expressed via Transform.scale.
//
// Entity layout:
//   MeshComponent (unit quad)  +  TransformComponent (pos + scale)  +
//   MaterialInstanceComponent (UnlitTexture3D + Billboard*)  +  RenderTagComponent

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
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/render/TextureMaterialBindingSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<iostream>
#include<memory>

#include "IconGradient.h"

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

static Color4f white_color(1, 1, 1, 1);

// Unit quad vertex data: Position2D + TexCoord
static const float kUnitQuadPosition[8] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.5f,  0.5f,
    -0.5f,  0.5f
};
static const float kUnitQuadTexCoord[8] = {
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f
};
static const uint16_t kUnitQuadIndices[6] = {0, 1, 2, 0, 2, 3};

#define SHOW_PLANE_GRID 1

class BillboardECSApp : public WorkObject
{
private:

    ECSContext* ecs_context = nullptr;

    // Entities
    Entity* billboard_entity = nullptr;
    Entity* textured_quad_entity = nullptr;
    Entity* textured_quad_domain_entity = nullptr;
    Entity* camera_entity = nullptr;

    // Shared unit quad geometry
    Geometry* geom_unit_quad = nullptr;

#ifdef SHOW_PLANE_GRID
    Entity* grid_entity = nullptr;
    Geometry* geom_plane_grid = nullptr;

    inline static const mtl::MaterialRecipe kPlaneGridCfg {
        .id            = "billboard_ecs_plane_grid",
        .preset        = mtl::MaterialPreset::VertexLuminance,
        .prim          = PrimitiveType::Lines,
        .vertex_policy = mtl::VertexTransformPolicy::Quad2D,
        .shading_model = mtl::SurfaceShadingModel::VertexLuminance,
        .schema        = mtl::ShaderDataSchema::Color4f,
        .has_explicit_schema = true,
        .pipeline      = GraphicsPipelinePreset::Solid3D,
    };
#endif//SHOW_PLANE_GRID

    // Billboard recipes: UnlitTexture3D + BillboardAxisLocked, one per blend mode
    inline static const mtl::MaterialRecipe kBillboardSolidCfg {
        .id            = "billboard_ecs_solid",
        .preset        = mtl::MaterialPreset::UnlitTexture,
        .dim           = mtl::MaterialRecipe::Dim::D3,
        .prim          = PrimitiveType::Triangles,
        .vertex_policy = mtl::VertexTransformPolicy::BillboardAxisLocked,
        .pipeline      = GraphicsPipelinePreset::Solid3D,
        .color_sources = {
            graph::ColorSource::MakeSampler2D(mtl::SamplerSlot::BaseColor),
        },
    };

    inline static const mtl::MaterialRecipe kBillboardAlphaCfg {
        .id            = "billboard_ecs_alpha",
        .preset        = mtl::MaterialPreset::UnlitTexture,
        .dim           = mtl::MaterialRecipe::Dim::D3,
        .prim          = PrimitiveType::Triangles,
        .vertex_policy = mtl::VertexTransformPolicy::BillboardAxisLocked,
        .pipeline      = GraphicsPipelinePreset::Alpha3D,
        .color_sources = {
            graph::ColorSource::MakeSampler2D(mtl::SamplerSlot::BaseColor),
        },
    };

    inline static const mtl::MaterialRecipe kBillboardDitherCfg {
        .id            = "billboard_ecs_dither",
        .preset        = mtl::MaterialPreset::UnlitTexture,
        .dim           = mtl::MaterialRecipe::Dim::D3,
        .prim          = PrimitiveType::Triangles,
        .vertex_policy = mtl::VertexTransformPolicy::BillboardAxisLocked,
        .pipeline      = GraphicsPipelinePreset::Dither3D,
        .color_sources = {
            graph::ColorSource::MakeSampler2D(mtl::SamplerSlot::BaseColor),
        },
    };

    inline static const mtl::MaterialRecipe kTexturedQuadCfg {
        .id            = "billboard_ecs_texture_quad",
        .preset        = mtl::MaterialPreset::UnlitTexture,
        .dim           = mtl::MaterialRecipe::Dim::D3,
        .prim          = PrimitiveType::Triangles,
        .vertex_policy = mtl::VertexTransformPolicy::BillboardAxisLocked,
        .pipeline      = GraphicsPipelinePreset::Alpha3D,
        .color_sources = {
            graph::ColorSource::MakeSampler2D(mtl::SamplerSlot::BaseColor),
        },
    };

    bool last_key_1 = false;
    bool last_key_2 = false;
    bool last_key_3 = false;

    GraphicsPipelinePreset current_pipeline = GraphicsPipelinePreset::Solid3D;
    int current_icon_index = 22;

private:

    static const char* GetPipelineName(GraphicsPipelinePreset preset)
    {
        switch (preset)
        {
        case GraphicsPipelinePreset::Solid3D:  return "Solid3D";
        case GraphicsPipelinePreset::Alpha3D:  return "Alpha3D";
        case GraphicsPipelinePreset::Dither3D: return "Dither3D";
        default:                               return "Unknown";
        }
    }

    bool ApplyBillboardMode(GraphicsPipelinePreset preset, int icon_index)
    {
        if (!ecs_context || !billboard_entity)
            return false;

        if (icon_index < 0)        icon_index = 0;
        if (icon_index >= kIconCount) icon_index = kIconCount - 1;

        current_pipeline   = preset;
        current_icon_index = icon_index;

        auto primitive = billboard_entity->GetComponent<PrimitiveComponent>();
        if (!primitive) return false;

        const mtl::MaterialRecipe* recipe = &kBillboardSolidCfg;
        if (preset == GraphicsPipelinePreset::Alpha3D)  recipe = &kBillboardAlphaCfg;
        if (preset == GraphicsPipelinePreset::Dither3D) recipe = &kBillboardDitherCfg;

        primitive->SetMaterialRecipe(RegisterMaterialRecipe(*recipe), nullptr, 0);

        auto texture_binding_system = ecs_context->GetSystem<TextureMaterialBindingSystem>();
        if (texture_binding_system)
            texture_binding_system->SubmitTextureBindingRequest(
                billboard_entity->GetID(),
                kIconTextures[current_icon_index],
                {},
                graph::mtl::SamplerSlot::BaseColor,
                graph::mtl::TextureSourceMode::Simple,
                graph::TextureChannelHint::Grayscale);

        std::cout << "[BillboardECS] Mode switched: " << GetPipelineName(preset)
                  << ", texture index=" << current_icon_index << std::endl;
        return true;
    }

    void HandleRuntimeModeSwitch()
    {
        if (!ecs_context)
            return;

        auto input_system = ecs_context->GetSystem<InputSystem>();
        if (!input_system)
            return;

        const bool key_1 = input_system->IsKeyDown(io::KeyboardButton::_1);
        const bool key_2 = input_system->IsKeyDown(io::KeyboardButton::_2);
        const bool key_3 = input_system->IsKeyDown(io::KeyboardButton::_3);

        if (key_1 && !last_key_1)
            ApplyBillboardMode(GraphicsPipelinePreset::Solid3D, 22);
        else if (key_2 && !last_key_2)
            ApplyBillboardMode(GraphicsPipelinePreset::Alpha3D, 10);
        else if (key_3 && !last_key_3)
            ApplyBillboardMode(GraphicsPipelinePreset::Dither3D, 6);

        last_key_1 = key_1;
        last_key_2 = key_2;
        last_key_3 = key_3;
    }

    /**
     * Initialize plane grid material and resources
     */
    bool InitPlaneGridResources()
    {
        return true;
    }

    /**
     * Create render geometry and primitives
     */
    bool CreateGeometryAndPrimitives()
    {
        auto* render_context = GetRenderContext();
        if (!render_context) return false;

        using namespace inline_geometry;

    #ifdef SHOW_PLANE_GRID
        {
            auto* graphics_context = render_context->GetGraphicsContext();
            if (!graphics_context) return false;

            GraphicsGeometryFactory geometry_factory(graphics_context);

            GeometryVertexFormat gvf;
            gvf.Set(VAN::Position,  VF_V2F);
            gvf.Set(VAN::Luminance, VF_V1UN8);

            auto pc = geometry_factory.CreateCreater(gvf);
            if (!pc) return false;

            PlaneGridCreateInfo pgci;
            pgci.grid_size.Set(500, 500);
            pgci.sub_count.Set(5, 5);
            pgci.lum = 128;
            pgci.sub_lum = 192;

            geom_plane_grid = CreatePlaneGrid2D(pc.get(), &pgci);
            if (!geom_plane_grid) return false;

            if (!geometry_factory.RegisterGeometry(geom_plane_grid)) return false;

            std::cout << "[BillboardECS] PlaneGrid geometry: " << (void*)geom_plane_grid << std::endl;
        }
    #endif//SHOW_PLANE_GRID

        return true;
    }

    /**
     * Ensure render systems are registered
     */
    bool EnsureRenderSystems()
    {
        return ecs_context != nullptr;
    }

    /**
     * Initialize ECS entities and components
     */
    bool InitializeECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context) return false;

        std::cout << "\n[BillboardECS] === ECS INITIALIZATION START ===" << std::endl;

        if (!EnsureRenderSystems()) return false;

        auto texture_binding_system = ecs_context->GetSystem<TextureMaterialBindingSystem>();
        if (!texture_binding_system) return false;

        // Create shared unit quad geometry
        {
            auto* graphics_context = GetGraphicsContext();
            if (!graphics_context) return false;

            GraphicsGeometryFactory geometry_factory(graphics_context);

            geom_unit_quad = WorkObject::CreateGeometry("UnitQuad",
                                                        4, 6, IndexType::U16,
                                                        {{VAN::Position, VF_V2F, kUnitQuadPosition},
                                                         {VAN::TexCoord, VF_V2F, kUnitQuadTexCoord}},
                                                        kUnitQuadIndices);
            if (!geom_unit_quad)
            {
                std::cout << "[BillboardECS] Failed to create unit quad geometry" << std::endl;
                return false;
            }
            std::cout << "[BillboardECS] Unit quad geometry: " << (void*)geom_unit_quad << std::endl;
        }

    #ifdef SHOW_PLANE_GRID
        std::cout << "\n[BillboardECS] Creating PlaneGrid entity..." << std::endl;
        {
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
        }
    #endif//SHOW_PLANE_GRID

        std::cout << "\n[BillboardECS] Creating Billboard entity..." << std::endl;
        {
            billboard_entity = ecs_context->CreateEntity<Entity>("Billboard");

            auto billboard_transform = billboard_entity->AddComponent<TransformComponent>(Mobility::Static);
            billboard_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            billboard_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            billboard_transform->SetLocalScale(glm::vec3(128.0f, 128.0f, 1.0f));  // 128x128 pixel size via scale
            billboard_transform->SetMovable(false);

            auto primitive = billboard_entity->AddComponent<PrimitiveComponent>();
            primitive->SetUnresolvedGeometry(geom_unit_quad);
            primitive->SetMaterialRecipe(RegisterMaterialRecipe(kBillboardSolidCfg), nullptr, 0);
            primitive->SetVisible(true);
            std::cout << "  -> PrimitiveComponent added (UnlitTexture3D + BillboardAxisLocked)" << std::endl;

            texture_binding_system->SubmitTextureBindingRequest(
                billboard_entity->GetID(),
                kIconTextures[current_icon_index],
                {},
                graph::mtl::SamplerSlot::BaseColor,
                graph::mtl::TextureSourceMode::Simple,
                graph::TextureChannelHint::Grayscale);
            std::cout << "  -> Texture binding submitted: gradient[" << current_icon_index << "]" << std::endl;
        }

        std::cout << "\n[BillboardECS] Creating TextureBinding entities..." << std::endl;
        {
            textured_quad_entity = ecs_context->CreateEntity<Entity>("TextureBoundQuad");
            auto transform = textured_quad_entity->AddComponent<TransformComponent>(Mobility::Static);
            transform->SetLocalPosition(glm::vec3(-12.0f, 10.0f, 0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(128.0f, 128.0f, 1.0f));
            transform->SetMovable(false);

            auto primitive = textured_quad_entity->AddComponent<PrimitiveComponent>();
            primitive->SetUnresolvedGeometry(geom_unit_quad);
            primitive->SetMaterialRecipe(RegisterMaterialRecipe(kTexturedQuadCfg), nullptr, 0);
            primitive->SetVisible(true);

            texture_binding_system->SubmitTextureBindingRequest(textured_quad_entity->GetID(), kIconTextures[22]);
        }

        {
            textured_quad_domain_entity = ecs_context->CreateEntity<Entity>("TextureBoundQuadDomain");
            auto transform = textured_quad_domain_entity->AddComponent<TransformComponent>(Mobility::Static);
            transform->SetLocalPosition(glm::vec3(12.0f, 10.0f, 0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(128.0f, 128.0f, 1.0f));
            transform->SetMovable(false);

            auto primitive = textured_quad_domain_entity->AddComponent<PrimitiveComponent>();
            primitive->SetUnresolvedGeometry(geom_unit_quad);
            primitive->SetMaterialRecipe(RegisterMaterialRecipe(kTexturedQuadCfg), nullptr, 0);
            primitive->SetVisible(true);

            texture_binding_system->SubmitTextureBindingRequest(textured_quad_domain_entity->GetID(),
                                                                kIconTextures[10],
                                                                "3001");
        }

        return true;
    }

    /**
     * Initialize camera
     */
    bool InitializeCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 45.0f;
        camera->yaw = 45.0f;
        camera->pitch = -25.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~BillboardECSApp()
    {
    }

    bool Init() override
    {
        std::cout << "\n\n===== BILLBOARD ECS APP INITIALIZATION START =====\n" << std::endl;

        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if (!InitPlaneGridResources()) return false;
        if (!CreateGeometryAndPrimitives()) return false;
        if (!InitializeECS()) return false;
        if (!InitializeCamera()) return false;

        std::cout << "\n[BillboardECS] ===== APP INITIALIZATION COMPLETE =====\n" << std::endl;

        return true;
    }

    void Tick(double delta_time) override
    {
        static int frame_count = 0;
        frame_count++;

        if (frame_count <= 3)
        {
            std::cout << "\n[BillboardECS] Frame " << frame_count << " starting..." << std::endl;
            if (ecs_context)
            {
                std::cout << "  -> Entity count: " << ecs_context->GetEntityCount() << std::endl;
            }
        }

        WorkObject::Tick(delta_time);
        HandleRuntimeModeSwitch();

        if (frame_count <= 3)
        {
            std::cout << "[BillboardECS] Frame " << frame_count << " end" << std::endl;
        }
    }
};//class BillboardECSApp:public WorkObject

int os_main(int argc, os_char** argv)
{
    return RunFramework<BillboardECSApp>(OS_TEXT("Billboard ECS Example"), argc, argv, 1280, 720);
}

