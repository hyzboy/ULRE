#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<memory>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/light_shadow/EnvironmentSystem.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include "../Shared/ExampleAppBase.h"

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

class AtmosphereSkyMinimalApp:public ulre_example::ExampleAppBase
{
public:

    AtmosphereSkyMinimalApp()
    {
        SetupExampleConfig({
            "SKYMIN",
            "skymindump",
            "SKYSELF",
            8,
            8,
            { 1280, 720, 0, 219, 0xC914C338EFC7790Eull }
        });
    }

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
        const mtl::MaterialRecipe *loaded_recipe =
            GetRecipe("rcp:pack(ShaderLibrary):asset/recipe/Examples/Environment/AtmosphereSkyMinimal.Sky");
        if (!loaded_recipe)
            return false;
        sky_recipe = *loaded_recipe;
        sky_asset = PrimitiveAsset(prim_sky_sphere, &sky_recipe, PrimitiveType::Triangles);
        return true;
    }

    bool CreateRenderObject()
    {
        auto* device = GetDevice();
        if (!device)
            return false;

        // subdivisions=3、radius=256 全在 res/geo/presets/hex_sphere_sky.geo.toml 里
        const GeometryVertexFormat geometry_vertex_format = CreateSkyMinimalGeometryVertexFormat();

        prim_sky_sphere=GetGeometry("geo:builtin:hex_sphere_sky", &geometry_vertex_format);
        return prim_sky_sphere;
    }

    bool InitECSScene()
    {
        if(!ecs_context)
            return false;

        if(!prim_sky_sphere)
            return false;

        sky_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("SkySphere");
        auto transform = ecs_context->Spatial().GetTransform(ecs_context->Spatial().CreateTransform(sky_entity->GetEntityID(), hgl::ecs::Mobility::Movable));
        auto prim_comp = sky_entity->GetContextAs<hgl::ecs::ECSContext>()->GetOrCreateGeometryData(sky_entity);

        transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform.SetMobility(hgl::ecs::Mobility::Static);

        prim_comp->GetOwner()->GetContextAs<hgl::ecs::ECSContext>()->GetOrCreateGeometryData(prim_comp->GetOwnerID())->SetPrimitiveAsset(&sky_asset);
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

        if(!InitECSScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:
    bool OnInit() override
    {
        if(!CreateRenderObject())
            return(false);

        if(!InitRecipe())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class AtmosphereSkyMinimalApp

int os_main(int argc,os_char **argv)
{
    return RunFramework<AtmosphereSkyMinimalApp>(OS_TEXT("SimplestAtmosphere"),argc,argv,1280,720);
}
