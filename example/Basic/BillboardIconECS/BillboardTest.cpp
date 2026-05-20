// Billboard (ECS)
//
// This example demonstrates rendering a billboard and a plane grid using ECS.

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/MaterialRecipeRegistry.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/ShaderMaterialProgramManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/color/Color.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<iostream>
#include<memory>
#include<cstdint>
#include<cstdio>

//#define SHOW_PLANE_GRID 1

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

// Unit quad vertex data: Position2D + TexCoord.
// Billboard size is expressed only through Transform.scale.
static const float kUnitQuadPosition[8]=
{
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.5f,  0.5f,
    -0.5f,  0.5f
};

static const float kUnitQuadTexCoord[8]=
{
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f
};

static const uint16_t kUnitQuadIndices[6]={0,1,2,0,2,3};

constexpr float kBillboardPixelWidth  = 512.0f;
constexpr float kBillboardPixelHeight = 512.0f;

static Color4f white_color(1,1,1,1);

class TestApp:public WorkObject
{
private:

#ifdef SHOW_PLANE_GRID
    inline static const mtl::MaterialRecipe kPlaneGridCfg {
        .id       = "billboard_test_plane_grid",
        .preset   = mtl::MaterialPreset::VertexLuminance,
        .prim     = PrimitiveType::Lines,
        .pipeline = GraphicsPipelinePreset::Solid3D,
    };
#endif//SHOW_PLANE_GRID

    inline static const mtl::MaterialRecipe kBillboardCfg {
        .id           = "billboard_test_fixed",
        .preset       = mtl::MaterialPreset::UnlitTexture,
        .dim          = mtl::MaterialRecipe::Dim::D3,
        .prim         = PrimitiveType::Triangles,
        .vertex_policy= mtl::VertexTransformPolicy::BillboardAxisLocked,
        .pipeline     = GraphicsPipelinePreset::Alpha3D,
        .color_sources = {
            graph::ColorSource::MakeSampler2D(mtl::SamplerSlot::BaseColor),
        },
        .textures = {
            { mtl::SamplerSlot::BaseColor, "res/image/lena.Tex2D" }
        },
    };

    void DumpShaderGenValidationSample()
    {
        if(shadergen_report_dumped)
            return;

        auto *graphics_context = GetGraphicsContext();
        if(!graphics_context)
            return;

        auto reports = graphics_context->GetShaderGenRecentValidationReports(32);

        std::fprintf(stderr,"[ShaderGenValidationSample] report_count=%zu\n",reports.size());

        size_t printed = 0;
        for(const auto &record : reports)
        {
            if(printed >= 5)
                break;

            std::fprintf(stderr,
                         "[ShaderGenValidationSample] seq=%llu valid=%d errors=%u warnings=%u\n",
                         static_cast<unsigned long long>(record.sequence),
                         record.report.overall_valid ? 1 : 0,
                         record.report.error_count,
                         record.report.warning_count);

            ++printed;
        }

        shadergen_report_dumped = true;
    }

private:

    ECSContext *  ecs_context       = nullptr;
    Entity *      camera_entity     = nullptr;

    Entity *      billboard_entity  = nullptr;
    Geometry *    geom_billboard    = nullptr;

#ifdef SHOW_PLANE_GRID
    Entity *      grid_entity       = nullptr;
    Geometry *    geom_plane_grid   = nullptr;
#endif//SHOW_PLANE_GRID

    bool          shadergen_report_dumped = false;

private:

    bool CreateRenderObject()
    {
        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        GraphicsGeometryFactory geometry_factory(graphics_context);

        using namespace inline_geometry;

    #ifdef SHOW_PLANE_GRID
        {
            GeometryVertexFormat gvf_lum;
            gvf_lum.Set(VAN::Position, VF_V2F);
            gvf_lum.Set(VAN::Luminance, VF_V1UN8);
            auto pc = geometry_factory.CreateCreater(gvf_lum);
            if (!pc)
                return false;

            PlaneGridCreateInfo pgci;
            pgci.grid_size.Set(500, 500);
            pgci.sub_count.Set(5, 5);
            pgci.lum = 128;
            pgci.sub_lum = 192;

            geom_plane_grid = CreatePlaneGrid2D(pc.get(), &pgci);
            if(!geom_plane_grid)
                return false;

            if(!geometry_factory.RegisterGeometry(geom_plane_grid))
                return false;

            std::cout << "[BillboardECS] PlaneGrid geometry: " << (void*)geom_plane_grid << std::endl;
        }
    #endif//SHOW_PLANE_GRID

        {
            geom_billboard = WorkObject::CreateGeometry("BillboardBase_UnitQuad",
                                                        4,
                                                        6,
                                                        IndexType::U16,
                                                        {{VAN::Position, VF_V2F, kUnitQuadPosition},
                                                         {VAN::TexCoord, VF_V2F, kUnitQuadTexCoord}},
                                                        kUnitQuadIndices);
            if(!geom_billboard)
                return false;

            std::cout << "[BillboardECS] Billboard geometry: " << (void*)geom_billboard << std::endl;
        }

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if(!ecs_context)
            return false;

#ifdef SHOW_PLANE_GRID
        grid_entity = ecs_context->CreateEntity<Entity>("PlaneGrid");
        auto grid_transform = grid_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto grid_primitive = grid_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        grid_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        grid_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        grid_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        grid_transform->SetMovable(false);

        grid_primitive->SetUnresolvedGeometry(geom_plane_grid);
        grid_primitive->SetMaterialRecipe(RegisterMaterialRecipe(kPlaneGridCfg), &white_color, sizeof(white_color));
        grid_primitive->SetVisible(true);
#endif//SHOW_PLANE_GRID

        billboard_entity = ecs_context->CreateEntity<Entity>("Billboard_AxisLocked");
        auto billboard_transform = billboard_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto billboard_primitive = billboard_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        billboard_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        billboard_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        billboard_transform->SetLocalScale(glm::vec3(kBillboardPixelWidth, kBillboardPixelHeight, 1.0f));
        billboard_transform->SetMovable(false);

        billboard_primitive->SetUnresolvedGeometry(geom_billboard);
        billboard_primitive->SetMaterialRecipe(RegisterMaterialRecipe(kBillboardCfg), nullptr, 0);
        billboard_primitive->SetVisible(true);

        return true;
    }

    bool InitCamera()
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
        camera->camera_info = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!CreateRenderObject())
            return false;

        if(!InitECS())
            return false;

        if(!InitCamera())
            return false;

        DumpShaderGenValidationSample();

        return true;
    }
};//class TestApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Billboard (ECS)"),argc,argv,1280,720);
}


