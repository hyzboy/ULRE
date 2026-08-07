// ExtrudedPolygonTest.cpp
// 测试2D多边形挤压为3D多边形功能

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/Extruded.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/mtl/MaterialRecipe.h>
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
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }
}

class ExtrudedPolygonTestApp : public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    graph::mtl::MaterialRecipe mesh_recipe{};
    graph::SSBOArrayAccessor<Color4f>* mtl_data_ssbo_accessor = nullptr;

    Geometry *         prim_rect_cube      = nullptr;
    Geometry *         prim_circle_cylinder = nullptr;
    Geometry *         prim_triangle       = nullptr;
    Geometry *         prim_pentagon       = nullptr;
    PrimitiveAsset     asset_rect_cube{};
    PrimitiveAsset     asset_circle_cylinder{};
    PrimitiveAsset     asset_triangle{};
    PrimitiveAsset     asset_pentagon{};

private:

    bool InitMDP()
    {
        auto* domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        mtl_data_ssbo_accessor = domain_manager->AllocateArrayAccessor<Color4f>(
            graph::mtl::SSBOType::EmissiveSurface,
            "ExtrudedPolygonTest:MaterialData",
            1);
        if (!mtl_data_ssbo_accessor)
            return false;

        (*mtl_data_ssbo_accessor)[0] = GetColor4f(COLOR::BlenderAxisRed, 1.0f);
        mtl_data_ssbo_accessor->Commit();

        mesh_recipe.recipe_name = "ExtrudedPolygonTest.DebugNormalColor";
        mesh_recipe.mtl_def_id = "DebugNormalColor";
        mesh_recipe.pipeline_config = mtl::MakeSolid3DConfig();
        mesh_recipe.domain = "ExtrudedPolygonTest";
        graph::mtl::UpsertRecipeSSBOAssetBinding(mesh_recipe,
                                                 graph::mtl::DefaultMaterialDataSlotName,
                                                 mtl_data_ssbo_accessor->GetSSBOBinding());

        return true;
    }

    bool CreateRenderObjects()
    {
        auto* device = GetDevice();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !geometry_manager)
            return false;

        using namespace inline_geometry;

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateGizmo3DGeometryVertexFormat());

        // 测试1: 矩形挤压成立方体
        prim_rect_cube = CreateExtrudedRectangle(pc.get(), 2.0f, 1.5f, 1.0f, math::Vector3f(0, 0, 1));
        if (prim_rect_cube)
        {
            geometry_manager->Add(prim_rect_cube);
            asset_rect_cube = PrimitiveAsset(prim_rect_cube, &mesh_recipe, PrimitiveType::Triangles);
        }

        // 测试2: 圆形挤压成圆柱体
        prim_circle_cylinder = CreateExtrudedCircle(pc.get(), 0.8f, 1.5f, 16, math::Vector3f(0, 0, 1));
        if (prim_circle_cylinder)
        {
            geometry_manager->Add(prim_circle_cylinder);
            asset_circle_cylinder = PrimitiveAsset(prim_circle_cylinder, &mesh_recipe, PrimitiveType::Triangles);
        }

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
        {
            geometry_manager->Add(prim_triangle);
            asset_triangle = PrimitiveAsset(prim_triangle, &mesh_recipe, PrimitiveType::Triangles);
        }

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
        {
            geometry_manager->Add(prim_pentagon);
            asset_pentagon = PrimitiveAsset(prim_pentagon, &mesh_recipe, PrimitiveType::Triangles);
        }

        return prim_rect_cube && prim_circle_cylinder && prim_triangle && prim_pentagon;
    }

    bool CreateMeshEntity(const char *name, const PrimitiveAsset *mesh_asset, const glm::vec3 &pos)
    {
        if(!ecs_context || !mesh_asset)
            return false;

        auto entity = ecs_context->CreateEntity<hgl::ecs::Entity>(name);
        auto transform = entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        transform->SetLocalPosition(pos);
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitiveAsset(mesh_asset);
        hgl::ecs::PrimitiveComponent::MaterialDataSlotNamedAuthoringResource mesh_struct{};
        mesh_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
        mesh_struct.ssbo_id = mtl_data_ssbo_accessor->GetSSBOId();
        mesh_struct.data_index = 0;
        mesh_struct.use_data_index = true;
        mesh_struct.shared_across_instances = true;
        prim_comp->SetMaterialDataSlotResource(mesh_struct);
        prim_comp->SetVisible(true);

        return true;
    }

    bool InitECSScene()
    {
        if(!ecs_context)
            return false;

        if(!CreateMeshEntity("RectCube", &asset_rect_cube, glm::vec3(-3.0f, 0.0f, 0.0f)))
            return false;

        if(!CreateMeshEntity("CircleCylinder", &asset_circle_cylinder, glm::vec3(3.0f, 0.0f, 0.0f)))
            return false;

        if(!CreateMeshEntity("TrianglePrism", &asset_triangle, glm::vec3(0.0f, 3.0f, 0.0f)))
            return false;

        if(!CreateMeshEntity("PentagonPrism", &asset_pentagon, glm::vec3(0.0f, -3.0f, 0.0f)))
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
        SAFE_CLEAR(mtl_data_ssbo_accessor)
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
