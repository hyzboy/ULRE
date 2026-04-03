// Recursive Cube (ECS)
//
// This example places a base cube at origin and spawns 6 independent chains.
// Each chain moves outward along a face normal by the parent cube size,
// then places a child cube with 0.9x scale. Repeats for 10 layers.

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/color/Color.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include<memory>
#include<random>
#include<vector>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

class RecursiveCubeApp:public WorkObject
{
private:
    ECSContext *ecs_context = nullptr;
    Entity *camera_entity = nullptr;

    Material *material = nullptr;
    MaterialInstance *mi = nullptr;
    GraphicsPipeline *pipeline = nullptr;

    Geometry *geometry = nullptr;
    std::vector<Primitive *> primitives;
    struct CubeNode
    {
        TransformComponent *transform = nullptr;
        glm::vec3 axis{0.0f, 1.0f, 0.0f};
        float max_angle_deg = 0.0f;
        float angle_deg = 0.0f;
        float speed_deg = 0.0f;
        int dir = 1;
    };
    std::vector<CubeNode> nodes;

    PrimitiveManager *primitive_manager = nullptr;

    static constexpr int kMaxDepth = 20;
    static constexpr float kChildScale = 0.99f;
    static constexpr float kMinAngleDeg = 18.0f;
    static constexpr float kMaxAngleDeg = 55.0f;
    static constexpr float kMinSpeed = 2.0f;
    static constexpr float kMaxSpeed = 4.0f;
    static constexpr float kStepScale = 1.1f;

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> unit_dist{0.0f, 1.0f};

private:
    glm::vec3 RandomAxis()
    {
        glm::vec3 v(unit_dist(rng) * 2.0f - 1.0f, unit_dist(rng) * 2.0f - 1.0f, unit_dist(rng) * 2.0f - 1.0f);
        float len2 = glm::dot(v, v);
        if (len2 < 1e-6f)
            return glm::vec3(0.0f, 1.0f, 0.0f);
        return glm::normalize(v);
    }

    float RandomAngleDeg()
    {
        return kMinAngleDeg + (kMaxAngleDeg - kMinAngleDeg) * unit_dist(rng);
    }

    float RandomSpeed()
    {
        return kMinSpeed + (kMaxSpeed - kMinSpeed) * unit_dist(rng);
    }

    void RandomizeNode(CubeNode &node)
    {
        node.axis = RandomAxis();
        node.max_angle_deg = RandomAngleDeg();
        node.speed_deg = RandomSpeed();
        node.angle_deg = 0.0f;
        node.dir = 1;
    }

private:
    bool InitMaterial()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

        auto *render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto *graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto *material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        material = material_manager->CreateMaterial(mtl::MaterialPreset::Gizmo3D, &cfg);
        if (!material)
            return false;

        Color4f color = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);
        mi = material_manager->CreateMaterialInstance(material, (VIL *)nullptr, &color);
        if (!mi)
            return false;

        auto *render_target = render_context->GetCurrentRenderTarget();
        auto *render_pass = render_target ? render_target->GetRenderFormat() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material, PipelinePreset::Solid3D) : nullptr;

        return pipeline != nullptr;
    }

    bool CreateCubeGeometry()
    {
        using namespace inline_geometry;

        auto *render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto *graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto *geometry_manager = graphics_context->GetGeometryManager();
        if (!geometry_manager)
            return false;

        auto *device = graphics_context->GetDevice();
        if (!device)
            return false;

        auto pc = std::make_unique<GeometryCreater>(device, material->GetDefaultVIL());

        CubeCreateInfo cci;
        cci.segments_x = 1;
        cci.segments_y = 1;
        cci.segments_z = 1;

        geometry = CreateCube(pc.get(), &cci);
        if (!geometry)
            return false;

        geometry_manager->Add(geometry);
        return true;
    }

    Entity *CreateCubeEntity(const glm::vec3 &local_pos,
                             float scale,
                             const char *name,
                             bool animate,
                             EntityID parent_id)
    {
        if (!ecs_context || !primitive_manager || !geometry || !mi || !pipeline)
            return nullptr;

        auto *entity = ecs_context->CreateEntity<Entity>(name);
        auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);
        if (parent_id.IsValid())
            transform->SetParent(parent_id);
        transform->SetLocalPosition(local_pos);
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(scale, scale, scale));
        transform->SetMovable(animate);

        auto prim = primitive_manager->CreatePrimitive(geometry, mi, pipeline);
        if (!prim)
            return nullptr;

        primitives.push_back(prim);

        auto primitive_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        primitive_comp->SetPrimitive(prim);
        primitive_comp->SetVisible(true);

        if (animate)
        {
            CubeNode node;
            node.transform = transform.get();
            RandomizeNode(node);
            nodes.push_back(node);
        }

        return entity;
    }

    bool SpawnChain(const glm::vec3 &dir,
                    int depth,
                    Entity *parent,
                    float size,
                    const std::string &name_prefix)
    {
        if (depth <= 0)
            return true;

        float child_size = size * kChildScale;
        glm::vec3 local_pos = dir * (size * kStepScale);

        std::string name = name_prefix + "_" + std::to_string(depth);
        EntityID parent_id = parent ? parent->GetID() : EntityID();
        Entity *child = CreateCubeEntity(local_pos, child_size, name.c_str(), true, parent_id);
        if (!child)
            return false;

        return SpawnChain(dir, depth - 1, child, child_size, name_prefix);
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();

        if (!ecs_context)
            return false;

        auto *render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto *graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        // Base cube at origin
        Entity *root = CreateCubeEntity(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, "RootCube", false, EntityID());
        if (!root)
            return false;

        const glm::vec3 dirs[6] = {
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(-1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, -1.0f),
        };

        const char *names[6] = {"PosX", "NegX", "PosY", "NegY", "PosZ", "NegZ"};

        for (int i = 0; i < 6; ++i)
        {
            if (!SpawnChain(dirs[i], kMaxDepth, root, 1.0f, names[i]))
                return false;
        }

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
        camera->distance = 18.0f;
        camera->yaw = 45.0f;
        camera->pitch = -25.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~RecursiveCubeApp()
    {
        for (auto *prim : primitives)
            SAFE_CLEAR(prim)
        SAFE_CLEAR(geometry)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.1f, 0.1f, 0.1f, 1.0f));

        if (!InitMaterial())
            return false;

        if (!CreateCubeGeometry())
            return false;

        if (!InitECS())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        if(delta_time>10)        //第一次时间会超大
        {
            delta_time=1;
        }

        for (auto &node : nodes)
        {
            if (!node.transform)
                continue;

            node.angle_deg += node.dir * node.speed_deg * static_cast<float>(delta_time);
            if (node.angle_deg >= node.max_angle_deg)
            {
                node.angle_deg = node.max_angle_deg;
                node.dir = -1;
            }
            else if (node.angle_deg <= 0.0f)
            {
                node.angle_deg = 0.0f;
                RandomizeNode(node);
            }

            const float angle_rad = glm::radians(node.angle_deg);
            node.transform->SetLocalRotation(glm::angleAxis(angle_rad, node.axis));
        }

        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<RecursiveCubeApp>(OS_TEXT("Recursive Cube (ECS)"), argc, argv, 1280, 720);
}
