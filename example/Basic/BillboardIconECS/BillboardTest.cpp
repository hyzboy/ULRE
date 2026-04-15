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
    const VIL *         vil_plane_grid      = nullptr;
    MaterialInstanceHandle handle_plane_grid = InvalidMaterialInstanceHandle;
    PrimitiveMaterialSlot slot_plane_grid;
    Geometry *          geom_plane_grid     = nullptr;
    Primitive *         prim_plane_grid     = nullptr;

    MaterialTemplate *  mtl_billboard       = nullptr;
    const VIL *         vil_billboard       = nullptr;
    MaterialInstanceHandle handle_billboard = InvalidMaterialInstanceHandle;
    PrimitiveMaterialSlot slot_billboard;
    Primitive *         prim_billboard      = nullptr;

    Texture2D *         texture             = nullptr;
    std::weak_ptr<Sampler> sampler;

    bool                shadergen_report_dumped = false;

private:

    bool InitPlaneGridMP()
    {

        static const mtl::MaterialAssetRecord kPlaneGridCfg {
            .id       = "billboard_test_plane_grid",
            .preset   = mtl::MaterialPreset::VertexLuminance2D,
            .prim     = PrimitiveType::Lines,
            .pipeline = GraphicsPipelinePreset::Solid3D,
        };

        auto *registry = GetMaterialAssetRegistry();
        if (!registry)
            return false;

        const MaterialDomainHandle handle = registry->Acquire(kPlaneGridCfg);
        if (!handle.IsValid())
            return false;

        mtl_plane_grid = handle.material;
        vil_plane_grid = registry->ResolveVIL(handle.material, kPlaneGridCfg, nullptr);
        if (!vil_plane_grid)
            vil_plane_grid = handle.material ? handle.material->GetDefaultVIL() : nullptr;
        if (!mtl_plane_grid || !vil_plane_grid)
            return false;

        MaterialBindingInit init;
        init.material = mtl_plane_grid;
        init.idd_handle = handle.idd_handle;
        init.vil = vil_plane_grid;
        init.preset = kPlaneGridCfg.pipeline;
        init.material_preset = kPlaneGridCfg.preset;
        init.instance_data = &white_color;
        init.instance_data_size = sizeof(white_color);

        handle_plane_grid = registry->AllocateHandle(init);
        if(handle_plane_grid == InvalidMaterialInstanceHandle)
            return false;

        if (!registry->BuildSlot(handle_plane_grid, slot_plane_grid))
            return false;

        std::cout << "[BillboardECS] PlaneGrid material: " << (void*)mtl_plane_grid << std::endl;

        std::cout << "[BillboardECS] PlaneGrid slot mi_id: " << slot_plane_grid.mi_id << std::endl;

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

        auto *registry = GetMaterialAssetRegistry();
        if (!registry)
            return false;

        const MaterialDomainHandle handle = registry->Acquire(kBillboardCfg);
        if (!handle.IsValid())
            return false;

        mtl_billboard = handle.material;
        vil_billboard = registry->ResolveVIL(handle.material, kBillboardCfg, nullptr);
        if (!vil_billboard)
            vil_billboard = handle.material ? handle.material->GetDefaultVIL() : nullptr;
        if (!mtl_billboard || !vil_billboard)
            return false;

        MaterialBindingInit init;
        init.material = mtl_billboard;
        init.idd_handle = handle.idd_handle;
        init.vil = vil_billboard;
        init.preset = kBillboardCfg.pipeline;
        init.material_preset = kBillboardCfg.preset;

        handle_billboard = registry->AllocateHandle(init);
        if(handle_billboard == InvalidMaterialInstanceHandle)
            return false;

        if (!registry->BuildSlot(handle_billboard, slot_billboard))
            return false;

        std::cout << "[BillboardECS] Billboard slot mi_id: " << slot_billboard.mi_id
                  << ", MaterialTemplate: " << (void*)mtl_billboard << std::endl;

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
        auto sampler_raw = sampler.lock().get();

        std::cout << "[BillboardECS] Sampler created: " << (void*)sampler_raw << std::endl;

        const bool bind_ok = mtl_billboard->BindTextureSampler(mtl::SamplerSlot::BaseColor,
                                              texture,
                                              sampler_raw);
        std::cout << "[BillboardECS] BindTextureSampler(BaseColor): " << (bind_ok ? "OK" : "FAILED")
                  << std::endl;
        if(!bind_ok)
            return false;

        math::Vector2u texture_size(texture->GetWidth(), texture->GetHeight());
        auto *registry = GetMaterialAssetRegistry();
        if (!registry)
            return false;

        if (!registry->WriteMIData(handle_billboard, &texture_size, sizeof(texture_size)))
            return false;

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
            auto pc = geometry_factory.CreateCreater(vil_plane_grid);
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
            prim_plane_grid = geometry_factory.CreatePrimitive(geom_plane_grid, slot_plane_grid);
            if(!prim_plane_grid)
                return false;

            std::cout << "[BillboardECS] PlaneGrid geometry: " << (void*)geom_plane_grid
                      << ", primitive: " << (void*)prim_plane_grid << std::endl;
        }

        {
            auto pc = geometry_factory.CreateCreater(vil_billboard);
            if (!pc)
                return false;

            pc->Init("Billboard", 4, 6, IndexType::U16);

            if(!pc->WriteVAB(VAN::Position, VF_V3F, billboard_position_data))
                return false;

            if(!pc->WriteIBO(billboard_index_data))
                return false;

            prim_billboard = geometry_factory.CreatePrimitive(pc.get(), slot_billboard);
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



