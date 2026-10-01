#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/graph/module/GeometryManager.h>
#include<memory>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    GeometryVertexFormat CreateSkyMinimalGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
        };
        return gvf;
    }
}

class AtmosphereSkyMinimalApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *sky_entity = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    Geometry *          prim_sky_sphere     =nullptr;
    graph::mtl::MaterialRecipe sky_recipe{};
    PrimitiveAsset             sky_asset{};

private:

    bool InitRecipe()
    {
        if (!prim_sky_sphere)
            return false;
        sky_recipe.recipe_name = "AtmosphereSkyMinimal.Sky";
        sky_recipe.mtl_def_id = "SkyMinimal";
        sky_recipe.render_state_overrides.pipeline_config = mtl::MakeSkyConfig();
        sky_asset = PrimitiveAsset(prim_sky_sphere, &sky_recipe, PrimitiveType::Triangles);
        return true;
    }

    bool CreateRenderObject()
    {
        auto* device = GetDevice();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !geometry_manager)
            return false;

        using namespace inline_geometry;

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateSkyMinimalGeometryVertexFormat());

        struct HexSphereCreateInfo hsci;

        hsci.subdivisions=3;
        hsci.radius=256;

        prim_sky_sphere=CreateHexSphere(pc.get(),&hsci);
        if (prim_sky_sphere)
            geometry_manager->Add(prim_sky_sphere);

        return prim_sky_sphere;
    }

    bool InitECSScene()
    {
        if(!ecs_context)
            return false;

        if(!prim_sky_sphere)
            return false;

        sky_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("SkySphere");
        auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(sky_entity->GetEntityID(), hgl::ecs::Mobility::Movable));
        auto prim_comp = sky_entity->GetContext()->GetOrCreateGeometryData(sky_entity);

        transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform.SetMobility(hgl::ecs::Mobility::Static);

        prim_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(prim_comp->GetOwnerID())->SetPrimitiveAsset(&sky_asset);
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = hgl::ecs::CameraControlMode::ViewModel;
        camera->target = math::Vector3f(10,10,10);
        camera->distance = 1.0f;
        camera->yaw = 0.0f;
        camera->pitch = 0.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if(!ecs_context)
            return false;

        auto environment_system = ecs_context->GetSystem<hgl::ecs::EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<hgl::ecs::EnvironmentSystem>();

        if (environment_system)
        {
            environment_system->EditSkyInfo();
                    }

        if(!InitECSScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:
    bool Init() override
    {
        if(!CreateRenderObject())
            return(false);

        if(!InitRecipe())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class AtmosphereSkyMinimalApp:public CameraAppFramework

int os_main(int argc,os_char **argv)
{
    return RunFramework<AtmosphereSkyMinimalApp>(OS_TEXT("SimplestAtmosphere"),argc,argv,1280,720);
}
