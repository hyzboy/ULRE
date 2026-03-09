// 该范例演示经典 PBR Metallic/Roughness 对比测试场景：10x10 共 100 个球体
// X 轴方向表示 Metallic（0→1），Y 轴方向表示 Roughness（0.05→1）
// This example renders a 10×10 grid of spheres for a classic PBR metallic/roughness comparison.
// X-axis: metallic (0→1), Y-axis: roughness (0.05→1)

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/color/ColorPacking.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

// 10×10 grid params
static constexpr uint GRID_SIZE       = 10;    // spheres per axis
static constexpr float SPHERE_SPACING = 2.5f;  // center-to-center distance

// Base albedo for all spheres (classic gray PBR test chart color)
static constexpr float BASE_COLOR_R = 1.72f;
static constexpr float BASE_COLOR_G = 1.72f;
static constexpr float BASE_COLOR_B = 1.72f;

class TestApp : public WorkObject
{
private:

    ECSContext *  ecs_world     = nullptr;
    Entity *      camera_entity = nullptr;

    Material *          material  = nullptr;
    Pipeline *          pipeline  = nullptr;

    Geometry *          sphere_geometry = nullptr;
    Primitive *         base_primitive  = nullptr;

    // One MI per sphere: row=roughness axis, col=metallic axis
    mtl::PBRColor3DMaterialInstance sphere_mi_data[GRID_SIZE][GRID_SIZE]{};
    MaterialInstance *               sphere_mi[GRID_SIZE][GRID_SIZE]{};

    // 100 entities, one per sphere
    Entity *sphere_entities[GRID_SIZE][GRID_SIZE]{};

private:

    bool InitMaterial()
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

        mtl::PBRColor3DMaterialCreateConfig cfg;
        material = material_manager->CreateMaterial(mtl::MaterialPreset::PBRColor3D, &cfg);
        if (!material)
            return false;

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass   = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material, InlinePipeline::Solid3D) : nullptr;

        return pipeline != nullptr;
    }

    bool CreatePBRMaterialInstances()
    {
        auto* render_context = GetRenderContext();
        if (!render_context) return false;
        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context) return false;
        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager) return false;

        for (uint row = 0; row < GRID_SIZE; ++row)
        {
            for (uint col = 0; col < GRID_SIZE; ++col)
            {
                // col  → metallic:  0.0 → 1.0
                // row  → roughness: 0.05 → 1.0  (avoid perfectly smooth mirrors at 0)
                float metallic  = float(col) / float(GRID_SIZE - 1);
                float roughness = 0.05f + float(row) / float(GRID_SIZE - 1) * 0.95f;

                auto &d    = sphere_mi_data[row][col];
                d.base_color = PackRGBA8Float(BASE_COLOR_R, BASE_COLOR_G, BASE_COLOR_B, 1.0f);
                d.metallic   = metallic;
                d.roughness  = roughness;

                sphere_mi[row][col] = material_manager->CreateMaterialInstance(
                    material, (VIL *)nullptr, &d);

                if (!sphere_mi[row][col])
                    return false;
            }
        }

        return true;
    }

    bool CreateSphereGeometry()
    {
        using namespace inline_geometry;

        auto* render_context = GetRenderContext();
        if (!render_context) return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context) return false;

        auto* device           = graphics_context->GetDevice();
        auto* geometry_manager = graphics_context->GetGeometryManager();
        if (!device || !geometry_manager) return false;

        auto pc = std::make_unique<GeometryCreater>(device, material->GetDefaultVIL());

        sphere_geometry = CreateSphere(pc.get(), 32);
        if (!sphere_geometry)
            return false;

        geometry_manager->Add(sphere_geometry);
        return true;
    }

    bool CreateBasePrimitive()
    {
        auto* render_context = GetRenderContext();
        if (!render_context) return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context) return false;

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager) return false;

        // Use sphere_mi[0][0] as default; per-entity override applied in InitECS
        base_primitive = primitive_manager->CreatePrimitive(
            sphere_geometry, sphere_mi[0][0], pipeline);

        return base_primitive != nullptr;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world)
            return false;

        // 计算网格原点，使整体居中于世界原点
        const float offset = (GRID_SIZE - 1) * SPHERE_SPACING * 0.5f;

        for (uint row = 0; row < GRID_SIZE; ++row)
        {
            for (uint col = 0; col < GRID_SIZE; ++col)
            {
                // col  → metallic  (X axis)
                // row  → roughness (Y axis)
                const float x = col * SPHERE_SPACING - offset;
                const float y = row * SPHERE_SPACING - offset;

                std::string name = "Sphere_M" + std::to_string(col)
                                 + "_R"       + std::to_string(row);

                Entity *e = ecs_world->CreateEntity<Entity>(name);
                sphere_entities[row][col] = e;

                auto transform = e->AddComponent<TransformComponent>(Mobility::Static);
                transform->SetLocalPosition(glm::vec3(x, y, 0.0f));
                transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
                transform->SetMovable(false);

                auto prim_comp = e->AddComponent<hgl::ecs::PrimitiveComponent>();
                prim_comp->SetPrimitive(base_primitive);
                prim_comp->SetOverrideMaterial(sphere_mi[row][col]);
                prim_comp->SetVisible(true);
            }
        }

        return true;
    }

    bool EnsureCameraSystem()
    {
        if (!ecs_world)
            return false;

        auto camera_system = ecs_world->GetSystem<CameraSystem>();
        if (!camera_system)
        {
            camera_system = ecs_world->RegisterTickSystem<CameraSystem>(ecs_world);
            if (ecs_world->IsActive())
            {
                camera_system->OnDependenciesReady();
                camera_system->Initialize();
            }
        }

        return camera_system != nullptr;
    }

    bool InitCamera()
    {
        if (!EnsureCameraSystem())
            return false;

        camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        // 俯视整个 10×10 局部 -- 拉远足够看到所有球
        camera->control_mode   = CameraComponent::ControlMode::ViewModel;
        camera->target         = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance       = 40.0f;
        camera->yaw            = 0.0f;
        camera->pitch          = -25.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty   = true;

        camera->camera_data   = GetCamera();
        camera->camera_info   = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:

    ~TestApp()
    {
        SAFE_CLEAR(base_primitive)
        SAFE_CLEAR(sphere_geometry)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.08f, 0.08f, 0.08f, 1.0f));

        if (!InitMaterial())
            return false;

        if (!CreatePBRMaterialInstances())
            return false;

        if (!CreateSphereGeometry())
            return false;

        if (!CreateBasePrimitive())
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

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("PBR Spheres 10x10 (ECS)"), argc, argv, 1280, 720);
}
