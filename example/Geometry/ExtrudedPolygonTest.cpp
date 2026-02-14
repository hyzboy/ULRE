// ExtrudedPolygonTest.cpp
// 测试2D多边形挤压为3D多边形功能

#include<hgl/WorkManager.h>
#include<hgl/graph/geo/Extruded.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/color/Color.h>
#include<cmath>

// ECS headers
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

class ExtrudedPolygonTestApp : public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_world = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    Material *          material            = nullptr;
    Pipeline *          pipeline            = nullptr;

    Geometry *         prim_rect_cube      = nullptr;
    Geometry *         prim_circle_cylinder = nullptr;
    Geometry *         prim_triangle       = nullptr;
    Geometry *         prim_pentagon       = nullptr;
    MaterialInstance *  material_instance   = nullptr;

private:

    bool InitMDP()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

        mtl::MaterialCreateInfo *mci=mtl::CreateGizmo3D(GetDevAttr(),&cfg);

        if(!mci)
            return(false);

        material=CreateMaterial("Gizmo3D",mci);

        Color4f color=GetColor4f(COLOR::BlenderAxisRed);

        material_instance = CreateMaterialInstance(material,(VIL *)nullptr,&color);

        pipeline = CreatePipeline(material_instance, InlinePipeline::Solid3D);

        return pipeline != nullptr;
    }

    bool CreateRenderObjects()
    {
        using namespace inline_geometry;

        auto pc = GetGeometryCreater(material_instance);

        // 测试1: 矩形挤压成立方体
        prim_rect_cube = CreateExtrudedRectangle(pc, 2.0f, 1.5f, 1.0f, math::Vector3f(0, 0, 1));

        // 测试2: 圆形挤压成圆柱体
        prim_circle_cylinder = CreateExtrudedCircle(pc, 0.8f, 1.5f, 16, math::Vector3f(0, 0, 1));

        // 测试3: 三角形挤压
        math::Vector2f triangleVertices[3] =
        {
            {-0.8f, -0.5f},  // 左下
            { 0.8f, -0.5f},  // 右下
            { 0.0f,  0.8f}   // 顶部
        };

        ExtrudedPolygonCreateInfo triangleEpci;
        triangleEpci.vertices = triangleVertices;
        triangleEpci.vertexCount = 3;
        triangleEpci.extrudeDistance = 1.2f;
        triangleEpci.extrudeAxis = math::Vector3f(0, 0, 1);
        triangleEpci.generateCaps = true;
        triangleEpci.generateSides = true;
        triangleEpci.clockwiseFront = true;

        prim_triangle = CreateExtrudedPolygon(pc, &triangleEpci);

        // 测试4: 五边形挤压
        math::Vector2f pentagonVertices[5];
        float angleStep = 2.0f * std::numbers::pi_v<float> / 5.0f;

        for (int i = 0; i < 5; i++)
        {
            float angle = i * angleStep;
            pentagonVertices[i].x = cos(angle) * 0.7f;
            pentagonVertices[i].y = sin(angle) * 0.7f;
        }

        ExtrudedPolygonCreateInfo pentagonEpci;
        pentagonEpci.vertices = pentagonVertices;
        pentagonEpci.vertexCount = 5;
        pentagonEpci.extrudeDistance = 1.0f;
        pentagonEpci.extrudeAxis = math::Vector3f(1, 0, 0);  // X轴方向挤压
        pentagonEpci.generateCaps = true;
        pentagonEpci.generateSides = true;
        pentagonEpci.clockwiseFront = true;

        prim_pentagon = CreateExtrudedPolygon(pc, &pentagonEpci);

        return prim_rect_cube && prim_circle_cylinder && prim_triangle && prim_pentagon;
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

    bool CreateMeshEntity(const char *name, Geometry *geometry, const glm::vec3 &pos)
    {
        if(!ecs_world || !geometry || !material_instance || !pipeline)
            return false;

        Primitive *mesh = CreatePrimitive(geometry, material_instance, pipeline);
        if(!mesh)
            return false;

        auto entity = ecs_world->CreateEntity<hgl::ecs::Entity>(name);
        auto transform = entity->AddComponent<hgl::ecs::TransformComponent>();
        auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        transform->SetLocalPosition(pos);
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitive(mesh);
        prim_comp->SetVisible(true);

        return true;
    }

    bool InitECSScene()
    {
        if(!ecs_world)
            return false;

        if(!CreateMeshEntity("RectCube", prim_rect_cube, glm::vec3(-3.0f, 0.0f, 0.0f)))
            return false;

        if(!CreateMeshEntity("CircleCylinder", prim_circle_cylinder, glm::vec3(3.0f, 0.0f, 0.0f)))
            return false;

        if(!CreateMeshEntity("TrianglePrism", prim_triangle, glm::vec3(0.0f, 3.0f, 0.0f)))
            return false;

        if(!CreateMeshEntity("PentagonPrism", prim_pentagon, glm::vec3(0.0f, -3.0f, 0.0f)))
            return false;

        return true;
    }

    bool InitCamera()
    {
        if(!EnsureCameraSystem())
            return false;

        camera_entity = ecs_world->CreateEntity<hgl::ecs::Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<hgl::ecs::CameraComponent>();

        camera->control_mode = hgl::ecs::CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 12.0f;
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

    ~ExtrudedPolygonTestApp()
    {
        SAFE_CLEAR(prim_rect_cube);
        SAFE_CLEAR(prim_circle_cylinder);
        SAFE_CLEAR(prim_triangle);
        SAFE_CLEAR(prim_pentagon);
    }

    bool Init() override
    {
        if (!InitMDP())
            return false;

        if (!CreateRenderObjects())
            return false;

        if (!InitECS())
            return false;

        return true;
    }
};

int os_main(int, os_char **)
{
    return RunFramework<ExtrudedPolygonTestApp>(OS_TEXT("Extruded Polygon"),1280,720);
}

