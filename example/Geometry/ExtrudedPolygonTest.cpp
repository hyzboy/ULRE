// ExtrudedPolygonTest.cpp
// 测试2D多边形挤压为3D多边形功能

#include<hgl/WorkManager.h>
#include<hgl/graph/geo/Extruded.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/color/Color.h>
#include<cmath>
#include<memory>

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
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        auto* device = graphics_context->GetDevice();
        if (!device)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

        mtl::MaterialCreateInfo *mci=mtl::CreateGizmo3D(device->GetDevAttr(),&cfg);

        if(!mci)
            return(false);

        material=material_manager->CreateMaterial("Gizmo3D",mci);

        Color4f color=GetColor4f(COLOR::BlenderAxisRed);

        material_instance = material_manager->CreateMaterialInstance(material,(VIL *)nullptr,&color);

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material_instance, InlinePipeline::Solid3D) : nullptr;

        return pipeline != nullptr;
    }

    bool CreateRenderObjects()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* device = graphics_context->GetDevice();
        auto* geometry_manager = graphics_context->GetGeometryManager();
        if (!device || !geometry_manager)
            return false;

        using namespace inline_geometry;

        auto pc = std::make_unique<GeometryCreater>(device, material_instance->GetVIL());

        // 测试1: 矩形挤压成立方体
        prim_rect_cube = CreateExtrudedRectangle(pc.get(), 2.0f, 1.5f, 1.0f, math::Vector3f(0, 0, 1));
        if (prim_rect_cube)
            geometry_manager->Add(prim_rect_cube);

        // 测试2: 圆形挤压成圆柱体
        prim_circle_cylinder = CreateExtrudedCircle(pc.get(), 0.8f, 1.5f, 16, math::Vector3f(0, 0, 1));
        if (prim_circle_cylinder)
            geometry_manager->Add(prim_circle_cylinder);

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

        prim_triangle = CreateExtrudedPolygon(pc.get(), &triangleEpci);
        if (prim_triangle)
            geometry_manager->Add(prim_triangle);

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

        prim_pentagon = CreateExtrudedPolygon(pc.get(), &pentagonEpci);
        if (prim_pentagon)
            geometry_manager->Add(prim_pentagon);

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

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        Primitive *mesh = primitive_manager->CreatePrimitive(geometry, material_instance, pipeline);
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

