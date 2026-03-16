// 该范例演示 10x10 的 BasicLit 网格：
// 使用 baseColor + normal + roughness 纹理，并保留 metallic/roughness 渐变。
// This example renders a 10x10 BasicLit grid:
// Uses baseColor + normal + roughness textures with metallic/roughness gradients.

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
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

// 10 built-in geometries, one per column
static constexpr uint GEOMETRY_VARIANT_COUNT = GRID_SIZE;

// PBR material folders under res/image/pbr (currently uses first folder as BasicLit texture set)
constexpr const os_char *PBR_FOLDER_NAME[GRID_SIZE]=
{
    OS_TEXT("Concrete_Plain"),
    OS_TEXT("Concrete_Planks"),
    OS_TEXT("Concrete_Tiles"),
    OS_TEXT("Fresco_Decor_Wallpaper"),
    OS_TEXT("TH_Brown_Leather"),
    OS_TEXT("TH_Cobblestone_Color"),
    OS_TEXT("TH_Large_Square_Pattern"),
    OS_TEXT("TH_Sandstone_Blocks"),
    OS_TEXT("TH_Sidewalk_Brick_Floor"),
    OS_TEXT("TH_Square_Floor_Pattern")
};

// Base albedo for all spheres (classic gray PBR test chart color)
static constexpr float BASE_COLOR_R = 0.72f;
static constexpr float BASE_COLOR_G = 0.72f;
static constexpr float BASE_COLOR_B = 0.72f;

class TestApp : public WorkObject
{
private:

    ECSContext *  ecs_world     = nullptr;
    Entity *      camera_entity = nullptr;

    Material *          material  = nullptr;
    Pipeline *          pipeline  = nullptr;
    Texture2D *         base_color_texture = nullptr;
    Texture2D *         normal_texture = nullptr;
    Texture2D *         roughness_texture = nullptr;
    Sampler *           sampler = nullptr;

    VertexDataManager * mesh_vdm = nullptr;
    Geometry *          builtin_geometries[GEOMETRY_VARIANT_COUNT]{};
    Primitive *         base_primitives[GEOMETRY_VARIANT_COUNT]{};

    // One MI per cell: col controls metallic, row controls roughness
    mtl::StandardMaterialInstance sphere_mi_data[GRID_SIZE][GRID_SIZE]{};
    MaterialInstance *               sphere_mi[GRID_SIZE][GRID_SIZE]{};

    // 100 entities, one per sphere
    Entity *sphere_entities[GRID_SIZE][GRID_SIZE]{};
    std::shared_ptr<TransformComponent> sphere_transforms[GRID_SIZE][GRID_SIZE]{};

    double elapsed_time = 0.0;

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
        // Convert lower 24 bits to [0, 1].
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

    bool InitMaterial()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* sampler_manager = graphics_context->GetSamplerManager();
        if (!material_manager || !sampler_manager || !base_color_texture || !normal_texture || !roughness_texture)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles,
                        mtl::WithCamera::With,
                        mtl::WithLocalToWorld::With,
                        mtl::WithSky::With);
        material = material_manager->CreateMaterial(mtl::MaterialPreset::Standard, &cfg);
        if (!material)
            return false;

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return false;

        if (!material->BindTextureSampler(DescriptorSetType::Material,
                                          mtl::SamplerName::BaseColor,
                                          base_color_texture,
                                          sampler))
            return false;

        if (!material->BindTextureSampler(DescriptorSetType::Material,
                                          "TextureNormal",
                                          normal_texture,
                                          sampler))
            return false;

        if (!material->BindTextureSampler(DescriptorSetType::Material,
                                          "TextureRoughness",
                                          roughness_texture,
                                          sampler))
            return false;

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass   = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material, InlinePipeline::Solid3D) : nullptr;

        return pipeline != nullptr;
    }

    bool InitTextures()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* texture_manager = graphics_context->GetTextureManager();
        if (!texture_manager)
            return false;

        OSString folder = filesystem::JoinPathWithFilename(OS_TEXT("res/image/pbr"),
                                                           PBR_FOLDER_NAME[0]);

        OSString base_color_filename = filesystem::JoinPathWithFilename(folder,
                                                                         OS_TEXT("baseColor.Tex2D"));

        OSString normal_filename = filesystem::JoinPathWithFilename(folder,
                                                                     OS_TEXT("normal.Tex2D"));
        if (!filesystem::FileExist(normal_filename))
            normal_filename = filesystem::JoinPathWithFilename(folder, OS_TEXT("Normal.Tex2D"));

        OSString roughness_filename = filesystem::JoinPathWithFilename(folder,
                                                                        OS_TEXT("roughness.Tex2D"));
        if (!filesystem::FileExist(base_color_filename)
         || !filesystem::FileExist(normal_filename)
         || !filesystem::FileExist(roughness_filename))
            return false;

        base_color_texture = texture_manager->LoadTexture2D(base_color_filename, true);
        if (!base_color_texture)
            return false;

        normal_texture = texture_manager->LoadTexture2D(normal_filename, true);
        if (!normal_texture)
            return false;

        roughness_texture = texture_manager->LoadTexture2D(roughness_filename, true);
        if (!roughness_texture)
            return false;

        return true;
    }

    bool CreateBasicLitMaterialInstances()
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
                d.normal_scale = 0.35f;

                sphere_mi[row][col] = material_manager->CreateMaterialInstance(
                    material, (VIL *)nullptr, &d);

                if (!sphere_mi[row][col])
                    return false;
            }
        }

        return true;
    }

    bool InitVDM()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* buffer_manager = graphics_context->GetBufferManager();
        if (!buffer_manager)
            return false;

        mesh_vdm = new VertexDataManager(buffer_manager, material->GetDefaultVIL());
        if (!mesh_vdm)
            return false;

        return mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U16);
    }

    bool CreateBuiltinGeometries()
    {
        using namespace inline_geometry;

        auto create_geometry = [this](auto &&creator) -> Geometry *
        {
            auto pc = std::make_unique<GeometryCreater>(mesh_vdm);
            if (!pc)
                return nullptr;

            return creator(pc.get());
        };

        builtin_geometries[0] = create_geometry([](GeometryCreater *pc)
        {
            return CreateSphere(pc, 64);
        });

        builtin_geometries[1] = create_geometry([](GeometryCreater *pc)
        {
            return CreateDome(pc, 64);
        });

        builtin_geometries[2] = create_geometry([](GeometryCreater *pc)
        {
            ConeCreateInfo cci;
            cci.radius       = 1.0f;
            cci.halfExtend   = 1.0f;
            cci.numberSlices = 64;
            cci.numberStacks = 4;
            return CreateCone(pc, &cci);
        });

        builtin_geometries[3] = create_geometry([](GeometryCreater *pc)
        {
            CylinderCreateInfo cci;
            cci.halfExtend   = 1.0f;
            cci.numberSlices = 32;
            cci.radius       = 1.0f;
            return CreateCylinder(pc, &cci);
        });

        builtin_geometries[4] = create_geometry([](GeometryCreater *pc)
        {
            TorusCreateInfo tci;
            tci.innerRadius  = 0.65f;
            tci.outerRadius  = 1.25f;
            tci.numberSlices = 96;
            tci.numberStacks = 24;
            return CreateTorus(pc, &tci);
        });

        builtin_geometries[5] = create_geometry([](GeometryCreater *pc)
        {
            HollowCylinderCreateInfo hcci;
            hcci.halfExtend   = 1.0f;
            hcci.innerRadius  = 0.6f;
            hcci.outerRadius  = 1.0f;
            hcci.numberSlices = 64;
            return CreateHollowCylinder(pc, &hcci);
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
                return false;
        }

        return true;
    }

    bool CreateBasePrimitives()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        for (uint i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
        {
            // Per-entity override material is still applied in InitECS.
            base_primitives[i] = primitive_manager->CreatePrimitive(
                builtin_geometries[i], sphere_mi[0][i], pipeline);

            if (!base_primitives[i])
                return false;
        }

        return true;
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

                auto transform = e->AddComponent<TransformComponent>(Mobility::Movable);
                sphere_transforms[row][col] = transform;
                transform->SetLocalPosition(glm::vec3(x, y, 0.0f));
                transform->SetLocalRotation(MakeRandomRotation(row, col));
                transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
                transform->SetMovable(true);

                auto prim_comp = e->AddComponent<hgl::ecs::PrimitiveComponent>();
                prim_comp->SetPrimitive(base_primitives[col]);
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
        for (uint i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
        {
            SAFE_CLEAR(base_primitives[i])
            SAFE_CLEAR(builtin_geometries[i])
        }

        SAFE_CLEAR(mesh_vdm)
        SAFE_CLEAR(base_color_texture)
        SAFE_CLEAR(normal_texture)
        SAFE_CLEAR(roughness_texture)
        SAFE_CLEAR(sampler)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.08f, 0.08f, 0.08f, 1.0f));

        if (!InitTextures())
            return false;

        if (!InitMaterial())
            return false;

        if (!CreateBasicLitMaterialInstances())
            return false;

        if (!InitVDM())
            return false;

        if (!CreateBuiltinGeometries())
            return false;

        if (!CreateBasePrimitives())
            return false;

        if (!InitECS())
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

                // Keep per-cell phase/speed differences to make highlights easier to observe.
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
    return RunFramework<TestApp>(OS_TEXT("BasicLit BuiltinGeometry x BaseColor+Normal+Roughness 10x10 (ECS)"), argc, argv, 1280, 720);
}
