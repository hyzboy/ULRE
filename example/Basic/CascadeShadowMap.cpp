#include <hgl/framework/WorkManager.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/vk/VKTexture.h>
#include <hgl/vk/VertexDataManager.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/render/RenderTargetDesc.h>
#include <hgl/graph/module/RenderTargetManager.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/ssbo/MaterialDataRows.h>
#include <hgl/graph/module/EnvironmentManager.h>
#include <hgl/graph/ubo/SkyInfo.h>
#include <hgl/graph/ubo/ShadowInfo.h>
#include <hgl/graph/camera/ReversedZProj.h>
#include <hgl/vk/VKBindlessTextureManager.h>
#include <hgl/graph/geo/InlineGeometry.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/render/lighting/CascadedShadowController.h>
#include <hgl/color/Color.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/graph/ssbo/LitMaterialData.h>
#include <hgl/filesystem/Filename.h>
#include <hgl/filesystem/FileSystem.h>

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/core/ScenePipelineMode.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/components/PrimitiveComponent.h>
#include <hgl/ecs/components/ShadowComponent.h>
#include <hgl/ecs/components/CameraComponent.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/render/RenderTargetSystem.h>
#include <hgl/ecs/systems/render/RenderSystemCore.h>
#include <hgl/ecs/systems/render/EnvironmentSystem.h>
#include <hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include <hgl/ecs/systems/tick/InputSystem.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>

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

    constexpr uint32_t kPBRTextureCount = 10;
    constexpr uint32_t kBuiltinGeomCount = 10;
    constexpr uint32_t kTotalObjectCount = 100;
    constexpr uint32_t kMovableCount = 20; // 前 20 个近景物体为 Movable，后 80 个为 Static
    constexpr uint32_t kShadowMapSize = 1024;
    constexpr float    kGroundExtent = 500.0f;

    // ── 阴影受光侧参数（世界单位米）——示例的"配方值"，调好后固化在这里 ────────
    // 两者互补：法线偏移按 tan(θ) 加权，只推斜射面（消掠射角 acne），正面几乎不动；
    // 深度 bias 与角度无关，负责整体贴合量（负值 = 阴影贴着遮挡体）。
    // 调参顺序：先找掠射面刚好看不到条纹的最小 offset，再把 |bias_world| 往 0 收，
    // 收到接触点刚要漏光为止。运行时仍可用 `-`/`=` 调 offset、`[`/`]` 调 bias 微调。
    constexpr float kShadowNormalOffsetWorld = 0.35f;  // 米；量级 ≈ 一个纹素的世界尺寸（CSM 0: 188m/1024texel）
    constexpr float kShadowBiasWorld         = -1.15f; // 米；负值 = 贴合遮挡体（正值会漏光）
    constexpr float kShadowTuneStepWorld     = 0.05f;  // 运行时微调步长（两种参数共用，米）

    constexpr const os_char *PBR_FOLDER_NAME[kPBRTextureCount] =
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

    uint32_t HashU32(uint32_t a, uint32_t b, uint32_t salt)
    {
        uint32_t x = a * 73856093u ^ b * 19349663u ^ salt * 83492791u;
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    float Hash01(uint32_t a, uint32_t b, uint32_t salt)
    {
        return static_cast<float>(HashU32(a, b, salt) & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
    }

    float GroundLift(const Geometry *geom)
    {
        if (!geom)
            return 0.0f;

        const math::BoundingVolumes &bv = geom->GetBoundingVolumes();
        if (bv.aabb.IsEmpty())
            return 0.0f;

        const float min_z = bv.aabb.GetMin().z;
        return (min_z < 0.0f) ? -min_z : 0.0f;
    }
}

class CascadeShadowMapApp final : public WorkObject
{
private:
    ECSContext *ecs_context = nullptr;

    Entity *main_camera_entity = nullptr;
    std::shared_ptr<CameraComponent> main_camera;
    std::shared_ptr<CameraComponent> light_camera;
    std::shared_ptr<CameraSystem> camera_system;

    std::shared_ptr<EnvironmentSystem> environment_system;
    graph::SkyInfo *sky_info = nullptr;

    VertexDataManager *vdm = nullptr;
    Texture2DArray *base_color_texture = nullptr;
    Texture2DArray *normal_texture = nullptr;
    Sampler *pbr_sampler = nullptr;

    Geometry *builtin_geometries[kBuiltinGeomCount]{};
    PrimitiveAsset builtin_primitives[kBuiltinGeomCount]{};

    Geometry *ground_geometry = nullptr;
    PrimitiveAsset ground_primitive{};
    Entity *ground_entity = nullptr;
    std::shared_ptr<TransformComponent> ground_transform;
    std::shared_ptr<PrimitiveComponent> ground_prim;

    graph::mtl::MaterialRecipe lit_recipe{};
    graph::GlobalSSBODataAccessor material_accessors[kPBRTextureCount]{};
    graph::GlobalSSBODataAccessor ground_accessor{};

    // 静态太阳光方向（指向场景地面）
    glm::vec3 sun_direction{0.5f, 0.6f, 0.8f};
    math::Vector3f light_dir_math{0.0f};

    // ── 动态物体（Movable）动画追踪 ──
    struct MovableTrack
    {
        std::shared_ptr<TransformComponent> transform;
        glm::vec3 base_pos{0.0f};
        glm::vec3 spin_axis{0.0f, 0.0f, 1.0f};
        float spin_speed = 1.0f;
        float orbit_radius = 0.0f;
        float orbit_speed = 0.0f;
        float orbit_phase = 0.0f;
        float hover_amplitude = 0.0f;
        float hover_speed = 0.0f;
    };
    MovableTrack movable_tracks[kMovableCount];

    // 性能与条带统计
    double elapsed_time = 0.0;
    double stats_timer = 0.0;
    uint32_t last_c0_draws = 1;
    uint32_t last_c1_strips = 0;
    uint32_t last_c2_strips = 0;
    uint32_t last_c3_strips = 0;

    // ── 阴影深度 bias（背面渲染的贴合补偿，运行时可调）──
    CascadedShadowConfig csm_config{};
    float cascade_radius0 = 0.0f; // 级联 0 包围球半径（供上层换算世界单位用）
    // 各级联正交投影的深度范围（米），由 RenderCSM 里那次真实 Update 记录；
    // bias_world 除以它即得该级需要写入的归一化 bias
    float cascade_depth_range[kMaxShadowCascades]{};
    bool bias_key_prev[2]{};      // [0]=[ 减小, [1]=] 增大，边沿触发
    bool no_key_prev[2]{};        // [0]=- 减小, [1]== 增大（normal-offset），边沿触发

private:

    bool InitTextures()
    {
        auto *texture_manager = GetManager<TextureManager>();
        if (!texture_manager)
            return false;

        auto BuildFilePair = [](const OSString &folder, OSString &base, OSString &normal) -> bool
        {
            base = filesystem::JoinPathWithFilename(folder, OS_TEXT("baseColor.Tex2D"));
            normal = filesystem::JoinPathWithFilename(folder, OS_TEXT("normal.Tex2D"));
            if (!filesystem::FileExist(normal))
                normal = filesystem::JoinPathWithFilename(folder, OS_TEXT("Normal.Tex2D"));

            return filesystem::FileExist(base) && filesystem::FileExist(normal);
        };

        OSString first_folder = filesystem::JoinPathWithFilename(OS_TEXT("res/image/pbr"), PBR_FOLDER_NAME[0]);
        OSString first_base, first_normal;
        if (!BuildFilePair(first_folder, first_base, first_normal))
        {
            GLogError("InitTextures: Failed to find texture pair for folder[0]");
            return false;
        }

        Texture2D *probe_base = texture_manager->LoadTexture2D(first_base, true);
        Texture2D *probe_normal = texture_manager->LoadTexture2D(first_normal, true);
        if (!probe_base || !probe_normal)
        {
            GLogError("InitTextures: Failed to load probe textures");
            return false;
        }

        base_color_texture = texture_manager->CreateTexture2DArray("csm_pbr_baseColor_array",
                                                                   probe_base->GetWidth(),
                                                                   probe_base->GetHeight(),
                                                                   kPBRTextureCount,
                                                                   probe_base->GetFormat(),
                                                                   probe_base->GetMipLevel());
        normal_texture = texture_manager->CreateTexture2DArray("csm_pbr_normal_array",
                                                               probe_normal->GetWidth(),
                                                               probe_normal->GetHeight(),
                                                               kPBRTextureCount,
                                                               probe_normal->GetFormat(),
                                                               probe_normal->GetMipLevel());

        SAFE_CLEAR(probe_base)
        SAFE_CLEAR(probe_normal)

        if (!base_color_texture || !normal_texture)
            return false;

        for (uint32_t layer = 0; layer < kPBRTextureCount; ++layer)
        {
            OSString folder = filesystem::JoinPathWithFilename(OS_TEXT("res/image/pbr"), PBR_FOLDER_NAME[layer]);
            OSString base_file, normal_file;
            if (BuildFilePair(folder, base_file, normal_file))
            {
                texture_manager->LoadTexture2DArray(base_color_texture, layer, base_file);
                texture_manager->LoadTexture2DArray(normal_texture, layer, normal_file);
            }
        }

        auto *sampler_manager = GetManager<SamplerManager>();
        pbr_sampler = sampler_manager ? sampler_manager->CreateSampler() : nullptr;

        return pbr_sampler != nullptr;
    }

    bool InitMaterial()
    {
        lit_recipe.recipe_name = "CascadeShadowMap.Lit";
        lit_recipe.mtl_def_id = "Lit";
        lit_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();

        auto *domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        for (uint32_t i = 0; i < kPBRTextureCount; ++i)
        {
            material_accessors[i] = domain_manager->GetAccessor<ssbo::PBRSurfaceRow>();
            if (!material_accessors[i])
                return false;

            ssbo::PBRSurfaceRow row{};
            row.base_color = Color4f(0.85f, 0.85f, 0.85f, 1.0f);
            row.metallic = 0.05f + 0.1f * static_cast<float>(i % 5);
            row.roughness = 0.2f + 0.08f * static_cast<float>(i);
            row.normal_scale = 0.5f;

            if (!material_accessors[i].Write(row))
                return false;
        }

        ground_accessor = domain_manager->GetAccessor<ssbo::PBRSurfaceRow>();
        if (!ground_accessor)
            return false;

        ssbo::PBRSurfaceRow ground_row{};
        ground_row.base_color = Color4f(0.6f, 0.6f, 0.62f, 1.0f);
        ground_row.metallic = 0.02f;
        ground_row.roughness = 0.85f;
        ground_row.normal_scale = 0.4f;
        ground_accessor.Write(ground_row);

        lit_recipe.material_ssbo_binding = material_accessors[0].GetGlobalSSBOBinding();
        return lit_recipe.material_ssbo_binding.IsValid();
    }

    bool InitVDM()
    {
        auto *buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return false;

        vdm = new VertexDataManager(buffer_manager, CreateStandardTextureArrayGeometryVertexFormat());
        if (!vdm || !vdm->Init(HGL_SIZE_1MB * 4, HGL_SIZE_1MB * 4, IndexType::U32))
            return false;

        return true;
    }

    bool CreateGeometries()
    {
        using namespace inline_geometry;

        auto create_geom = [this](auto &&creator) -> Geometry *
        {
            auto pc = std::make_unique<GeometryCreater>(vdm);
            return pc ? creator(pc.get()) : nullptr;
        };

        builtin_geometries[0] = create_geom([](GeometryCreater *pc) { return CreateSphere(pc, 64); });
        builtin_geometries[1] = create_geom([](GeometryCreater *pc) { return CreateDome(pc, 64); });
        builtin_geometries[2] = create_geom([](GeometryCreater *pc)
        {
            ConeCreateInfo cci;
            cci.radius = 1.0f;
            cci.halfExtend = 1.0f;
            cci.numberSlices = 64;
            cci.numberStacks = 4;
            return CreateCone(pc, &cci);
        });
        builtin_geometries[3] = create_geom([](GeometryCreater *pc)
        {
            CylinderCreateInfo cci;
            cci.radius = 1.0f;
            cci.halfExtend = 1.0f;
            cci.numberSlices = 32;
            return CreateCylinder(pc, &cci);
        });
        builtin_geometries[4] = create_geom([](GeometryCreater *pc)
        {
            TorusCreateInfo tci;
            tci.innerRadius = 0.65f;
            tci.outerRadius = 1.25f;
            tci.numberSlices = 96;
            tci.numberStacks = 24;
            return CreateTorus(pc, &tci);
        });
        builtin_geometries[5] = create_geom([](GeometryCreater *pc)
        {
            HollowCylinderCreateInfo hcci;
            hcci.halfExtend = 1.0f;
            hcci.innerRadius = 0.6f;
            hcci.outerRadius = 1.0f;
            hcci.numberSlices = 64;
            return CreateHollowCylinder(pc, &hcci);
        });
        builtin_geometries[6] = create_geom([](GeometryCreater *pc)
        {
            HexSphereCreateInfo hsci;
            hsci.subdivisions = 3;
            return CreateHexSphere(pc, &hsci);
        });
        builtin_geometries[7] = create_geom([](GeometryCreater *pc)
        {
            CapsuleCreateInfo cci;
            return CreateCapsule(pc, &cci);
        });
        builtin_geometries[8] = create_geom([](GeometryCreater *pc)
        {
            TaperedCapsuleCreateInfo tcci;
            tcci.topRadius = 0.25f;
            return CreateTaperedCapsule(pc, &tcci);
        });
        builtin_geometries[9] = create_geom([](GeometryCreater *pc)
        {
            CubeCreateInfo cci;
            cci.segments_x = 1;
            cci.segments_y = 1;
            cci.segments_z = 1;
            return CreateCube(pc, &cci);
        });

        for (uint32_t i = 0; i < kBuiltinGeomCount; ++i)
        {
            if (!builtin_geometries[i])
                return false;
            builtin_primitives[i] = PrimitiveAsset(builtin_geometries[i], &lit_recipe, PrimitiveType::Triangles);
        }

        ground_geometry = create_geom([](GeometryCreater *pc)
        {
            return CreatePlaneSqaure(pc);
        });
        if (!ground_geometry)
            return false;

        ground_primitive = PrimitiveAsset(ground_geometry, &lit_recipe, PrimitiveType::Triangles);
        return ground_primitive.IsValid();
    }

    bool InitCSMTargets()
    {
        if (!environment_system)
            return false;

        CascadedShadowConfig &cfg = csm_config;
        cfg.cascade_count = 4;
        cfg.split_distances[0] = 50.0f;  // CSM 0 (全动态近距，每帧重绘): 0.1m ~ 50.0m
        cfg.split_distances[1] = 50.0f;  // CSM 1 (静态近+中距，与CSM 0重叠覆盖，滚动更新): 0.1m ~ 50.0m
        cfg.split_distances[2] = 160.0f; // CSM 2 (静态远景，不重叠，滚动更新): 50.0m ~ 160.0m
        cfg.split_distances[3] = 300.0f; // CSM 3 (静态超远景，不重叠，滚动更新): 160.0m ~ 300.0m
        cfg.max_distance = 300.0f;
        cfg.use_custom_splits = true;
        cfg.c0_dynamic_overlay = true;  // 启用动静分层模式
        cfg.shadow_map_size = static_cast<float>(kShadowMapSize);
        cfg.caster_depth_margin = 120.0f;
        cfg.bias = -0.003f;                    // 仅在 bias_world == 0 时生效（历史行为）
        cfg.bias_world = kShadowBiasWorld;     // 见文件头 kShadowBiasWorld
        cfg.normal_offset_world = kShadowNormalOffsetWorld; // 见文件头同名常量
        cfg.pcf_radius = 1.5f;
        cfg.darkness = 0.15f;
        cfg.blend_width = 0.05f;

        GLogInfo(u8"[CSM] shadow bias_world=%.2fm normal_offset=%.2fm (back-face shadow map; press [ / ] and - / = to tune)",
                 cfg.bias_world, cfg.normal_offset_world);

        return environment_system->EnableMainLightShadow(cfg, kShadowMapSize);
    }

    bool PopulateWorld()
    {
        if (!ecs_context)
            return false;

        // 1. 创建地面实体（Static，超大范围，网格随相机平铺对齐）
        ground_entity = ecs_context->CreateEntity<Entity>("InfiniteGround");
        ground_transform = ground_entity->AddComponent<TransformComponent>(Mobility::Static);
        ground_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        ground_transform->SetLocalScale(glm::vec3(kGroundExtent, kGroundExtent, 1.0f));

        ground_prim = ground_entity->AddComponent<PrimitiveComponent>();
        ground_prim->SetPrimitiveAsset(&ground_primitive);
        ground_prim->SetMaterialTextureResource("base_color", base_color_texture, pbr_sampler,
            PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray, "", 0); // Concrete_Plain
        ground_prim->SetMaterialTextureResource("normal", normal_texture, pbr_sampler,
            PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray, "", 0);
        ground_prim->SetMaterialDataResource(ground_accessor.GetGlobalSSBOBinding());
        ground_prim->SetVisible(true);

        auto ground_shadow = ground_entity->AddComponent<ShadowComponent>();
        ground_shadow->SetCastShadow(false); // 规范化声明：地面不投射阴影，防止自遮挡

        // 2. 随机分布 100 个几何体覆盖 200m 纵深
        for (uint32_t i = 0; i < kTotalObjectCount; ++i)
        {
            const bool is_movable = (i < kMovableCount);
            const Mobility mobility = is_movable ? Mobility::Movable : Mobility::Static;

            float x = 0.0f;
            float y = 0.0f;

            if (is_movable)
            {
                // 近景动态物体分布在原点与初始相机前方附近 [-25, 25]
                x = (Hash01(i, 0, 101u) * 2.0f - 1.0f) * 25.0f;
                y = (Hash01(i, 1, 203u) * 2.0f - 1.0f) * 25.0f;
            }
            else
            {
                // 静态物体全场景覆盖：横向 [-85, 85]，纵向覆盖近距到远景 [-20, 180]
                x = (Hash01(i, 0, 307u) * 2.0f - 1.0f) * 85.0f;
                y = Hash01(i, 1, 409u) * 200.0f - 20.0f;

                // 避开玩家初始诞生点 (0, -28)
                if (glm::distance(glm::vec2(x, y), glm::vec2(0.0f, -28.0f)) < 6.0f)
                    x += 12.0f;
            }

            const uint32_t geom_idx = HashU32(i, 2, 521u) % kBuiltinGeomCount;
            const uint32_t tex_idx  = HashU32(i, 3, 613u) % kPBRTextureCount;
            const uint32_t mat_idx  = HashU32(i, 4, 727u) % kPBRTextureCount;

            glm::vec3 scale(1.0f);
            if (!is_movable && (geom_idx == 3 || geom_idx == 9) && Hash01(i, 5, 839u) > 0.6f)
            {
                // 部分静态柱体/方块拉高为高塔（长阴影地标）
                const float h = 6.0f + Hash01(i, 6, 953u) * 10.0f;
                scale = glm::vec3(1.8f, 1.8f, h);
            }
            else
            {
                const float s = 1.2f + Hash01(i, 5, 839u) * 2.2f;
                scale = glm::vec3(s);
            }

            const float lift = GroundLift(builtin_geometries[geom_idx]) * scale.z;
            const glm::vec3 pos(x, y, lift);

            AnsiString name = (is_movable ? "Movable_" : "Static_") + AnsiString::numberOf(i);
            Entity *e = ecs_context->CreateEntity<Entity>(name.c_str());

            auto tf = e->AddComponent<TransformComponent>(mobility);
            tf->SetLocalPosition(pos);
            tf->SetLocalScale(scale);

            // 静态物体生成固定朝向，动态物体记录初始动画轨迹
            const float rx = Hash01(i, 7, 1061u) * 6.283f;
            const float ry = Hash01(i, 8, 1171u) * 6.283f;
            const float rz = Hash01(i, 9, 1283u) * 6.283f;
            const glm::quat initial_rot = glm::quat(glm::vec3(rx * 0.1f, ry * 0.1f, rz));
            tf->SetLocalRotation(initial_rot);

            if (is_movable)
            {
                auto &track = movable_tracks[i];
                track.transform = tf;
                track.base_pos = pos;

                glm::vec3 axis(Hash01(i, 10, 1399u) * 2.0f - 1.0f,
                               Hash01(i, 11, 1487u) * 2.0f - 1.0f,
                               0.6f + Hash01(i, 12, 1597u) * 0.8f);
                track.spin_axis = glm::normalize(axis);
                track.spin_speed = (0.5f + Hash01(i, 13, 1693u) * 1.5f) * (Hash01(i, 14, 1789u) > 0.5f ? 1.0f : -1.0f);

                track.orbit_radius = (i % 3 == 0) ? (2.0f + Hash01(i, 15, 1889u) * 3.5f) : 0.0f;
                track.orbit_speed = 0.6f + Hash01(i, 16, 1993u) * 0.8f;
                track.orbit_phase = Hash01(i, 17, 2099u) * 6.283f;

                track.hover_amplitude = (i % 2 == 0) ? (0.6f + Hash01(i, 18, 2203u) * 1.2f) : 0.0f;
                track.hover_speed = 1.2f + Hash01(i, 19, 2309u) * 1.5f;
            }

            auto prim = e->AddComponent<PrimitiveComponent>();
            prim->SetPrimitiveAsset(&builtin_primitives[geom_idx]);
            prim->SetMaterialTextureResource("base_color", base_color_texture, pbr_sampler,
                PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray, "", tex_idx);
            prim->SetMaterialTextureResource("normal", normal_texture, pbr_sampler,
                PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray, "", tex_idx);
            prim->SetMaterialDataResource(material_accessors[mat_idx].GetGlobalSSBOBinding());
            prim->SetVisible(true);
        }

        return true;
    }

    bool SetupCameras()
    {
        if (!ecs_context->EnsureCameraSystem())
            return false;

        camera_system = ecs_context->GetSystem<CameraSystem>();
        if (!camera_system)
            return false;

        main_camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        main_camera = main_camera_entity->AddComponent<CameraComponent>();
        main_camera->is_main_camera = true;
        main_camera->control_mode = CameraComponent::ControlMode::FirstPerson;
        main_camera->position = math::Vector3f(0.0f, -28.0f, 10.0f);
        main_camera->target = math::Vector3f(0.0f, 0.0f, 3.0f);
        main_camera->world_up = math::Vector3f(0.0f, 0.0f, 1.0f);
        main_camera->pitch = -12.0f;
        main_camera->yaw = 90.0f;
        main_camera->fov = 60.0f;
        main_camera->near_plane = 0.1f;
        main_camera->far_plane = 500.0f;
        main_camera->move_speed = 22.0f;
        main_camera->rotation_sensitivity = 0.18f;

        camera_system->Update(0.0f);
        if (auto *main_rt = ecs_context->GetRenderTarget())
            camera_system->SetViewportInfo(main_rt->GetViewportInfo());

        return true;
    }

    void UpdateMovableAnimation(float t)
    {
        for (uint32_t i = 0; i < kMovableCount; ++i)
        {
            auto &track = movable_tracks[i];
            if (!track.transform)
                continue;

            glm::vec3 pos = track.base_pos;
            if (track.orbit_radius > 0.0f)
            {
                const float angle = track.orbit_phase + t * track.orbit_speed;
                pos.x += std::cos(angle) * track.orbit_radius;
                pos.y += std::sin(angle) * track.orbit_radius;
            }
            if (track.hover_amplitude > 0.0f)
            {
                pos.z += std::sin(t * track.hover_speed) * track.hover_amplitude;
            }

            const glm::quat spin = glm::angleAxis(t * track.spin_speed, track.spin_axis);
            track.transform->SetLocalPosition(pos);
            track.transform->SetLocalRotation(spin);
        }

        // 无限平铺地表：地面中心网格吸附（Grid Snapping）至主相机 XY
        if (ground_transform && main_camera)
        {
            constexpr float kSnapGrid = 10.0f;
            const float gx = std::floor(main_camera->position.x / kSnapGrid) * kSnapGrid;
            const float gy = std::floor(main_camera->position.y / kSnapGrid) * kSnapGrid;
            ground_transform->SetLocalPosition(glm::vec3(gx, gy, 0.0f));
        }
    }

    /// 由世界偏移换算某级联的归一化 bias（打印用；depth_range<=0 时返回 0）
    static float ResolvedBiasOf(float bias_world, float depth_range)
    {
        return (depth_range > 0.0f) ? (bias_world / depth_range) : 0.0f;
    }

    /// 深度 bias 运行时微调：`[` 推向正偏移（更漏光）/ `]` 推向负偏移（更贴合），
    /// 边沿触发。bias_world 非 0 时按米调，否则回退调归一化 bias。
    void TuneShadowBias()
    {
        if (!ecs_context)
            return;

        auto input_system = ecs_context->GetSystem<InputSystem>();
        if (!input_system)
            return;

        const bool key_dec = input_system->IsKeyDown(io::KeyboardButton::LeftBracket);
        const bool key_inc = input_system->IsKeyDown(io::KeyboardButton::RightBracket);

        const bool use_world = (csm_config.bias_world != 0.0f);
        const float kStep = use_world ? kShadowTuneStepWorld : 0.0005f;
        const float kMin = use_world ? -8.0f : -0.02f;
        const float kMax = use_world ? 2.0f : 0.01f;

        float new_bias = use_world ? csm_config.bias_world : csm_config.bias;
        if (key_dec && !bias_key_prev[0])
            new_bias -= kStep;
        if (key_inc && !bias_key_prev[1])
            new_bias += kStep;

        bias_key_prev[0] = key_dec;
        bias_key_prev[1] = key_inc;

        if (new_bias < kMin)
            new_bias = kMin;
        if (new_bias > kMax)
            new_bias = kMax;

        // bias_world 模式下不要被 0 卡住（0 代表"退回归一化 bias"）
        if (use_world)
        {
            if (new_bias == csm_config.bias_world)
                return;
            csm_config.bias_world = new_bias;
        }
        else
        {
            if (new_bias == csm_config.bias)
                return;
            csm_config.bias = new_bias;
        }

        if (environment_system)
        {
            if (auto *ctrl = environment_system->GetShadowController())
                ctrl->SetConfig(csm_config);
        }

        // 只改接收者侧的比较基准，滚动缓存里的深度仍然有效，无需重建级联。
        // 用上一帧真实 Update 记录的深度范围直接换算，**不能**在这里再调一次
        // csm_controller.Update 取数：那会把缓存的 snapped_origin 提前推进，
        // 下一帧就会拿旧贴图当新中心用（静态阴影滑动）。
        const float w = csm_config.bias_world;
        if (w != 0.0f)
        {
            GLogInfo(u8"[Shadow Bias] bias_world=%.2fm normalized=[%.5f %.5f %.5f %.5f] (per-cascade depth range=[%.0f %.0f %.0f %.0f]m)",
                     w,
                     ResolvedBiasOf(w, cascade_depth_range[0]), ResolvedBiasOf(w, cascade_depth_range[1]),
                     ResolvedBiasOf(w, cascade_depth_range[2]), ResolvedBiasOf(w, cascade_depth_range[3]),
                     cascade_depth_range[0], cascade_depth_range[1], cascade_depth_range[2], cascade_depth_range[3]);
        }
        else
        {
            GLogInfo(u8"[Shadow Bias] normalized bias=%.5f (bias_world disabled)", csm_config.bias);
        }

        GLogInfo(u8"[Shadow Bias] negative=shadows hug the caster (crisper), positive=shadows detach toward the light (light leak)");
    }

    /// Normal-offset 强度运行时微调：`-` 减小 / `=` 增大（米），边沿触发。
    /// 与深度 bias 不同，这里调的是**接收者采样位置**，同样不需要重建级联缓存。
    void TuneShadowNormalOffset()
    {
        if (!ecs_context)
            return;

        auto input_system = ecs_context->GetSystem<InputSystem>();
        if (!input_system)
            return;

        const bool key_dec = input_system->IsKeyDown(io::KeyboardButton::Minus);
        const bool key_inc = input_system->IsKeyDown(io::KeyboardButton::Equals);

        constexpr float kStep = kShadowTuneStepWorld;
        constexpr float kMax = 4.0f;

        float new_offset = csm_config.normal_offset_world;
        if (key_dec && !no_key_prev[0])
            new_offset -= kStep;
        if (key_inc && !no_key_prev[1])
            new_offset += kStep;

        no_key_prev[0] = key_dec;
        no_key_prev[1] = key_inc;

        if (new_offset < 0.0f)
            new_offset = 0.0f;
        if (new_offset > kMax)
            new_offset = kMax;

        if (new_offset == csm_config.normal_offset_world)
            return;

        csm_config.normal_offset_world = new_offset;
        if (environment_system)
        {
            if (auto *ctrl = environment_system->GetShadowController())
                ctrl->SetConfig(csm_config);
        }

        const char *state = (new_offset > 0.0f) ? "on" : "off";
        GLogInfo(u8"[Shadow Normal Offset] strength=%.2fm (%s) - tan(theta) weighted, clamped to 1.50m",
                 new_offset, state);
    }

public:
    ~CascadeShadowMapApp() override
    {
        for (uint32_t i = 0; i < kBuiltinGeomCount; ++i)
        {
            SAFE_CLEAR(builtin_geometries[i])
        }
        SAFE_CLEAR(ground_geometry)
        SAFE_CLEAR(vdm)
        SAFE_CLEAR(base_color_texture)
        SAFE_CLEAR(normal_texture)

        if (auto *gc = GetGraphicsContext())
        {
            if (auto *sm = gc->GetManager<SamplerManager>())
            {
                if (pbr_sampler) sm->Release(pbr_sampler);
            }
        }
        pbr_sampler = nullptr;
    }

    void Tick(double delta) override
    {
        WorkObject::Tick(delta);

        elapsed_time += delta;
        stats_timer += delta;

        UpdateMovableAnimation(static_cast<float>(elapsed_time));

        TuneShadowBias();
        TuneShadowNormalOffset();

        if (stats_timer >= 1.0)
        {
            const bool stationary_c13 = (last_c1_strips == 0 && last_c2_strips == 0 && last_c3_strips == 0);
            GLogInfo(u8"[CSM Rolling Cache Stats] Cam=(%.1f, %.1f, %.1f) | C1=%u strips | C2=%u strips | C3=%u strips | Mid/Far Status: %s",
                     main_camera->position.x, main_camera->position.y, main_camera->position.z,
                     last_c1_strips, last_c2_strips, last_c3_strips,
                     stationary_c13 ? u8"100% Cached (ZERO DrawCalls!)" : u8"Incremental Rolling Updating");

            stats_timer = 0.0;
        }
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.12f, 0.12f, 0.14f, 1.0f));

        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        ecs_context->SetResourceNamePrefix("CascadeShadowMap:MainScene");

        // 显式声明场景工作流模式（黄金路径：标准 3D 陆地主光级联阴影）
        ecs_context->SetScenePipelineMode(ScenePipelineMode::StandardLitCSM);

        environment_system = ecs_context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<EnvironmentSystem>();

        if (!environment_system)
            return false;

        sky_info = environment_system->EditSkyInfo();
        if (!sky_info)
            return false;

        sun_direction = glm::normalize(glm::vec3(0.5f, 0.6f, 0.8f));
        light_dir_math = math::Vector3f(-sun_direction.x, -sun_direction.y, -sun_direction.z);
        sky_info->sun_direction = math::Vector4f(sun_direction.x, sun_direction.y, sun_direction.z, 0.0f);
        sky_info->SetTime(9, 30, 0);
        environment_system->MarkSkyDirty();

        if (!InitTextures())
            return false;
        if (!InitMaterial())
            return false;
        if (!InitVDM())
            return false;
        if (!CreateGeometries())
            return false;
        if (!InitCSMTargets())
            return false;
        if (!PopulateWorld())
            return false;
        if (!SetupCameras())
            return false;

        GLogInfo(u8"=== CascadeShadowMapApp initialized successfully (100 objects, 4 cascades automated pipeline) ===");
        return true;
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<CascadeShadowMapApp>(
        OS_TEXT("Cascaded Shadow Map (CSM Rolling Toroidal Cache & Mobility Stream)"),
        argc, argv, 1600, 900);
}
