// ExtrudedPolygonTest.cpp
// 测试2D多边形挤压为3D多边形功能

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/Extruded.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/DescriptorBindingSet.h>
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

namespace
{
    GeometryVertexFormat CreateGizmo3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf;
        gvf.Add(VertexSemantic::Position, VF_V3F, 3, sizeof(float) * 3);
        gvf.Add(VertexSemantic::Normal, VF_V3F, 3, sizeof(float) * 3);
        return gvf;
    }
}

class ExtrudedPolygonTestApp : public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    Material *          material            = nullptr;
    DescriptorBindingSet *dbs              = nullptr;
    graph::DeviceBuffer *mi_ssbo           = nullptr;

    Geometry *         prim_rect_cube      = nullptr;
    Geometry *         prim_circle_cylinder = nullptr;
    Geometry *         prim_triangle       = nullptr;
    Geometry *         prim_pentagon       = nullptr;

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
        auto* buffer_manager   = graphics_context->GetBufferManager();
        auto* domain_manager   = graphics_context->GetResourceDomainManager();
        if (!material_manager || !buffer_manager || !domain_manager)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);
        material = material_manager->CreateMaterial(mtl::MaterialPreset::Gizmo3D, &cfg);
        if (!material)
            return false;

        const Color4f color = GetColor4f(COLOR::BlenderAxisRed, 1.0f);

        const uint32_t stride = material->GetMIDataBytes();
        if (stride > 0)
        {
            mi_ssbo = buffer_manager->CreateSSBO("ExtrudedPolygonTest:MIData", stride, nullptr, SharingMode::Exclusive);
            if (!mi_ssbo)
                return false;

            if (auto *gpu = mi_ssbo->GetGPUBuffer())
                gpu->Write(&color, 0, hgl_min(stride, static_cast<uint32_t>(sizeof(color))));

            for (const auto &req : material->GetBindingContract().requirements)
            {
                if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                    continue;

                const graph::mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
                if (!domain_manager->RegisterBuffer(addr, mi_ssbo, 1))
                    return false;

                dbs = new DescriptorBindingSet(material);
                dbs->SetSSBOBinding(req.ssbo_type, req.ssbo_id, 0);
            }
        }

        if (!dbs)
            dbs = new DescriptorBindingSet(material);

        return true;
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

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateGizmo3DGeometryVertexFormat());

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

    bool CreateMeshEntity(const char *name, Geometry *geometry, const glm::vec3 &pos)
    {
        if(!ecs_context || !geometry || !dbs)
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

        Primitive *mesh = primitive_manager->CreatePrimitive(geometry, material, dbs, nullptr);
        if(!mesh)
            return false;

        auto entity = ecs_context->CreateEntity<hgl::ecs::Entity>(name);
        auto transform = entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        transform->SetLocalPosition(pos);
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitive(mesh);
        prim_comp->RequestPipeline(InlinePipeline::Solid3D);
        prim_comp->SetVisible(true);

        return true;
    }

    bool InitECSScene()
    {
        if(!ecs_context)
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
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
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
    ~ExtrudedPolygonTestApp()
    {
        SAFE_CLEAR(prim_rect_cube);
        SAFE_CLEAR(prim_circle_cylinder);
        SAFE_CLEAR(prim_triangle);
        SAFE_CLEAR(prim_pentagon);
        delete dbs;  dbs = nullptr;
        SAFE_CLEAR(mi_ssbo)
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

int os_main(int argc, os_char **argv)
{
    return RunFramework<ExtrudedPolygonTestApp>(OS_TEXT("Extruded Polygon"),argc,argv,1280,720);
}
