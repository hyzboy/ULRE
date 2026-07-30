// PlaneGrid3D

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/mtl/MaterialLibrary.h>
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

class TestApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;
    graph::SSBOArrayAccessor<Color4f>* mi_ssbo_accessor = nullptr;

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
        named_struct.ssbo_id = mi_ssbo_accessor->GetSSBOId();
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
        plane_grid_recipe.mtl_def_id = "VertexLuminance3D";
        plane_grid_recipe.domain = "PlaneGrid3D";
        graph::mtl::UpsertRecipeSSBOAssetBinding(plane_grid_recipe, graph::mtl::SBS_MaterialInstance.name, mi_ssbo_accessor->GetSSBOBinding());
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

        auto *domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        mi_ssbo_accessor = domain_manager->AllocateArrayAccessor<Color4f>(
            graph::mtl::SSBOType::PBRSurface,
            "PlaneGrid3D:MIData",
            3);
        if (!mi_ssbo_accessor)
            return false;

        Color4f grid_color = GetColor4f(COLOR::BlenderAxisRed, 1.0f);
        for (uint32_t i = 0; i < 3; ++i)
        {
            (*mi_ssbo_accessor)[i] = grid_color;
            grid_color = GetColor4f(COLOR(int(COLOR::BlenderAxisRed) + int(i) + 1), 1.0f);
        }
        mi_ssbo_accessor->Commit();

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
        SAFE_CLEAR(mi_ssbo_accessor)
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
