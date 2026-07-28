// PlaneGrid3D

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/UBOCommon.h>
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
    graph::DeviceBuffer *mi_ssbo            =nullptr;
    graph::mtl::SSBOType material_ssbo_type = graph::mtl::SSBOType::PBRSurface;
    uint32_t             material_ssbo_id = kPlaneGrid3DSsboId;
    uint32_t             material_ssbo_count = 0;
    uint32_t             material_ssbo_stride = sizeof(Color4f);

    Geometry *         geom_plane_grid     =nullptr;
    graph::mtl::MaterialRecipe plane_grid_recipe{};
    PrimitiveAsset             plane_grid_asset{};

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
        auto entity = ecs_context->CreateEntity<hgl::ecs::Entity>(name);
        auto transform = entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(rotation);
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitiveAsset(&plane_grid_asset);
        hgl::ecs::PrimitiveComponent::MaterialStructNamedAuthoringResource named_struct{};
        named_struct.ssbo_name = graph::mtl::SBS_MaterialInstance.name;
        named_struct.ssbo_id = material_ssbo_id;
        named_struct.struct_index = struct_index;
        named_struct.use_struct_index = true;
        named_struct.shared_across_instances = true;
        prim_comp->SetMaterialStructResource(named_struct);
        prim_comp->RequestPipeline(InlinePipeline::Solid3D);
        prim_comp->SetVisible(true);

        return true;
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        plane_grid_recipe.recipe_name = "PlaneGrid3D.VertexLuminance3D";
        plane_grid_recipe.shading_model = graph::mtl::ShadingModel::Unlit;
        plane_grid_recipe.base_material_info_name = "VertexLuminance3D";
        plane_grid_recipe.domain = "PlaneGrid3D";
        plane_grid_recipe.ssbo_assets.push_back({graph::mtl::SBS_MaterialInstance.name, material_ssbo_type, material_ssbo_id});
        plane_grid_asset = PrimitiveAsset(geom_plane_grid, &plane_grid_recipe, PrimitiveType::Lines);

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
        if (!ecs_context)
            return false;

        const uint32_t mi_count = 3;
        const VkDeviceSize ssbo_size = static_cast<VkDeviceSize>(mi_count) * material_ssbo_stride;
        material_ssbo_count = mi_count;

        auto *domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        mi_ssbo = domain_manager->EnsureBuffer(graph::mtl::SSBOAddress{material_ssbo_type, material_ssbo_id, 0},
                                               "PlaneGrid3D:MIData",
                                               ssbo_size,
                                               mi_count,
                                               SharingMode::Exclusive);
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
            memcpy(dst + static_cast<VkDeviceSize>(i) * material_ssbo_stride, &grid_color, material_ssbo_stride);

            grid_color = GetColor4f(COLOR(int(COLOR::BlenderAxisRed) + int(i) + 1), 1.0f);
        }
        gpu_buf->Unmap();

        return true;
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
    }

    bool Init() override
    {
        if(!CreateRenderObject())
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
