// PlaneGrid3D

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/color/Color.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<cstring>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    constexpr uint32_t kPlaneGrid3DSsboId = hgl::graph::mtl::MakeRecipeSSBOId(7001);
}

class TestApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;
    MaterialProgram *    material            =nullptr;
    graph::DeviceBuffer *mi_ssbo            =nullptr;
    graph::mtl::SSBOType material_ssbo_type = graph::mtl::SSBOType::UserDefined;
    uint32_t             material_ssbo_id = 0;
    uint32_t             material_ssbo_count = 0;
    uint32_t             material_ssbo_stride = 0;

    Geometry *         geom_plane_grid     =nullptr;

private:

    bool InitMDP()
    {
        if (!geom_plane_grid)
            return false;

        auto* material_manager = GetManager<MaterialManager>();
        if (!material_manager)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);

        cfg.local_to_world=true;
        const GeometryVertexFormat &plane_grid_gvf = geom_plane_grid->GetGeometryVertexFormat();
        material = material_manager->AcquireMaterialProgram(mtl::MaterialPreset::VertexLuminance3D, &cfg, plane_grid_gvf);
        if(!material)return(false);

        return material;
    }

    bool CreateRenderObject()
    {
        auto* device = GetDevice();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !geometry_manager)
            return false;

        using namespace inline_geometry;

        struct PlaneGridCreateInfo pgci;

        pgci.grid_size.Set(32,32);
        pgci.sub_count.Set(8,8);

        pgci.lum=180;
        pgci.sub_lum=255;

        GeometryVertexFormat plane_grid_gvf{
            {VertexSemantic::Position,  VF_V2F},
            {VertexSemantic::Luminance, VF_V1UN8},
        };

        auto pc = std::make_unique<GeometryCreater>(
            device,
            plane_grid_gvf);

        geom_plane_grid=CreatePlaneGrid2D(pc.get(),&pgci);
        if (geom_plane_grid)
            geometry_manager->Add(geom_plane_grid);

        return geom_plane_grid;
    }

    bool Add(const char *name,const uint32_t struct_index,const glm::quat &rotation)
    {
        auto* primitive_manager = GetManager<PrimitiveManager>();
        if (!primitive_manager)
            return false;

        Primitive *ri=primitive_manager->CreatePrimitive(geom_plane_grid,material,nullptr,nullptr);

        if(!ri)
            return false;

        auto entity = ecs_context->CreateEntity<hgl::ecs::Entity>(name);
        auto transform = entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(rotation);
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitive(ri);
        graph::mtl::MaterialRecipe recipe{};
        recipe.recipe_name = "PlaneGrid3D.VertexLuminance3D";
        recipe.shading_model = graph::mtl::ShadingModel::Unlit;
        recipe.preset_hint = static_cast<uint32_t>(graph::mtl::MaterialPreset::VertexLuminance3D);
        recipe.domain = "PlaneGrid3D";
        prim_comp->SetMaterialRecipe(recipe);
        prim_comp->SetMaterialStructResource(graph::mtl::DataSlot::PBRSurface,
                                             material_ssbo_type,
                                             material_ssbo_id,
                                             mi_ssbo,
                                             material_ssbo_count,
                                             material_ssbo_stride,
                                             struct_index,
                                             true,
                                             true);
        prim_comp->RequestPipeline(InlinePipeline::Solid3D);
        prim_comp->SetVisible(true);

        return true;
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        if(!Add("PlaneXY", 0, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)))
            return false;

        const float rot90 = glm::radians(90.0f);
        if(!Add("PlaneYZ", 1, glm::angleAxis(rot90, glm::vec3(0.0f, 1.0f, 0.0f))))
            return false;
        if(!Add("PlaneXZ", 2, glm::angleAxis(rot90, glm::vec3(1.0f, 0.0f, 0.0f))))
            return false;

        return true;
    }

    bool InitMISSBO()
    {
        if (!ecs_context || !material)
            return false;

        const uint32_t mi_data_bytes = material->GetMIDataBytes();
        if (mi_data_bytes == 0)
            return true;
        if (mi_data_bytes != sizeof(Color4f))
            return false;

        auto *render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto *graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto *buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return false;

        const uint32_t mi_count = 3;
        const VkDeviceSize ssbo_size = static_cast<VkDeviceSize>(mi_count) * mi_data_bytes;
        material_ssbo_count = mi_count;
        material_ssbo_stride = mi_data_bytes;

        bool has_struct_binding = false;
        for (const auto &req : material->GetMaterialResourceLayout().requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                continue;

            has_struct_binding = true;
            material_ssbo_type = req.ssbo_type;
            material_ssbo_id = kPlaneGrid3DSsboId;
            break;
        }

        if (!has_struct_binding)
            return false;

        mi_ssbo = buffer_manager->CreateSSBO("PlaneGrid3D:MIData", ssbo_size, nullptr, SharingMode::Exclusive);
        if (!mi_ssbo)
            return false;

        auto *gpu_buf = mi_ssbo->GetGPUBuffer();
        if (!gpu_buf)
            return false;

        auto *dst = static_cast<uint8_t *>(gpu_buf->Map(0, ssbo_size));
        if (!dst)
            return false;

        memset(dst, 0, static_cast<size_t>(ssbo_size));

        Color4f grid_color = GetColor4f(COLOR::BlenderAxisRed,1.0f);
        for (uint32_t i = 0; i < 3; ++i)
        {
            memcpy(dst + static_cast<VkDeviceSize>(i) * mi_data_bytes, &grid_color, mi_data_bytes);

            grid_color = GetColor4f(COLOR(int(COLOR::BlenderAxisRed) + int(i) + 1), 1.0f);
        }
        gpu_buf->Unmap();

        return has_struct_binding;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<hgl::ecs::CameraComponent>();

        camera->control_mode = hgl::ecs::CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 48.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<hgl::graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();

        if(!ecs_context)
            return false;

        if(!InitMISSBO())
            return false;

        if(!InitScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:
    ~TestApp()
    {
        SAFE_CLEAR(geom_plane_grid);
        SAFE_CLEAR(mi_ssbo);
    }

    bool Init() override
    {
        if(!CreateRenderObject())
            return(false);

        if(!InitMDP())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp:public CameraAppFramework

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("PlaneGrid3D"),argc,argv,1280,720);
}
