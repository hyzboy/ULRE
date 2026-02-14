// 该范例主要演示使用ECS架构绘制一个立方体，并通过ECS CameraSystem使用ViewModel模式
// This example demonstrates rendering a cube with ECS and driving the camera via ViewModel mode
//
// 本范例展示了：
// 1. 使用ECS架构创建立方体实体
// 2. 使用TransformComponent管理空间变换
// 3. 使用PrimitiveComponent管理渲染图元
// 4. CameraSystem配置为ViewModel控制模式

#include<hgl/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/color/Color.h>

// 引入ECS相关头文件
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

class TestApp:public WorkObject
{
private:

    ECSContext *  ecs_world      =nullptr;
    Entity *      cube_entity    =nullptr;
    Entity *      camera_entity  =nullptr;

    Material *          material        = nullptr;
    MaterialInstance *  mi              = nullptr;
    Pipeline *          pipeline        = nullptr;

    Geometry *          geometry        = nullptr;
    Primitive *         primitive       = nullptr;

private:

    bool InitMaterial()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

        mtl::MaterialCreateInfo *mci = mtl::CreateGizmo3D(GetDevAttr(), &cfg);

        if(!mci)
            return false;

        material = CreateMaterial("Gizmo3D", mci);

        if(!material)
            return false;

        Color4f color = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);

        mi = CreateMaterialInstance(material, (VIL *)nullptr, &color);

        if(!mi)
            return false;

        pipeline = CreatePipeline(material, InlinePipeline::Solid3D);

        return pipeline != nullptr;
    }

    bool CreateCubeGeometry()
    {
        using namespace inline_geometry;

        auto pc = GetGeometryCreater(material);

        if(!pc)
            return false;

        CubeCreateInfo cci;
        cci.segments_x = 2;
        cci.segments_y = 3;
        cci.segments_z = 4;

        geometry = CreateCube(pc, &cci);

        if(!geometry)
            return false;

        Add(geometry);
        return true;
    }

    bool InitPrimitive()
    {
        primitive = CreatePrimitive(geometry, mi, pipeline);
        return primitive != nullptr;
    }

    bool EnsureCameraSystem()
    {
        if(!ecs_world)
            return false;

        auto camera_system = ecs_world->GetSystem<CameraSystem>();
        if(!camera_system)
        {
            camera_system = ecs_world->RegisterTickSystem<CameraSystem>(ecs_world);
            if(ecs_world->IsActive())
            {
                camera_system->OnDependenciesReady();
                camera_system->Initialize();
            }
        }

        return camera_system != nullptr;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        cube_entity = ecs_world->CreateEntity<Entity>("CubeEntity");

        auto transform = cube_entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        auto primitive_comp = cube_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        primitive_comp->SetPrimitive(primitive);
        primitive_comp->SetVisible(true);

        return true;
    }

    bool InitCamera()
    {
        if(!EnsureCameraSystem())
            return false;

        camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 6.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(primitive)
        SAFE_CLEAR(geometry)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!InitMaterial())
            return false;

        if(!CreateCubeGeometry())
            return false;

        if(!InitPrimitive())
            return false;

        if(!InitECS())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
    }
};

int os_main(int, os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Simple Cube (ECS)"), 1280, 720);
}

