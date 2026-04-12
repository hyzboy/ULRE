// 该范例演示 4x10 的 Standard 网格：
// 4 行分别表示 Normal/Tangent 存储质量档位：LOW/MID/HIGH/FULL。
// 每列为一种内建几何体，便于在相同材质流程下直观对比法线/切线精度差异。
//
// This example renders a 4x10 Standard grid:
// 4 rows correspond to Normal/Tangent quality levels: LOW/MID/HIGH/FULL.
// Each column is one built-in geometry variant for side-by-side quality comparison.

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
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
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    enum class NTQuality : uint
    {
        LOW = 0,   // 8-bit SNORM x4
        MID,       // packed 10:10:10:2 SNORM
        HIGH,      // float16 x4
        FULL,      // float32 x4
        COUNT
    };

    constexpr uint QUALITY_COUNT = static_cast<uint>(NTQuality::COUNT);

    // NOTE:
    // LOW row: mobile low-quality profile — uses PF_NORMAL_LOW (2-channel encoded),
    // no Tangent (consistent with vfmt::kLitSurfaceN_Low_NoTangent_UV_HF16x2 policy).
    // MID/HIGH/FULL rows: all include Tangent for comparable normal-map lighting.
    const VertexFormatMap kLitSurfaceVertexFormats[QUALITY_COUNT] = {
        {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal,   PF_NORMAL_LOW},
            {VAN::TexCoord, PF_RG16F},
        },
        {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal,   PF_NORMAL_MID},
            {VAN::Tangent,  PF_TANGENT_MID},
            {VAN::TexCoord, PF_RG16F},
        },
        {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal,   PF_NORMAL_HIGH},
            {VAN::Tangent,  PF_TANGENT_HIGH},
            {VAN::TexCoord, PF_RG16F},
        },
        {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal,   PF_NORMAL_FULL},
            {VAN::Tangent,  PF_TANGENT_FULL},
            {VAN::TexCoord, PF_RG16F},
        },
    };

    static constexpr const char *kQualityName[QUALITY_COUNT] = {
        "LOW", "MID", "HIGH", "FULL"
    };
}

static constexpr uint GRID_COLS = 10;
static constexpr float SPHERE_SPACING_X = 2.7f;
static constexpr float SPHERE_SPACING_Y = 3.0f;
static constexpr uint GEOMETRY_VARIANT_COUNT = GRID_COLS;

constexpr const os_char *PBR_FOLDER_NAME[GRID_COLS] =
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

static constexpr float BASE_COLOR_R = 0.72f;
static constexpr float BASE_COLOR_G = 0.72f;
static constexpr float BASE_COLOR_B = 0.72f;

class TestApp : public WorkObject
{
private:
    ECSContext *ecs_world = nullptr;
    Entity *camera_entity = nullptr;

    MaterialTemplate *material = nullptr;
    const VIL *material_vil = nullptr;
    MaterialDomainHandle material_handle;

    Texture2DArray *base_color_texture = nullptr;
    Texture2DArray *normal_texture = nullptr;
    std::weak_ptr<Sampler> sampler;

    VertexDataManager *mesh_vdm[QUALITY_COUNT]{};
    Geometry *builtin_geometries[QUALITY_COUNT][GEOMETRY_VARIANT_COUNT]{};
    Primitive *base_primitives[QUALITY_COUNT][GEOMETRY_VARIANT_COUNT]{};

    mtl::StandardMaterialInstance sphere_mi_data[QUALITY_COUNT][GRID_COLS]{};
    MaterialInstanceHandle sphere_handle[QUALITY_COUNT][GRID_COLS]{};
    PrimitiveMaterialSlot sphere_slot[QUALITY_COUNT][GRID_COLS]{};

    Entity *sphere_entities[QUALITY_COUNT][GRID_COLS]{};
    std::shared_ptr<TransformComponent> sphere_transforms[QUALITY_COUNT][GRID_COLS]{};

    double elapsed_time = 0.0;

    Entity *sky_entity = nullptr;

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

    bool InitTextures()
    {
        auto *texture_manager = GetTextureManager();
        if (!texture_manager)
            return false;

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
        if (!BuildFilePair(first_folder, first_base, first_normal))
            return false;

        Texture2D *probe_base = texture_manager->LoadTexture2D(first_base, true);
        Texture2D *probe_normal = texture_manager->LoadTexture2D(first_normal, true);
        if (!probe_base || !probe_normal)
            return false;

        base_color_texture = texture_manager->CreateTexture2DArray("ntq_baseColor_array",
                                                                   probe_base->GetWidth(),
                                                                   probe_base->GetHeight(),
                                                                   GRID_COLS,
                                                                   probe_base->GetFormat(),
                                                                   true);
        normal_texture = texture_manager->CreateTexture2DArray("ntq_normal_array",
                                                                probe_normal->GetWidth(),
                                                                probe_normal->GetHeight(),
                                                                GRID_COLS,
                                                                probe_normal->GetFormat(),
                                                                true);

        SAFE_CLEAR(probe_base)
        SAFE_CLEAR(probe_normal)

        if (!base_color_texture || !normal_texture)
            return false;

        for (uint32_t layer = 0; layer < GRID_COLS; ++layer)
        {
            OSString folder = filesystem::JoinPathWithFilename(OS_TEXT("res/image/pbr"), PBR_FOLDER_NAME[layer]);
            OSString base_file, normal_file;
            if (!BuildFilePair(folder, base_file, normal_file))
                return false;

            if (!texture_manager->LoadTexture2DArray(base_color_texture, layer, base_file))
                return false;

            if (!texture_manager->LoadTexture2DArray(normal_texture, layer, normal_file))
                return false;
        }

        return true;
    }

    bool InitMaterial()
    {
        auto *sampler_manager = GetSamplerManager();
        if (!sampler_manager || !base_color_texture || !normal_texture)
            return false;

        static const mtl::MaterialAssetRecord kPBRArrayAcquireCfg {
            .id              = "pbr_spheres_nt_quality",
            .preset          = mtl::MaterialPreset::Standard,
            .sky             = true,
            .sky_ambient     = mtl::SkyLightAmbientModel::FakeAtmosphere,
            .lighting        = mtl::LightingModel::PBR,
            .pipeline        = GraphicsPipelinePreset::Solid3D,
            .textures        = {
                {mtl::SamplerSlot::BaseColor, mtl::TextureSourceMode::Array, ""},
                {mtl::SamplerSlot::Normal,    mtl::TextureSourceMode::Array, ""},
            },
        };

        auto *registry = GetMaterialAssetRegistry();
        if (!registry)
            return false;

        material_handle = registry->Acquire(kPBRArrayAcquireCfg);
        if (!material_handle.IsValid())
            return false;

        material = material_handle.material;
        material_vil = registry->ResolveVIL(material_handle.material, kPBRArrayAcquireCfg, nullptr);
        if (!material_vil)
            material_vil = material_handle.material ? material_handle.material->GetDefaultVIL() : nullptr;
        if (!material || !material_vil)
            return false;

        sampler = sampler_manager->CreateSampler();
        if (sampler.expired())
            return false;

        auto sampler_raw = sampler.lock().get();

        if (!material->BindTextureSampler(hgl::graph::mtl::SamplerSlot::BaseColor,
                                          base_color_texture,
                                          sampler_raw))
            return false;

        if (!material->BindTextureSampler(hgl::graph::mtl::SamplerSlot::Normal,
                                          normal_texture,
                                          sampler_raw))
            return false;

        return true;
    }

    bool CreateStandardMaterialInstances()
    {
        auto *registry = GetMaterialAssetRegistry();
        if (!registry || !material_handle.IsValid())
            return false;

        //static const float kRoughnessByQuality[QUALITY_COUNT] = {
        //    0.15f, 0.35f, 0.60f, 0.85f
        //};

        for (uint q = 0; q < QUALITY_COUNT; ++q)
        {
            for (uint col = 0; col < GRID_COLS; ++col)
            {
                float metallic = float(col) / float(GRID_COLS - 1);
                float roughness = 0.5f;//kRoughnessByQuality[q];

                mtl::StandardMaterialInstance d{};
                d.base_color = PackRGBA8Float(BASE_COLOR_R, BASE_COLOR_G, BASE_COLOR_B, 1.0f);
                d.metallic = metallic;
                d.roughness = roughness;
                d.normal_scale = 0.35f;

                auto &store = sphere_mi_data[q][col];
                store.base_color = d.base_color;
                store.metallic = d.metallic;
                store.roughness = d.roughness;
                store.normal_scale = d.normal_scale;

                MaterialBindingInit init;
                init.material = material;
                init.domain = material_handle.domain;
                init.vil = material_vil;
                init.preset = GraphicsPipelinePreset::Solid3D;
                init.material_preset = mtl::MaterialPreset::Standard;
                init.instance_data = &d;
                init.instance_data_size = sizeof(d);

                sphere_handle[q][col] = registry->AllocateHandle(init);
                if (sphere_handle[q][col] == InvalidMaterialInstanceHandle)
                    return false;

                if (!registry->BuildSlot(sphere_handle[q][col], sphere_slot[q][col]))
                    return false;

                if (!registry->SetTextureArrayLayer(sphere_handle[q][col], mtl::SamplerSlot::BaseColor, col))
                    return false;

                if (!registry->SetTextureArrayLayer(sphere_handle[q][col], mtl::SamplerSlot::Normal, col))
                    return false;
            }
        }

        return true;
    }

    bool InitVDMs()
    {
        auto *buffer_manager = GetBufferManager();
        if (!buffer_manager)
            return false;

        for (uint q = 0; q < QUALITY_COUNT; ++q)
        {
            mesh_vdm[q] = new VertexDataManager(buffer_manager, kLitSurfaceVertexFormats[q]);
            if (!mesh_vdm[q])
                return false;

            if (!mesh_vdm[q]->Init( HGL_SIZE_1MB*GEOMETRY_VARIANT_COUNT,
                                    HGL_SIZE_1MB*GEOMETRY_VARIANT_COUNT,
                                    IndexType::U16))
                return false;
        }

        return true;
    }

    bool CreateBuiltinGeometries()
    {
        using namespace inline_geometry;

        auto *graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        GraphicsGeometryFactory geometry_factory(graphics_context);

        for (uint q = 0; q < QUALITY_COUNT; ++q)
        {
            auto create_geometry = [this, &geometry_factory, q](auto &&creator) -> Geometry *
            {
                auto pc = std::make_unique<GeometryCreater>(mesh_vdm[q]);
                if (!pc)
                    return nullptr;

                auto *geometry = creator(pc.get());
                if (!geometry)
                    return nullptr;

                return geometry_factory.RegisterGeometry(geometry);
            };

            builtin_geometries[q][0] = create_geometry([](GeometryCreater *pc) { return CreateSphere(pc, 64); });
            builtin_geometries[q][1] = create_geometry([](GeometryCreater *pc)
            {
                DomeCreateInfo dci;
                dci.number_slices = 64;
                dci.inside_out = false;
                return CreateDome(pc, &dci);
            });
            builtin_geometries[q][2] = create_geometry([](GeometryCreater *pc)
            {
                ConeCreateInfo cci;
                cci.radius = 1.0f;
                cci.halfExtend = 1.0f;
                cci.numberSlices = 64;
                cci.numberStacks = 4;
                return CreateCone(pc, &cci);
            });
            builtin_geometries[q][3] = create_geometry([](GeometryCreater *pc)
            {
                CylinderCreateInfo cci;
                cci.halfExtend = 1.0f;
                cci.numberSlices = 32;
                cci.radius = 1.0f;
                return CreateCylinder(pc, &cci);
            });
            builtin_geometries[q][4] = create_geometry([](GeometryCreater *pc)
            {
                TorusCreateInfo tci;
                tci.innerRadius = 0.65f;
                tci.outerRadius = 1.25f;
                tci.numberSlices = 96;
                tci.numberStacks = 24;
                return CreateTorus(pc, &tci);
            });
            builtin_geometries[q][5] = create_geometry([](GeometryCreater *pc)
            {
                TubeCreateInfo tci;
                tci.length = 2.0f;
                tci.inner_radius = 0.6f;
                tci.outer_radius = 1.0f;
                tci.segments = 64;
                tci.generate_caps = true;
                return CreateTube(pc, &tci);
            });
            builtin_geometries[q][6] = create_geometry([](GeometryCreater *pc)
            {
                HexSphereCreateInfo hsci;
                hsci.subdivisions = 3;
                return CreateHexSphere(pc, &hsci);
            });
            builtin_geometries[q][7] = create_geometry([](GeometryCreater *pc)
            {
                CapsuleCreateInfo cci;
                return CreateCapsule(pc, &cci);
            });
            builtin_geometries[q][8] = create_geometry([](GeometryCreater *pc)
            {
                TaperedCapsuleCreateInfo tcci;
                tcci.topRadius = 0.25f;
                return CreateTaperedCapsule(pc, &tcci);
            });
            builtin_geometries[q][9] = create_geometry([](GeometryCreater *pc)
            {
                CubeCreateInfo cci;
                cci.segments_x = 1;
                cci.segments_y = 1;
                cci.segments_z = 1;
                return CreateCube(pc, &cci);
            });

            for (uint i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
            {
                if (!builtin_geometries[q][i])
                    return false;
            }
        }

        return true;
    }

    bool CreateBasePrimitives()
    {
        auto *primitive_manager = GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        for (uint q = 0; q < QUALITY_COUNT; ++q)
        {
            for (uint col = 0; col < GEOMETRY_VARIANT_COUNT; ++col)
            {
                base_primitives[q][col] = primitive_manager->CreatePrimitive(
                    builtin_geometries[q][col], sphere_slot[q][col]);

                if (!base_primitives[q][col])
                    return false;
            }
        }

        return true;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world)
            return false;

        const float offset_x = (GRID_COLS - 1) * SPHERE_SPACING_X * 0.5f;
        const float offset_y = (QUALITY_COUNT - 1) * SPHERE_SPACING_Y * 0.5f;

        for (uint q = 0; q < QUALITY_COUNT; ++q)
        {
            for (uint col = 0; col < GRID_COLS; ++col)
            {
                const float x = col * SPHERE_SPACING_X - offset_x;
                const float y = q * SPHERE_SPACING_Y - offset_y;

                std::string name = std::string("Sphere_") + kQualityName[q] + "_G" + std::to_string(col);

                Entity *e = ecs_world->CreateEntity<Entity>(name);
                sphere_entities[q][col] = e;

                auto transform = e->AddComponent<TransformComponent>(Mobility::Movable);
                sphere_transforms[q][col] = transform;
                transform->SetLocalPosition(glm::vec3(x, y, 0.0f));
                transform->SetLocalRotation(MakeRandomRotation(q, col));
                transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
                transform->SetMovable(true);

                auto prim_comp = e->AddComponent<hgl::ecs::PrimitiveComponent>();
                prim_comp->SetPrimitive(base_primitives[q][col]);
                prim_comp->SetMIIDOverride(sphere_slot[q][col].mi_id);
                prim_comp->SetVisible(true);
            }
        }

        return true;
    }

    bool InitSkySphere()
    {
        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        static const mtl::MaterialAssetRecord kSkyCfg {
            .id       = "pbr_nt_quality_sky",
            .preset   = mtl::MaterialPreset::SkyMinimal,
            .l2w      = false,
            .sky      = true,
            .pipeline = GraphicsPipelinePreset::Sky,
        };

        const SemanticMaterialId sky_semantic_id = RegisterSemanticMaterial(kSkyCfg);
        if (sky_semantic_id == 0)
            return false;

        using namespace inline_geometry;

        HexSphereCreateInfo hsci;
        hsci.subdivisions = 3;
        hsci.radius = 256;

        Primitive *ri = CreateComplexSemanticPrimitive(
            sky_semantic_id,
            "SkySphere",
            graph::vfmt::kLitSurface,
            [&](graph::GeometryCreater *pc) { return CreateHexSphere(pc, &hsci); });
        if (!ri)
            return false;

        sky_entity = ecs_world->CreateEntity<Entity>("SkySphere");
        auto transform = sky_entity->AddComponent<TransformComponent>(Mobility::Movable);
        auto prim_comp = sky_entity->AddComponent<PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitive(ri);
        prim_comp->SetSemanticMaterial(sky_semantic_id);
        prim_comp->SetVisible(true);

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

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 32.0f;
        camera->yaw = 0.0f;
        camera->pitch = -20.0f;
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
        for (uint q = 0; q < QUALITY_COUNT; ++q)
        {
            for (uint col = 0; col < GEOMETRY_VARIANT_COUNT; ++col)
                SAFE_CLEAR(base_primitives[q][col])
        }

        if (auto *registry = GetMaterialAssetRegistry())
        {
            for (uint q = 0; q < QUALITY_COUNT; ++q)
            {
                for (uint col = 0; col < GRID_COLS; ++col)
                {
                    if (sphere_handle[q][col] != InvalidMaterialInstanceHandle)
                    {
                        registry->ReleaseHandle(sphere_handle[q][col]);
                        sphere_handle[q][col] = InvalidMaterialInstanceHandle;
                    }
                }
            }
        }

        for (uint q = 0; q < QUALITY_COUNT; ++q)
            SAFE_CLEAR(mesh_vdm[q])

        SAFE_CLEAR(base_color_texture)
        SAFE_CLEAR(normal_texture)
        sampler.reset();
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

        if (!InitVDMs())
            return false;

        if (!CreateBuiltinGeometries())
            return false;

        if (!CreateBasePrimitives())
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

        if (auto render_collect = ecs_world->GetSystem<RenderPrimitiveCollectSystem>())
        {
            render_collect->SetSemanticRuntimeResolveEnabled(true);
            render_collect->SetDomainDirectMISsboEnabled(true);
            printf("[PBRSpheresNTQualityECS] Enabled domain-direct MI SSBO collect path for shared-primitive validation.\n");
        }

        return true;
    }

    void Tick(double delta_time) override
    {
        elapsed_time += delta_time;

        const float t = static_cast<float>(elapsed_time);

        for (uint q = 0; q < QUALITY_COUNT; ++q)
        {
            for (uint col = 0; col < GRID_COLS; ++col)
            {
                auto &transform = sphere_transforms[q][col];
                if (!transform)
                    continue;

                const float phase = Hash01(q, col, 101u) * 6.28318530718f;
                const float speed = 0.18f + Hash01(q, col, 211u) * 0.50f;
                const float angle = phase + t * speed;

                const glm::quat spin = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
                transform->SetLocalRotation(spin * MakeRandomRotation(q, col));
            }
        }

        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Standard BuiltinGeometry NT Quality 4x10 (LOW/MID/HIGH/FULL, ECS)"), argc, argv, 1400, 780);
}
