// 该范例演示 10x10 的 Standard 网格：
// 使用 baseColor + normal 纹理（Albedo+Normal）
// 通过 metallic/roughness 参数渐变来控制材质
// This example renders a 10x10 Standard grid:
// Uses baseColor + normal textures (Albedo+Normal only)
// Metallic/roughness gradients control material properties.

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/ssbo/StandardMaterialInstance.h>
#include<hgl/graph/ssbo/StandardTextureArrayMaterialInstance.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/color/ColorPacking.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/MaterialComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<cstring>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateStandardTextureArrayGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }
}

// 10×10 grid params
static constexpr uint GRID_SIZE       = 10;    // spheres per axis
static constexpr float SPHERE_SPACING = 2.5f;  // center-to-center distance

// 10 built-in geometries, one per column
static constexpr uint GEOMETRY_VARIANT_COUNT = GRID_SIZE;

// PBR material folders under res/image/pbr (currently uses first folder as Standard texture set)
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

    graph::mtl::MaterialRecipe sphere_recipe{};
    graph::SSBOArrayAccessor<ssbo::StandardMaterialInstance>* mi_ssbo_accessor = nullptr;
    Texture2DArray *    base_color_texture = nullptr;
    Texture2DArray *    normal_texture = nullptr;
    Sampler *           sampler = nullptr;
    std::unique_ptr<BindlessTextureManager> bindless_texture_manager;

    VertexDataManager * mesh_vdm = nullptr;
    Geometry *          builtin_geometries[GEOMETRY_VARIANT_COUNT]{};
    PrimitiveAsset      base_primitives[GEOMETRY_VARIANT_COUNT]{};

    // One MI per cell: col controls metallic, row controls roughness
    ssbo::StandardMaterialInstance sphere_mi_data[GRID_SIZE][GRID_SIZE]{};
    uint32_t                         sphere_slot_rows[GRID_SIZE][GRID_SIZE]{};

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
        auto* sampler_manager = GetManager<SamplerManager>();
        if (!sampler_manager || !base_color_texture || !normal_texture) {
            printf("[ERROR] InitMaterial: Failed manager/texture checks - material_mgr=%p sampler_mgr=%p base=%p normal=%p\n",
                   nullptr, sampler_manager, base_color_texture, normal_texture);
            return false;
        }
        sphere_recipe.recipe_name = "PBRSpheres.StandardTextureArray";
        sphere_recipe.mtl_def_id = "StandardTextureArray";
        sphere_recipe.pipeline_preset = PipelinePreset::Solid3D;
        sphere_recipe.domain = "PBRSpheres";

        sampler = sampler_manager->CreateSampler();
        if (!sampler) {
            printf("[ERROR] InitMaterial: Failed to create sampler\n");
            return false;
        }

        return true;
    }

    bool InitTextures()
    {
        auto* texture_manager = GetManager<TextureManager>();
        if (!texture_manager) {
            printf("[ERROR] InitTextures: No texture_manager\n");
            return false;
        }

        auto BuildFilePair = [](const OSString &folder, OSString &base, OSString &normal) -> bool
        {
            base = filesystem::JoinPathWithFilename(folder, OS_TEXT("baseColor.Tex2D"));
            normal = filesystem::JoinPathWithFilename(folder, OS_TEXT("normal.Tex2D"));
            if (!filesystem::FileExist(normal))
                normal = filesystem::JoinPathWithFilename(folder, OS_TEXT("Normal.Tex2D"));

            return filesystem::FileExist(base)
                && filesystem::FileExist(normal);
        };

        OSString first_folder = filesystem::JoinPathWithFilename(OS_TEXT("res/image/pbr"), PBR_FOLDER_NAME[0]);
        OSString first_base, first_normal;
        if (!BuildFilePair(first_folder, first_base, first_normal)) {
            printf("[ERROR] InitTextures: Failed to find texture pair for folder[0]\n");
            return false;
        }

        Texture2D *probe_base = texture_manager->LoadTexture2D(first_base, true);
        Texture2D *probe_normal = texture_manager->LoadTexture2D(first_normal, true);
        if (!probe_base || !probe_normal) {
            printf("[ERROR] InitTextures: Failed to load probe textures - base=%p normal=%p\n", 
                   probe_base, probe_normal);
            return false;
        }

        base_color_texture = texture_manager->CreateTexture2DArray("pbr_baseColor_array",
                                                                   probe_base->GetWidth(),
                                                                   probe_base->GetHeight(),
                                                                   GRID_SIZE,
                                                                   probe_base->GetFormat(),
                                                                   true);
        normal_texture = texture_manager->CreateTexture2DArray("pbr_normal_array",
                                                                 probe_normal->GetWidth(),
                                                                 probe_normal->GetHeight(),
                                                                 GRID_SIZE,
                                                                 probe_normal->GetFormat(),
                                                                 true);

        SAFE_CLEAR(probe_base)
        SAFE_CLEAR(probe_normal)

        if (!base_color_texture || !normal_texture) {
            printf("[ERROR] InitTextures: Failed to create Texture2DArray - base_color=%p normal=%p\n", 
                   base_color_texture, normal_texture);
            return false;
        }

        for (uint32_t layer = 0; layer < GRID_SIZE; ++layer)
        {
            OSString folder = filesystem::JoinPathWithFilename(OS_TEXT("res/image/pbr"), PBR_FOLDER_NAME[layer]);
            OSString base_file, normal_file;
            if (!BuildFilePair(folder, base_file, normal_file)) {
                printf("[ERROR] InitTextures: Layer %u - Failed to find texture pair for PBR_FOLDER_NAME[%u]\n", layer, layer);
                return false;
            }

            if (!texture_manager->LoadTexture2DArray(base_color_texture, layer, base_file)) {
                printf("[ERROR] InitTextures: Layer %u - Failed to load baseColor texture\n", layer);
                return false;
            }
            if (!texture_manager->LoadTexture2DArray(normal_texture, layer, normal_file)) {
                printf("[ERROR] InitTextures: Layer %u - Failed to load normal texture\n", layer);
                return false;
            }
        }

        return true;
    }

    bool CreateStandardMaterialInstances()
    {
        for (uint row = 0; row < GRID_SIZE; ++row)
        {
            for (uint col = 0; col < GRID_SIZE; ++col)
            {
                // col  → metallic:  0.0 → 1.0
                // row  → roughness: 0.05 → 1.0  (avoid perfectly smooth mirrors at 0)
                float metallic  = float(col) / float(GRID_SIZE - 1);
                float roughness = 0.05f + float(row) / float(GRID_SIZE - 1) * 0.95f;

                ssbo::StandardTextureArrayMaterialInstance d{};
                d.base_color = PackRGBA8Float(BASE_COLOR_R, BASE_COLOR_G, BASE_COLOR_B, 1.0f);
                d.metallic   = metallic;
                d.roughness  = roughness;
                d.normal_scale = 0.35f;

                auto &store = sphere_mi_data[row][col];
                store.base_color = d.base_color;
                store.metallic = d.metallic;
                store.roughness = d.roughness;
                store.normal_scale = d.normal_scale;

                sphere_slot_rows[row][col] = uint32_t(-1);
            }
        }

        return true;
    }

    bool InitMISSBO()
    {
        if (!ecs_world)
            return false;

        auto *domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        const uint32_t mi_count = GRID_SIZE * GRID_SIZE;

        mi_ssbo_accessor = domain_manager->AllocateArrayAccessor<ssbo::StandardMaterialInstance>(
            graph::mtl::SSBOType::ClearCoatSurface,
            "PBRSpheres:ClearCoatSurface:MIData",
            mi_count);
        if (!mi_ssbo_accessor)
            return false;

        for (uint row = 0; row < GRID_SIZE; ++row)
        {
            for (uint col = 0; col < GRID_SIZE; ++col)
            {
                const uint32_t slot_index = row * GRID_SIZE + col;
                sphere_slot_rows[row][col] = slot_index;
                (*mi_ssbo_accessor)[slot_index] = sphere_mi_data[row][col];
            }
        }
        mi_ssbo_accessor->Commit();
        return true;
    }

    bool InitVDM()
    {
        auto* buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager) {
            printf("[ERROR] InitVDM: No buffer_manager\n");
            return false;
        }

        mesh_vdm = new VertexDataManager(
            buffer_manager,
            CreateStandardTextureArrayGeometryVertexFormat());
        if (!mesh_vdm) {
            printf("[ERROR] InitVDM: Failed to create VertexDataManager\n");
            return false;
        }

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U16)) {
            printf("[ERROR] InitVDM: Failed to init VertexDataManager\n");
            return false;
        }
        return true;
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
            if (!builtin_geometries[i]) {
                printf("[ERROR] CreateBuiltinGeometries: Failed to create geometry %u\n", i);
                return false;
            }
        }

        return true;
    }

    bool CreateBasePrimitives()
    {
        for (uint i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
        {
            if (!builtin_geometries[i]) {
                printf("[ERROR] CreateBasePrimitives: Failed to create primitive %u\n", i);
                return false;
            }
            base_primitives[i] = PrimitiveAsset(builtin_geometries[i], &sphere_recipe, PrimitiveType::Triangles);
        }

        return true;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world) {
            printf("[ERROR] InitECS: No ecs_world\n");
            return false;
        }

        if (!InitMISSBO())
            return false;

        graph::mtl::UpsertRecipeSSBOAssetBinding(sphere_recipe,
                                                 graph::mtl::DefaultMaterialSSBOName,
                                                 mi_ssbo_accessor->GetSSBOBinding());

        if (!CreateBasePrimitives())
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
                prim_comp->SetPrimitiveAsset(&base_primitives[col]);
                prim_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor,
                                                      base_color_texture,
                                                      sampler,
                                                      PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray);
                prim_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Normal,
                                                      normal_texture,
                                                      sampler,
                                                      PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray);
                prim_comp->SetMaterialTextureValue(graph::mtl::TextureSlot::Custom0, row);
                hgl::ecs::PrimitiveComponent::MaterialSSBONamedAuthoringResource sphere_struct{};
                sphere_struct.ssbo_name = graph::mtl::DefaultMaterialSSBOName;
                sphere_struct.ssbo_id = mi_ssbo_accessor->GetSSBOId();
                sphere_struct.ssbo_element_index = sphere_slot_rows[row][col];
                sphere_struct.use_ssbo_element_index = true;
                sphere_struct.shared_across_instances = false;
                prim_comp->SetMaterialSSBOResource(sphere_struct);
                prim_comp->SetVisible(true);
            }
        }

        return true;
    }

    bool EnsureCameraSystem()
    {
        if (!ecs_world) {
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

        if (!camera_system) {
            printf("[ERROR] EnsureCameraSystem: Failed to get or create camera system\n");
            return false;
        }
        return true;
    }

    bool InitCamera()
    {
        if (!EnsureCameraSystem()) {
            printf("[ERROR] InitCamera: Failed to ensure camera system\n");
            return false;
        }

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
            SAFE_CLEAR(builtin_geometries[i])
        }

        SAFE_CLEAR(mesh_vdm)
        SAFE_CLEAR(mi_ssbo_accessor)
        SAFE_CLEAR(base_color_texture)
        SAFE_CLEAR(normal_texture)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.08f, 0.08f, 0.08f, 1.0f));

        if (!InitTextures())
            return false;

        if (!InitMaterial())
            return false;

        if (!CreateStandardMaterialInstances())
            return false;

        if (!InitVDM())
            return false;

        if (!CreateBuiltinGeometries())
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
    return RunFramework<TestApp>(OS_TEXT("Standard BuiltinGeometry x Albedo+Normal 10x10 (ECS)"), argc, argv, 1280, 720);
}
