// Billboard (ECS)
//
// This example demonstrates rendering a billboard and a plane grid using ECS.

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/MaterialManager.h>
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

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

static const float billboard_position_data[12]=
{
    -0.5f, -0.5f, 0.0f,
     0.5f, -0.5f, 0.0f,
     0.5f,  0.5f, 0.0f,
    -0.5f,  0.5f, 0.0f
};

static const uint16_t billboard_index_data[6]={0,1,2,0,2,3};

static Color4f white_color(1,1,1,1);

class TestApp:public WorkObject
{
private:

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

    ECSContext *  ecs_context      = nullptr;
    Entity *      grid_entity    = nullptr;
    Entity *      billboard_entity = nullptr;
    Entity *      camera_entity  = nullptr;

    MaterialTemplate *          mtl_plane_grid      = nullptr;
    MaterialInstance *  mi_plane_grid       = nullptr;
    Geometry *          geom_plane_grid     = nullptr;
    Primitive *         prim_plane_grid     = nullptr;

    MaterialInstance *  mi_billboard        = nullptr;
    Primitive *         prim_billboard      = nullptr;

    Texture2D *         texture             = nullptr;
    Sampler *           sampler             = nullptr;

    bool                shadergen_report_dumped = false;

private:

    bool InitPlaneGridMP()
    {

        static const mtl::MaterialAssetRecord kPlaneGridCfg {
            .id       = "billboard_test_plane_grid",
            .preset   = mtl::MaterialPreset::VertexLuminance2D,
            .prim     = PrimitiveType::Lines,
            .pipeline = GraphicsPipelinePreset::Solid3D,
            .mi_vil_overrides = {
                { VAN::Luminance, VF_V1UN8 },
            },
        };
        mi_plane_grid = AcquireMI(kPlaneGridCfg,
                          &white_color, sizeof(white_color));
        if(!mi_plane_grid)
            return false;

        mtl_plane_grid = mi_plane_grid->GetMaterial();

        std::cout << "[BillboardECS] PlaneGrid material: " << (void*)mtl_plane_grid << std::endl;

        std::cout << "[BillboardECS] PlaneGrid MI: " << (void*)mi_plane_grid << std::endl;

        return true;
    }

    bool InitBillboardMP()
    {

        static const mtl::MaterialAssetRecord kBillboardCfg {
            .id        = "billboard_test_fixed",
            .preset    = mtl::MaterialPreset::Billboard2DFixed,
            .prim      = PrimitiveType::Billboard,
            .billboard = { .fixed_size = true },
        };
        mi_billboard = AcquireMI(kBillboardCfg);
        if(!mi_billboard)
            return false;

        std::cout << "[BillboardECS] Billboard MI: " << (void*)mi_billboard
                  << ", MaterialTemplate: " << (void*)mi_billboard->GetMaterial() << std::endl;

        return true;
    }

    bool InitTexture()
    {

        TextureManager *tex_manager = GetTextureManager();
        if (!tex_manager)
            return false;

        texture = tex_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"), true);
        if(!texture)
            return false;

        std::cout << "[BillboardECS] Texture loaded: " << (void*)texture
                  << " (" << texture->GetWidth() << "x" << texture->GetHeight() << ")" << std::endl;

        auto* sampler_manager = GetSamplerManager();
        if (!sampler_manager)
            return false;

        sampler = sampler_manager->CreateSampler();

        std::cout << "[BillboardECS] Sampler created: " << (void*)sampler << std::endl;

        const bool bind_ok = mi_billboard->GetMaterial()->BindTextureSampler(mtl::SamplerSlot::BaseColor,
                                              texture,
                                              sampler);
        std::cout << "[BillboardECS] BindTextureSampler(BaseColor): " << (bind_ok ? "OK" : "FAILED")
                  << std::endl;
        if(!bind_ok)
            return false;

        math::Vector2u texture_size(texture->GetWidth(), texture->GetHeight());
        mi_billboard->WriteMIData(texture_size);
        std::cout << "[BillboardECS] Billboard MI data written (texture size)." << std::endl;

        return true;
    }

    bool CreateRenderObject()
    {

        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        GraphicsGeometryFactory geometry_factory(graphics_context);

        using namespace inline_geometry;

        {
            auto pc = geometry_factory.CreateCreater(mi_plane_grid);
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
            prim_plane_grid = geometry_factory.CreatePrimitive(geom_plane_grid, mi_plane_grid);
            if(!prim_plane_grid)
                return false;

            std::cout << "[BillboardECS] PlaneGrid geometry: " << (void*)geom_plane_grid
                      << ", primitive: " << (void*)prim_plane_grid << std::endl;
        }

        {
            auto pc = geometry_factory.CreateCreater(mi_billboard);
            if (!pc)
                return false;

            pc->Init("Billboard", 4, 6, IndexType::U16);

            if(!pc->WriteVAB(VAN::Position, VF_V3F, billboard_position_data))
                return false;

            if(!pc->WriteIBO(billboard_index_data))
                return false;

            prim_billboard = geometry_factory.CreatePrimitive(pc.get(), mi_billboard);
            if(!prim_billboard)
                return false;

            std::cout << "[BillboardECS] Billboard primitive: " << (void*)prim_billboard << std::endl;
        }

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if(!ecs_context)
            return false;

        grid_entity = ecs_context->CreateEntity<Entity>("PlaneGrid");
        auto grid_transform = grid_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto grid_primitive = grid_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        grid_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        grid_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        grid_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        grid_transform->SetMovable(false);

        grid_primitive->SetPrimitive(prim_plane_grid);
        grid_primitive->SetVisible(true);

        billboard_entity = ecs_context->CreateEntity<Entity>("Billboard");
        auto billboard_transform = billboard_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto billboard_primitive = billboard_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        billboard_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        billboard_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        billboard_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        billboard_transform->SetMovable(false);

        billboard_primitive->SetPrimitive(prim_billboard);
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
    ~TestApp()
    {
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!InitPlaneGridMP())
            return false;

        if(!InitBillboardMP())
            return false;

        if(!InitTexture())
            return false;

        if(!CreateRenderObject())
            return false;

        if(!InitECS())
            return false;

        if(!InitCamera())
            return false;

        DumpShaderGenValidationSample();

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        DumpShaderGenValidationSample();
    }
};//class TestApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Billboard (ECS)"),argc,argv,1280,720);
}


