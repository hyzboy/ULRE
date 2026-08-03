/*
 SunDirectionControlSystem 使用示例

 展示如何使用 SunDirectionControlSystem 控制太阳方向
 通过旋转 Gizmo 实时调整天空光照的太阳方向
*/

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/graph/module/GeometryManager.h>
#include<memory>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>

#include<hgl/graph/gizmo/SunDirectionControlSystem.h>

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

class TestApp:public WorkObject
{
private:
    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *sky_entity = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    std::shared_ptr<hgl::ecs::EnvironmentSystem> environment_system;
    std::shared_ptr<hgl::graph::SunDirectionControlSystem> sun_gizmo_system;

    Geometry *prim_sky_sphere = nullptr;
    graph::mtl::MaterialRecipe sky_recipe{};
    PrimitiveAsset             sky_asset{};

private:
    bool InitRecipe()
    {
        if (!prim_sky_sphere)
            return false;
        sky_recipe.recipe_name = "AtmosphereSkySunGizmo.Sky";
        sky_recipe.mtl_def_id = "SkyMinimal";
        sky_recipe.domain = "AtmosphereSkySunGizmo";
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

        HexSphereCreateInfo hsci;
        hsci.subdivisions = 3;
        hsci.radius = 256;

        prim_sky_sphere = CreateHexSphere(pc.get(), &hsci);
        if (prim_sky_sphere)
            geometry_manager->Add(prim_sky_sphere);

        return prim_sky_sphere != nullptr;
    }

    bool InitECSScene()
    {
        if(!ecs_context || !prim_sky_sphere)
            return false;

        sky_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("SkySphere");
        auto transform = sky_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        auto prim_comp = sky_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitiveAsset(&sky_asset);
        prim_comp->SetVisible(true);

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
        camera->distance = 64.0f;
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

        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        environment_system = ecs_context->GetSystem<hgl::ecs::EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<hgl::ecs::EnvironmentSystem>();

        if (environment_system)
        {
            if (auto *sky = environment_system->EditSkyInfo())
            {
                sky->sun_direction = math::Vector4f(0.2f, 0.7f, 0.68f, 0.0f);
            }
            environment_system->MarkSkyDirty();
            environment_system->SyncSkyUBO();
        }

        sun_gizmo_system = ecs_context->GetSystem<hgl::graph::SunDirectionControlSystem>();
        if (!sun_gizmo_system)
            sun_gizmo_system = ecs_context->RegisterTickSystem<hgl::graph::SunDirectionControlSystem>();

        if (!sun_gizmo_system)
            return false;

        sun_gizmo_system->SetEnvironmentSystem(environment_system.get());
        sun_gizmo_system->SetGizmoPosition(math::Vector3f(0.0f, 0.0f, 0.0f));

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
            return false;

        if(!InitRecipe())
            return false;

        if(!InitECS())
            return false;

        return true;
    }
};

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("AtmosphereSkySunGizmo"),argc,argv,1280,720);
}
