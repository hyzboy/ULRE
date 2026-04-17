// 该范例演示 10x10 的 PBRColor3D 网格：
// 不使用纹理，仅使用 baseColor + metallic + roughness。
// This example renders a 10x10 PBRColor3D grid:
// No textures; only baseColor + metallic + roughness.

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/color/ColorPacking.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

static constexpr uint GRID_SIZE = 10;
static constexpr float SPHERE_SPACING = 2.5f;
static constexpr uint GEOMETRY_VARIANT_COUNT = GRID_SIZE;

static constexpr float BASE_COLOR_R = 0.72f;
static constexpr float BASE_COLOR_G = 0.72f;
static constexpr float BASE_COLOR_B = 0.72f;

class TestApp : public WorkObject
{
private:
    ECSContext *ecs_world = nullptr;
    Entity *camera_entity = nullptr;

    VertexDataManager *mesh_vdm = nullptr;
    Geometry *builtin_geometries[GEOMETRY_VARIANT_COUNT]{};

    mtl::PBRColor3DMaterialInstance sphere_mi_data[GRID_SIZE][GRID_SIZE]{};

    Entity *sphere_entities[GRID_SIZE][GRID_SIZE]{};
    std::shared_ptr<TransformComponent> sphere_transforms[GRID_SIZE][GRID_SIZE]{};

    double elapsed_time = 0.0;

    Entity* sky_entity = nullptr;

    inline static const mtl::MaterialRecipe kPBRColorCfg {
        .id          = "pbr_color_spheres",
        .preset      = mtl::MaterialPreset::PBRColor3D,
        .sky         = true,
        .pipeline    = GraphicsPipelinePreset::Solid3D,
    };

    inline static const mtl::MaterialRecipe kSkyCfg {
        .id       = "pbr_color_sky",
        .preset   = mtl::MaterialPreset::SkyMinimal,
        .l2w      = false,
        .sky      = true,
        .pipeline = GraphicsPipelinePreset::Sky,
    };

private:
    static uint HashU32(uint row, uint col, uint salt)
    {
        uint x = row * 73856093u ^ col * 19349663u ^ salt * 83492791u;
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    static float Hash01(uint row, uint col, uint salt)
    {
        return float(HashU32(row, col, salt) & 0x00FFFFFFu) / float(0x00FFFFFFu);
    }

    static glm::quat MakeRandomRotation(uint row, uint col)
    {
        constexpr float TAU = 6.28318530718f;

        const float rx = Hash01(row, col, 11u) * TAU;
        const float ry = Hash01(row, col, 23u) * TAU;
        const float rz = Hash01(row, col, 37u) * TAU;

        return glm::quat(glm::vec3(rx, ry, rz));
    }

    bool CreatePBRColorMaterialInstances()
    {
        for (uint row = 0; row < GRID_SIZE; ++row)
        {
            for (uint col = 0; col < GRID_SIZE; ++col)
            {
                const float metallic = float(col) / float(GRID_SIZE - 1);
                const float roughness = 0.05f + float(row) / float(GRID_SIZE - 1) * 0.95f;

                auto &store = sphere_mi_data[row][col];
                store.base_color = PackRGBA8Float(BASE_COLOR_R, BASE_COLOR_G, BASE_COLOR_B, 1.0f);
                store.metallic = metallic;
                store.roughness = roughness;
            }
        }

        return true;
    }

    bool InitVDM()
    {
        auto *render_context = GetRenderContext();
        if (!render_context)
        {
            printf("[ERROR] InitVDM: No render_context\n");
            return false;
        }

        auto *graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
        {
            printf("[ERROR] InitVDM: No graphics_context\n");
            return false;
        }

        auto *buffer_manager = GetBufferManager();
        if (!buffer_manager)
        {
            printf("[ERROR] InitVDM: No buffer_manager\n");
            return false;
        }

        if (sphere_mi_data[0][0].base_color == 0) return false;
        GeometryVertexFormat gvf;
        gvf.Set(VAN::Position, VF_V3F);
        gvf.Set(VAN::Normal,   VF_V3F);
        gvf.Set(VAN::TexCoord, VF_V2F);
        mesh_vdm = new VertexDataManager(buffer_manager, gvf);
        if (!mesh_vdm)
        {
            printf("[ERROR] InitVDM: Failed to create VertexDataManager\n");
            return false;
        }

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U16))
        {
            printf("[ERROR] InitVDM: Failed to init VertexDataManager\n");
            return false;
        }

        return true;
    }

    bool CreateBuiltinGeometries()
    {
        using namespace inline_geometry;

        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        GraphicsGeometryFactory geometry_factory(graphics_context);

        auto create_geometry = [this, &geometry_factory](auto &&creator) -> Geometry *
        {
            auto pc = std::make_unique<GeometryCreater>(mesh_vdm);
            if (!pc)
                return nullptr;

            auto* geometry = creator(pc.get());
            if (!geometry)
                return nullptr;

            return geometry_factory.RegisterGeometry(geometry);
        };

        builtin_geometries[0] = create_geometry([](GeometryCreater *pc) { return CreateSphere(pc, 64); });
        builtin_geometries[1] = create_geometry([](GeometryCreater *pc)
        {
            DomeCreateInfo dci;
            dci.number_slices = 64;
            dci.inside_out = true;
            return CreateDome(pc, &dci);
        });

        builtin_geometries[2] = create_geometry([](GeometryCreater *pc)
        {
            ConeCreateInfo cci;
            cci.radius = 1.0f;
            cci.halfExtend = 1.0f;
            cci.numberSlices = 64;
            cci.numberStacks = 4;
            return CreateCone(pc, &cci);
        });

        builtin_geometries[3] = create_geometry([](GeometryCreater *pc)
        {
            CylinderCreateInfo cci;
            cci.halfExtend = 1.0f;
            cci.numberSlices = 32;
            cci.radius = 1.0f;
            return CreateCylinder(pc, &cci);
        });

        builtin_geometries[4] = create_geometry([](GeometryCreater *pc)
        {
            TorusCreateInfo tci;
            tci.innerRadius = 0.65f;
            tci.outerRadius = 1.25f;
            tci.numberSlices = 96;
            tci.numberStacks = 24;
            return CreateTorus(pc, &tci);
        });

        builtin_geometries[5] = create_geometry([](GeometryCreater *pc)
        {
            TubeCreateInfo tci;
            tci.length        = 2.0f; // halfExtend * 2
            tci.inner_radius  = 0.6f;
            tci.outer_radius  = 1.0f;
            tci.segments      = 64;
            tci.generate_caps = true;
            return CreateTube(pc, &tci);
        });

        builtin_geometries[6] = create_geometry([](GeometryCreater *pc)
        {
            HexSphereCreateInfo hsci;
            hsci.subdivisions = 3;
            return CreateHexSphere(pc, &hsci);
        });

        builtin_geometries[7] = create_geometry([](GeometryCreater *pc)
        {
            CapsuleCreateInfo cci;
            return CreateCapsule(pc, &cci);
        });

        builtin_geometries[8] = create_geometry([](GeometryCreater *pc)
        {
            TaperedCapsuleCreateInfo tcci;
            tcci.topRadius = 0.25f;
            return CreateTaperedCapsule(pc, &tcci);
        });

        builtin_geometries[9] = create_geometry([](GeometryCreater *pc)
        {
            CubeCreateInfo cci;
            cci.segments_x = 1;
            cci.segments_y = 1;
            cci.segments_z = 1;
            return CreateCube(pc, &cci);
        });

        for (uint i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
        {
            if (!builtin_geometries[i])
            {
                printf("[ERROR] CreateBuiltinGeometries: Failed to create geometry %u\n", i);
                return false;
            }
        }

        return true;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world)
        {
            printf("[ERROR] InitECS: No ecs_world\n");
            return false;
        }

        const float offset = (GRID_SIZE - 1) * SPHERE_SPACING * 0.5f;

        for (uint row = 0; row < GRID_SIZE; ++row)
        {
            for (uint col = 0; col < GRID_SIZE; ++col)
            {
                const float x = col * SPHERE_SPACING - offset;
                const float y = row * SPHERE_SPACING - offset;

                std::string name = "Sphere_M" + std::to_string(col)
                                 + "_R" + std::to_string(row);

                Entity *e = ecs_world->CreateEntity<Entity>(name);
                sphere_entities[row][col] = e;

                auto transform = e->AddComponent<TransformComponent>(Mobility::Movable);
                sphere_transforms[row][col] = transform;
                transform->SetLocalPosition(glm::vec3(x, y, 0.0f));
                transform->SetLocalRotation(MakeRandomRotation(row, col));
                transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
                transform->SetMovable(true);

                auto prim_comp = e->AddComponent<hgl::ecs::PrimitiveComponent>();
                prim_comp->SetUnresolvedGeometry(builtin_geometries[col]);
                prim_comp->SetMaterialRecipe(&kPBRColorCfg, &sphere_mi_data[row][col], sizeof(mtl::PBRColor3DMaterialInstance));
                prim_comp->SetVisible(true);
            }
        }

        return true;
    }

    bool InitSkySphere()
    {

        auto* device = GetDevice();
        auto* geometry_manager = GetGeometryManager();
        if (!device || !geometry_manager)
            return false;

        GeometryVertexFormat sky_gvf;
        sky_gvf.Set(VAN::Position, VF_V3F);

        auto pc = std::make_unique<GeometryCreater>(device, sky_gvf);
        if (!pc)
            return false;

        using namespace inline_geometry;
        HexSphereCreateInfo hsci;
        hsci.subdivisions = 3;
        hsci.radius = 256;
        Geometry* sky_geom = CreateHexSphere(pc.get(), &hsci);
        if (!sky_geom)
            return false;

        geometry_manager->Add(sky_geom);

        sky_entity = ecs_world->CreateEntity<Entity>("SkySphere");
        auto transform = sky_entity->AddComponent<TransformComponent>(Mobility::Movable);
        auto prim_comp = sky_entity->AddComponent<PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetUnresolvedGeometry(sky_geom);
        prim_comp->SetMaterialRecipe(&kSkyCfg);
        prim_comp->SetVisible(true);

        return true;
    }

    bool EnsureCameraSystem()
    {
        if (!ecs_world)
        {
            printf("[ERROR] EnsureCameraSystem: No ecs_world\n");
            return false;
        }

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

        if (!camera_system)
        {
            printf("[ERROR] EnsureCameraSystem: Failed to get or create camera system\n");
            return false;
        }

        return true;
    }

    bool InitCamera()
    {
        if (!EnsureCameraSystem())
        {
            printf("[ERROR] InitCamera: Failed to ensure camera system\n");
            return false;
        }

        camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 40.0f;
        camera->yaw = 0.0f;
        camera->pitch = -25.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~TestApp()
    {
        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.08f, 0.08f, 0.08f, 1.0f));

        if (!CreatePBRColorMaterialInstances())
            return false;

        if (!InitVDM())
            return false;

        if (!CreateBuiltinGeometries())
            return false;

        if (!InitECS())
            return false;

        {
            auto env = ecs_world->GetSystem<EnvironmentSystem>();
            if (!env)
                env = ecs_world->RegisterRenderSystem<EnvironmentSystem>();
            if (env)
            {
                env->EditSkyInfo();
                env->SyncSkyUBO();
            }
        }

        if (!InitSkySphere())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        elapsed_time += delta_time;

        const float t = static_cast<float>(elapsed_time);

        for (uint row = 0; row < GRID_SIZE; ++row)
        {
            for (uint col = 0; col < GRID_SIZE; ++col)
            {
                auto &transform = sphere_transforms[row][col];
                if (!transform)
                    continue;

                const float phase = Hash01(row, col, 101u) * 6.28318530718f;
                const float speed = 0.20f + Hash01(row, col, 211u) * 0.55f;
                const float angle = phase + t * speed;

                const glm::quat spin = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
                transform->SetLocalRotation(spin * MakeRandomRotation(row, col));
            }
        }

        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("PBRColor3D BuiltinGeometry 10x10 (ECS, no textures)"), argc, argv, 1280, 720);
}

