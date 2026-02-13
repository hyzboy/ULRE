#include<hgl/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/mtl/UBOCommon.h>

// ECS headers
#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/CameraComponent.h>
#include<hgl/ecs/CameraSystem.h>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_world = nullptr;
    hgl::ecs::Entity *sky_entity = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    Material *          mtl_sky_sphere      =nullptr;
    Pipeline *          mtl_pipeline        =nullptr;

    Geometry *          prim_sky_sphere     =nullptr;
    MaterialInstance *  mi_sky_sphere       =nullptr;

private:

    bool InitMDP()
    {
        mtl::SkyMinimalCreateConfig cfg;

        mi_sky_sphere=CreateMaterialInstance(mtl::inline_material::SkyMinimal,&cfg);

        mtl_pipeline=CreatePipeline(mi_sky_sphere,InlinePipeline::Sky);

        return mtl_pipeline;
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        auto pc=GetGeometryCreater(mi_sky_sphere);

        struct HexSphereCreateInfo hsci;

        hsci.subdivisions=3;
        hsci.radius=256;

        prim_sky_sphere=CreateHexSphere(pc,&hsci);

        return prim_sky_sphere;
    }

    bool EnsureCameraSystem()
    {
        if(!ecs_world)
            return false;

        auto camera_system = ecs_world->GetSystem<hgl::ecs::CameraSystem>();
        if(!camera_system)
        {
            camera_system = ecs_world->RegisterTickSystem<hgl::ecs::CameraSystem>(ecs_world);
            if(ecs_world->IsActive())
            {
                camera_system->OnDependenciesReady();
                camera_system->Initialize();
            }
        }

        return camera_system != nullptr;
    }

    bool InitECSScene()
    {
        if(!ecs_world)
            return false;

        if(!prim_sky_sphere || !mi_sky_sphere || !mtl_pipeline)
            return false;

        Primitive *ri=CreatePrimitive(prim_sky_sphere,mi_sky_sphere,mtl_pipeline);
        if(!ri)
            return false;

        sky_entity = ecs_world->CreateEntity<hgl::ecs::Entity>("SkySphere");
        auto transform = sky_entity->AddComponent<hgl::ecs::TransformComponent>();
        auto prim_comp = sky_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitive(ri);
        prim_comp->SetVisible(true);

        return true;
    }

    bool InitCamera()
    {
        if(!EnsureCameraSystem())
            return false;

        camera_entity = ecs_world->CreateEntity<hgl::ecs::Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<hgl::ecs::CameraComponent>();

        camera->control_mode = hgl::ecs::CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(10,10,10);
        camera->distance = 1.0f;
        camera->yaw = 0.0f;
        camera->pitch = 0.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<hgl::graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        if(!InitECSScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        if(!InitMDP())
            return(false);

        if(!CreateRenderObject())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp:public CameraAppFramework

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("SimplestAtmosphere"),1280,720);
}
