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
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/components/PrimitiveComponent.h>
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

    // ── CSM 级联阴影控制器与离屏 RT ──
    CascadedShadowController csm_controller;
    graph::RenderTargetHandle cascade_rts[kMaxShadowCascades]{};
    uint32_t cascade_handles[kMaxShadowCascades]{};

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

    // 调试开关：屏蔽 CSM 0（动态近景层），用于对比静态层 CSM 1..3 的覆盖效果
    // 按住 F1 屏蔽 CSM 0，松开恢复
    bool debug_mask_cascade0 = false;
    double csm_diag_next_time = 0.0;

    // 性能与条带统计
    double elapsed_time = 0.0;
    double stats_timer = 0.0;
    uint32_t last_c0_draws = 1;
    uint32_t last_c1_strips = 0;
    uint32_t last_c2_strips = 0;
    uint32_t last_c3_strips = 0;

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
        auto *rtm = GetGraphicsContext()->GetRenderTargetManager();
        auto *btm = GetGraphicsContext()->GetBindlessTextureManager();
        if (!rtm || !btm)
            return false;

        CascadedShadowConfig cfg;
        cfg.cascade_count = 4;
        cfg.split_distances[0] = 35.0f;  // CSM 0 (全动态近距，每帧重绘): 0.1m ~ 35.0m
        cfg.split_distances[1] = 80.0f;  // CSM 1 (静态近+中距，与CSM 0重叠覆盖，滚动更新): 0.1m ~ 80.0m
        cfg.split_distances[2] = 160.0f; // CSM 2 (静态远景，不重叠，滚动更新): 80.0m ~ 160.0m
        cfg.split_distances[3] = 300.0f; // CSM 3 (静态超远景，不重叠，滚动更新): 160.0m ~ 300.0m
        cfg.max_distance = 300.0f;
        cfg.use_custom_splits = true;
        cfg.c0_dynamic_overlay = true;  // 启用动静分层模式
        cfg.shadow_map_size = static_cast<float>(kShadowMapSize);
        cfg.caster_depth_margin = 120.0f;
        cfg.bias = 0.0003f;
        cfg.pcf_radius = 1.5f;
        cfg.darkness = 0.15f;
        cfg.blend_width = 0.05f;
        csm_controller.SetConfig(cfg);

        for (uint32_t c = 0; c < kMaxShadowCascades; ++c)
        {
            RenderTargetDesc desc = RenderTargetDesc::OffscreenDepthOnly(
                kShadowMapSize, kShadowMapSize,
                AnsiString("CSM_Cascade_") + AnsiString::numberOf(c),
                PF_D32F);

            cascade_rts[c] = rtm->Create(desc);
            if (!cascade_rts[c] || !cascade_rts[c]->hasDepth())
                return false;

            auto *depth_tex = cascade_rts[c]->GetDepthTexture();
            if (!depth_tex)
                return false;

            cascade_handles[c] = btm->RegisterTexture(depth_tex);
            if (cascade_handles[c] == 0)
                return false;

            csm_controller.SetCascadeTexture(c, cascade_handles[c], 0);
        }

        return true;
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

        auto *light_entity = ecs_context->CreateEntity<Entity>("CSMLightCamera");
        light_camera = light_entity->AddComponent<CameraComponent>();
        light_camera->is_main_camera = false;

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

    void RenderCSM()
    {
        if (!ecs_context || !environment_system || !main_camera || !light_camera)
            return;

        auto *shadow_info = environment_system->EditShadowInfo();
        if (!shadow_info)
            return;

        // 提取主相机当前数据
        graph::Camera main_cam;
        main_cam.pos = main_camera->position;
        main_cam.viewDirection = main_camera->forward;
        main_cam.world_up = main_camera->world_up;
        main_cam.fovY = main_camera->fov > 0.0f ? main_camera->fov : 60.0f;
        main_cam.znear = main_camera->near_plane > 0.0f ? main_camera->near_plane : 0.1f;
        main_cam.zfar = main_camera->far_plane > 0.0f ? main_camera->far_plane : 500.0f;

        const auto *vp = main_camera->viewport_info;
        const float aspect = (vp && vp->GetViewportHeight() > 0)
            ? vp->GetAspectRatio()
            : (16.0f / 9.0f);

        CascadeUpdateResult updates[kMaxShadowCascades];
        csm_controller.Update(main_cam, aspect, light_dir_math, *shadow_info, updates);

        // 调试：屏蔽 CSM 0 时，让着色器把第 0 级视为未绑定（动态层恒为受光）
        if (debug_mask_cascade0)
        {
            shadow_info->cascades[0].shadow_tex = Vector4u(0);
            updates[0].need_full_update = false;
            updates[0].ClearDirtyRects();
        }

        // 确保非零以激活 shader 级联分支
        shadow_info->shadow_tex.x = debug_mask_cascade0 ? cascade_handles[1] : cascade_handles[0];
        environment_system->MarkShadowDirty();

        // ==== TEMP DIAG: 核对送往 GPU 的 ShadowInfo 与近景探针的选级结果 ====
        if (elapsed_time >= csm_diag_next_time)
        {
            csm_diag_next_time = elapsed_time + 2.0;

            GLogInfo(u8"[CSMDIAG] mask_c0=%d csm_params=(%u,%u,%u,%u) handles=(%u,%u,%u,%u) shadow_tex.x=%u",
                     debug_mask_cascade0 ? 1 : 0,
                     shadow_info->csm_params.x, shadow_info->csm_params.y,
                     shadow_info->csm_params.z, shadow_info->csm_params.w,
                     cascade_handles[0], cascade_handles[1], cascade_handles[2], cascade_handles[3],
                     shadow_info->shadow_tex.x);

            const math::Vector3f cam_pos(main_cam.pos.x, main_cam.pos.y, main_cam.pos.z);
            const math::Vector3f cam_fwd = main_cam.viewDirection;

            for (uint32_t c = 0; c < kMaxShadowCascades; ++c)
            {
                const auto &casc = shadow_info->cascades[c];
                GLogInfo(u8"[CSMDIAG]   C%u tex.x=%u split=[%.2f,%.2f] blend=%.3f map=%.0f snap_origin=(%.2f,%.2f) texel=%.4f cache_off=(%u,%u)",
                         c, casc.shadow_tex.x,
                         casc.cascade_params.x, casc.cascade_params.y, casc.cascade_params.z,
                         casc.shadow_map_size.x,
                         casc.cache_origin.x, casc.cache_origin.y, casc.cache_origin.z,
                         casc.cache_offset.x, casc.cache_offset.y);
            }

            const float probe_d[6] = { 3.0f, 10.0f, 20.0f, 30.0f, 45.0f, 70.0f };
            const float fwd_len = (Length(cam_fwd) > 1.0e-6f) ? Length(cam_fwd) : 1.0f;

            for (uint32_t i = 0; i < 6; ++i)
            {
                // 沿相机前向投射到地面 (z = 0)
                const float along = probe_d[i];
                math::Vector3f probe = cam_pos + cam_fwd * along;
                probe.z = 0.0f;

                const float vd = Dot(probe - cam_pos, cam_fwd) / fwd_len;

                float z[4], u[4], v[4];
                int   ok[4];
                for (uint32_t c = 0; c < kMaxShadowCascades; ++c)
                {
                    const math::Vector4f lc = shadow_info->cascades[c].shadow_vp * math::Vector4f(probe, 1.0f);
                    ok[c] = (lc.w > 1.0e-6f) ? 1 : 0;
                    z[c] = ok[c] ? (lc.z / lc.w) : 0.0f;
                    u[c] = ok[c] ? (0.5f + 0.5f * lc.x / lc.w) : 0.0f;
                    v[c] = ok[c] ? (0.5f + 0.5f * lc.y / lc.w) : 0.0f;
                }

                GLogInfo(u8"[CSMDIAG]   probe d=%.1f vd=%.2f | C0 %s z=%.4f uv=(%.3f,%.3f) | C1 %s z=%.4f uv=(%.3f,%.3f) | C2 %s z=%.4f uv=(%.3f,%.3f) | C3 %s z=%.4f uv=(%.3f,%.3f)",
                         along, vd,
                         ok[0] ? "in " : "out", z[0], u[0], v[0],
                         ok[1] ? "in " : "out", z[1], u[1], v[1],
                         ok[2] ? "in " : "out", z[2], u[2], v[2],
                         ok[3] ? "in " : "out", z[3], u[3], v[3]);
            }
        }

        // 避免地面自身在阴影贴图中写入深度导致阴影自遮挡
        if (ground_prim)
            ground_prim->SetVisible(false);

        last_c0_draws = 1;
        last_c1_strips = updates[1].need_full_update ? 1 : updates[1].dirty_rect_count;
        last_c2_strips = updates[2].need_full_update ? 1 : updates[2].dirty_rect_count;
        last_c3_strips = updates[3].need_full_update ? 1 : updates[3].dirty_rect_count;

        for (uint32_t c = 0; c < kMaxShadowCascades; ++c)
        {
            const auto &res = updates[c];
            auto *rt = cascade_rts[c].get();
            if (!rt)
                continue;

            if (c == 0 && debug_mask_cascade0)
                continue; // 调试：跳过 CSM 0 渲染

            if (c == 0 || res.need_full_update)
            {
                // 全量重绘
                RenderPassRequest req;
                req.target = rt;
                req.camera = light_camera.get();
                light_camera->custom_matrices = true;
                light_camera->custom_view = res.light_view;
                light_camera->custom_projection = res.light_proj;
                req.load_depth = false;
                req.use_scissor = false;
                req.clear_scissor_depth = false;
                // 级联 0 仅收集动态物体（Movable）；中远景级联仅收集静态物体（Static）
                req.mobility_filter = (c == 0) ? static_cast<int>(Mobility::Movable) : static_cast<int>(Mobility::Static);

                ecs_context->RenderTo(req);
            }
            else if (res.dirty_rect_count > 0)
            {
                // 滚动增量条带更新
                for (uint32_t r = 0; r < res.dirty_rect_count; ++r)
                {
                    const auto &rect = res.dirty_rects[r];
                    RenderPassRequest req;
                    req.target = rt;
                    req.camera = light_camera.get();
                    light_camera->custom_matrices = true;
                    light_camera->custom_view = res.light_view;
                    light_camera->custom_projection = res.light_proj;
                    req.load_depth = true; // 保留旧深度图内容
                    req.use_scissor = true;
                    req.scissor.offset = { static_cast<int32_t>(rect.x), static_cast<int32_t>(rect.y) };
                    req.scissor.extent = { rect.width, rect.height };
                    req.clear_scissor_depth = true; // 局部清空条带区域（Reversed-Z 远平面 0.0f）
                    req.mobility_filter = static_cast<int>(Mobility::Static); // 仅收集静态物体

                    ecs_context->RenderTo(req);
                }
            }
            else
            {
                // 静止命中：0 DrawCall！
            }
        }

        if (ground_prim)
            ground_prim->SetVisible(true);
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

        // 调试：按住 F1 屏蔽 CSM 0（仅静态层 CSM 1..3 生效），松开恢复
        if (ecs_context)
        {
            auto input_system = ecs_context->GetSystem<InputSystem>();
            if (input_system)
                debug_mask_cascade0 = input_system->IsKeyDown(io::KeyboardButton::F1);
        }

        RenderCSM();

        if (stats_timer >= 1.0)
        {
            const bool stationary_c13 = (last_c1_strips == 0 && last_c2_strips == 0 && last_c3_strips == 0);
            GLogInfo(u8"[CSM Rolling Cache Stats] C0=%s | Cam=(%.1f, %.1f, %.1f) | C1=%u strips | C2=%u strips | C3=%u strips | Mid/Far Status: %s",
                     debug_mask_cascade0 ? u8"MASKED" : u8"Full",
                     main_camera->position.x, main_camera->position.y, main_camera->position.z,
                     last_c1_strips, last_c2_strips, last_c3_strips,
                     stationary_c13 ? u8"100% Cached (ZERO DrawCalls!)" : u8"Incremental Rolling Updating");

            // ==== TEMP DIAG: 核对主渲染时刻着色器实际读到的 camera（= 系统级 CameraInfo / camera_ubo）====
            if (auto cam_sys = ecs_context->GetSystem<CameraSystem>())
            {
                auto *cam_core = cam_sys.get();
                const graph::CameraInfo *ci = cam_core ? cam_core->GetCameraInfo() : nullptr;
                if (ci)
                {
                    GLogInfo(u8"[DIAG] UBOcamera pos=(%.2f,%.2f,%.2f) view_line=(%.3f,%.3f,%.3f) world_pos=(%.2f,%.2f,%.2f) znear=%.3f",
                             ci->pos.x, ci->pos.y, ci->pos.z,
                             ci->view_line.x, ci->view_line.y, ci->view_line.z,
                             ci->camera_world_pos.x, ci->camera_world_pos.y, ci->camera_world_pos.z,
                             ci->znear);
                }
            }

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

        GLogInfo(u8"=== CascadeShadowMapApp initialized successfully (100 objects, 4 cascades rolling cache) ===");
        return true;
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<CascadeShadowMapApp>(
        OS_TEXT("Cascaded Shadow Map (CSM Rolling Toroidal Cache & Mobility Stream)"),
        argc, argv, 1600, 900);
}
