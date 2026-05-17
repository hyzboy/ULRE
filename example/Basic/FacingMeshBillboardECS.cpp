// Facing Mesh Billboard ECS Example
//
// This example creates a batch of real 3D meshes and makes them always face the camera.
// Similar to billboard behavior, but with full 3D geometry instead of quad sprites.
// Coordinate convention: Z axis is up.
//
// Differences from RenderBoundBox:
// - No bounding box wireframe rendering
// - Every mesh entity has FacingTransformComponent (LookAtCamera)
// - FacingTransformSystem updates orientation each frame

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>

#include<hgl/color/Color.h>

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
using namespace hgl::ecs;

constexpr const COLOR DemoColors[]=
{
    COLOR::BlenderAxisRed,
    COLOR::BlenderAxisGreen,
    COLOR::BlenderAxisBlue,
    COLOR::BananaYellow,
    COLOR::CherryBlossomPink,
    COLOR::SkyBlue,
    COLOR::Lavender,
    COLOR::Coral,
};

constexpr const size_t DEMO_COLOR_COUNT = sizeof(DemoColors) / sizeof(COLOR);

class FacingMeshBillboardECSApp : public WorkObject
{
private:

    struct RenderMesh
    {
        Geometry* geometry = nullptr;
        int color_index = 0;

        Entity* entity = nullptr;
        std::shared_ptr<TransformComponent> transform;
        std::shared_ptr<PrimitiveComponent> primitive_comp;

        ~RenderMesh()
        {
        }
    };

    ECSContext* ecs_context = nullptr;

    Color4f entity_colors[DEMO_COLOR_COUNT];
    VertexDataManager* mesh_vdm = nullptr;

    std::vector<std::unique_ptr<RenderMesh>> render_meshes;

    Entity* camera_entity = nullptr;

    inline static const mtl::MaterialRecipe kSolidCfg {
        .id            = "facing_billboard_solid",
        .preset        = mtl::MaterialPreset::Gizmo3D,
        .vertex_policy = mtl::VertexTransformPolicy::BillboardCameraFacing,
        .pipeline      = GraphicsPipelinePreset::Solid3D,
    };

private:

    bool InitMaterial()
    {
        for (size_t i = 0; i < DEMO_COLOR_COUNT; ++i)
            entity_colors[i] = GetColor4f(DemoColors[i], 1.0f);

        return true;
    }

    bool InitVDM()
    {

        auto* buffer_manager = GetBufferManager();
        if (!buffer_manager)
            return false;

        GeometryVertexFormat gvf;
        gvf.Set(VAN::Position, VF_V3F);
        gvf.Set(VAN::Normal,   VF_V3F);
        mesh_vdm = new VertexDataManager(buffer_manager, gvf);
        if (!mesh_vdm)
            return false;

        return mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U16);
    }

    RenderMesh* CreateRenderMesh(Geometry* geometry, int color_index)
    {
        if (!geometry)
            return nullptr;

        auto mesh = std::make_unique<RenderMesh>();
        mesh->geometry = geometry;
        mesh->color_index = color_index % DEMO_COLOR_COUNT;

        RenderMesh* result = mesh.get();
        render_meshes.push_back(std::move(mesh));
        return result;
    }

    bool CreateGeometryMeshes()
    {
        using namespace inline_geometry;

        auto create_geometry = [this](auto&& creator) -> Geometry*
        {
            auto pc = std::make_unique<GeometryCreater>(mesh_vdm);
            if (!pc)
                return nullptr;

            return creator(pc.get());
        };

        if (!CreateRenderMesh(create_geometry([](GeometryCreater* pc) { return CreateSphere(pc, 32); }), 0))
            return false;

        if (!CreateRenderMesh(create_geometry([](GeometryCreater* pc)
                                              {
                                                  CubeCreateInfo cci;
                                                  cci.segments_x = 1;
                                                  cci.segments_y = 1;
                                                  cci.segments_z = 1;
                                                  return CreateCube(pc, &cci);
                                              }), 1))
            return false;

        if (!CreateRenderMesh(create_geometry([](GeometryCreater* pc)
                                              {
                                                  ConeCreateInfo cci;
                                                  cci.radius = 0.9f;
                                                  cci.halfExtend = 1.0f;
                                                  cci.numberSlices = 24;
                                                  cci.numberStacks = 3;
                                                  return CreateCone(pc, &cci);
                                              }), 2))
            return false;

        if (!CreateRenderMesh(create_geometry([](GeometryCreater* pc)
                                              {
                                                  CylinderCreateInfo cci;
                                                  cci.halfExtend = 1.0f;
                                                  cci.numberSlices = 24;
                                                  cci.radius = 0.8f;
                                                  return CreateCylinder(pc, &cci);
                                              }), 3))
            return false;

        if (!CreateRenderMesh(create_geometry([](GeometryCreater* pc)
                                              {
                                                  TorusCreateInfo tci;
                                                  tci.innerRadius = 0.6f;
                                                  tci.outerRadius = 1.0f;
                                                  tci.numberSlices = 48;
                                                  tci.numberStacks = 12;
                                                  return CreateTorus(pc, &tci);
                                              }), 4))
            return false;

        if (!CreateRenderMesh(create_geometry([](GeometryCreater* pc)
                                              {
                                                  CapsuleCreateInfo cci;
                                                  return CreateCapsule(pc, &cci);
                                              }), 5))
            return false;

        if (!CreateRenderMesh(create_geometry([](GeometryCreater* pc)
                                              {
                                                  FrustumCreateInfo fci;
                                                  fci.bottom_radius = 0.9f;
                                                  fci.top_radius = 0.35f;
                                                  fci.height = 2.0f;
                                                  fci.numberSlices = 24;
                                                  return CreateFrustum(pc, &fci);
                                              }), 6))
            return false;

        if (!CreateRenderMesh(create_geometry([](GeometryCreater* pc)
                                              {
                                                  ArrowCreateInfo aci;
                                                  aci.shaft_radius = 0.08f;
                                                  aci.shaft_length = 1.8f;
                                                  aci.head_radius = 0.25f;
                                                  aci.head_length = 0.5f;
                                                  aci.numberSlices = 16;
                                                  aci.cross_section = ArrowCrossSection::Circular;
                                                  return CreateArrow(pc, &aci);
                                              }), 7))
            return false;

        return true;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context || render_meshes.empty())
            return false;

        const size_t mesh_count = render_meshes.size();

        for (size_t i = 0; i < mesh_count; ++i)
        {
            auto* rm = render_meshes[i].get();
            if (!rm)
                continue;

            rm->entity = ecs_context->CreateEntity<Entity>("FacingMesh_" + std::to_string(i));
            rm->transform = rm->entity->AddComponent<TransformComponent>(Mobility::Static);
            rm->primitive_comp = rm->entity->AddComponent<PrimitiveComponent>();

            float angle = glm::radians(360.0f * static_cast<float>(i) / static_cast<float>(mesh_count));
            glm::vec3 pos = glm::vec3(std::cos(angle) * 8.0f,
                                      std::sin(angle) * 8.0f,
                                      1.5f);

            rm->transform->SetLocalPosition(pos);
            rm->transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            rm->transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            rm->transform->SetMovable(true);

            rm->primitive_comp->SetUnresolvedGeometry(rm->geometry);
            rm->primitive_comp->SetMaterialRecipe(RegisterMaterialRecipe(kSolidCfg), &entity_colors[rm->color_index], sizeof(Color4f));
            rm->primitive_comp->SetVisible(true);
        }

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        if (!CreateGeometryMeshes())
            return false;

        if (!InitSceneEntities())
            return false;

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 16.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:

    ~FacingMeshBillboardECSApp()
    {
        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if (!InitMaterial())
            return false;

        if (!InitVDM())
            return false;

        if (!InitECS())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<FacingMeshBillboardECSApp>(OS_TEXT("Facing Mesh Billboard ECS"), argc, argv, 1280, 720);
}

