#include <hgl/framework/WorkManager.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/vk/VKTexture.h>
#include <hgl/vk/VertexDataManager.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/render/RenderTargetDesc.h>
#include <hgl/graph/module/RenderTargetManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/module/EnvironmentManager.h>
#include <hgl/graph/ubo/SkyInfo.h>
#include <hgl/graph/ubo/ShadowInfo.h>
#include <hgl/vk/VKBindlessTextureManager.h>
#include <hgl/graph/geo/InlineGeometry.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/render/lighting/CascadedShadowController.h>
#include <hgl/color/Color.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/MaterialRecipe.h>
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
#include <hgl/ecs/systems/render/EnvironmentSystem.h>

#include <glm/glm.hpp>
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

    // ── A1-4 专用最小验证用例 ─────────────────────────────────────────────────
    //
    // 只回答一个问题：alpha test 物体的阴影是否按 opacity_mask 镂空。
    //
    // 场景 = 地面 + 两个悬浮棋盘 cube（同一 alpha_recipe、同一棋盘纹理）：
    //   CubeA  绑定 opacity_mask        → 影子应为棋盘镂空（白格挡光黑格透光）
    //   CubeB  不绑定 opacity_mask      → SampleOptional fallback 1.0
    //                                   → 影子应为实心方影（对照组）
    // 两个 cube 都是 Movable（进 CSM 0 动态层，每帧全量重绘）——不受静态级联
    // 滚动缓存影响，任何一帧都在验证 masked 链路本身。
    //
    // 本体侧：两者都应有镂空（forward alpha test，HGL_ALPHA_TEST 生效）。
    // 若 A/B 影子一致（都实心或都镂空）即为回归。

    constexpr uint32_t kShadowMapSize = 1024;

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
}

class AlphaTestShadowApp final : public WorkObject
{
private:
    ECSContext *ecs_context = nullptr;
    std::shared_ptr<CameraComponent> main_camera;
    std::shared_ptr<EnvironmentSystem> environment_system;

    VertexDataManager *vdm = nullptr;
    Texture2DArray *alpha_base_texture = nullptr;
    Texture2DArray *white_texture = nullptr; // 地面用：纯白，影子落点清晰可读
    Sampler *pbr_sampler = nullptr;

    Geometry *ground_geometry = nullptr;
    PrimitiveAsset ground_primitive{};
    std::shared_ptr<TransformComponent> ground_transform;

    Geometry *cube_geometry = nullptr;
    PrimitiveAsset cube_primitive{};

    graph::mtl::MaterialRecipe alpha_recipe{};
    graph::mtl::MaterialRecipe ground_recipe{}; // 无 alpha_test：地面实心，避免镂空干扰影子判读
    graph::GlobalSSBODataAccessor material_accessor{};

public:
    ~AlphaTestShadowApp() override
    {
        SAFE_CLEAR(ground_geometry)
        SAFE_CLEAR(cube_geometry)
        SAFE_CLEAR(vdm)
        SAFE_CLEAR(alpha_base_texture)
        SAFE_CLEAR(white_texture)

        if (auto *gc = GetGraphicsContext())
        {
            if (auto *sm = gc->GetManager<SamplerManager>())
            {
                if (pbr_sampler) sm->Release(pbr_sampler);
            }
        }
        pbr_sampler = nullptr;
    }

    bool InitTextures()
    {
        auto *texture_manager = GetManager<TextureManager>();
        if (!texture_manager)
            return false;

        // 黑白棋盘（RGBA8 UNORM）：白格 r=255 挡光，黑格 r=0 镂空
        alpha_base_texture = texture_manager->CreateTexture2DArray(
            "alpha_test_baseColor_array", 256, 256, 1,
            VK_FORMAT_R8G8B8A8_UNORM, 1);
        if (!alpha_base_texture)
            return false;

        if (!texture_manager->LoadTexture2DArray(
                alpha_base_texture, 0,
                filesystem::JoinPathWithFilename(
                    OS_TEXT("res/image/pbr/AlphaChecker"), OS_TEXT("baseColor.Tex2D"))))
        {
            GLogError("[AlphaTestShadow] failed to load AlphaChecker/baseColor.Tex2D");
            return false;
        }

        auto *sampler_manager = GetManager<SamplerManager>();
        pbr_sampler = sampler_manager ? sampler_manager->CreateSampler() : nullptr;
        if (!pbr_sampler)
            return false;

        white_texture = texture_manager->CreateTexture2DArray(
            "alpha_test_white_array", 256, 256, 1,
            VK_FORMAT_R8G8B8A8_UNORM, 1);
        if (!white_texture)
            return false;

        return texture_manager->LoadTexture2DArray(
            white_texture, 0,
            filesystem::JoinPathWithFilename(
                OS_TEXT("res/image/pbr/AlphaChecker"), OS_TEXT("white.Tex2D")));
    }

    bool InitMaterial()
    {
        auto *domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        alpha_recipe.recipe_name = "AlphaTestShadow.Lit";
        alpha_recipe.mtl_def_id = "Lit";
        alpha_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        alpha_recipe.render_state_overrides.has_alpha_test = true;
        alpha_recipe.render_state_overrides.alpha_test = true;
        alpha_recipe.render_state_overrides.has_alpha_cutoff = true;
        alpha_recipe.render_state_overrides.alpha_cutoff = 0.5f;

        material_accessor = domain_manager->GetAccessor<ssbo::PBRSurfaceRow>();
        if (!material_accessor)
            return false;

        ssbo::PBRSurfaceRow row{};
        row.base_color = Color4f(0.9f, 0.9f, 0.9f, 1.0f);
        row.metallic = 0.02f;
        row.roughness = 0.5f;
        row.normal_scale = 0.0f;

        if (!material_accessor.Write(row))
            return false;

        alpha_recipe.material_ssbo_binding = material_accessor.GetGlobalSSBOBinding();
        if (!alpha_recipe.material_ssbo_binding.IsValid())
            return false;

        // 地面用同一 Lit 定义但不带 alpha_test——实心渲染，影子判读不被
        // 地面自身的镂空干扰。
        ground_recipe = alpha_recipe;
        ground_recipe.recipe_name = "AlphaTestShadow.Ground";
        ground_recipe.render_state_overrides.has_alpha_test = false;
        ground_recipe.render_state_overrides.alpha_test = false;
        return true;
    }

    bool InitVDM()
    {
        auto *buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return false;

        vdm = new VertexDataManager(buffer_manager, CreateStandardTextureArrayGeometryVertexFormat());
        if (!vdm || !vdm->Init(HGL_SIZE_1MB * 2, HGL_SIZE_1MB * 2, IndexType::U32))
            return false;

        return true;
    }

    bool CreateGeometries()
    {
        using namespace inline_geometry;

        {
            auto pc = std::make_unique<GeometryCreater>(vdm);
            CubeCreateInfo cci;
            cci.segments_x = 1;
            cci.segments_y = 1;
            cci.segments_z = 1;
            cube_geometry = pc ? CreateCube(pc.get(), &cci) : nullptr;
        }
        {
            auto pc = std::make_unique<GeometryCreater>(vdm);
            ground_geometry = pc ? CreatePlaneSqaure(pc.get()) : nullptr;
        }

        if (!cube_geometry || !ground_geometry)
            return false;

        ground_primitive = PrimitiveAsset(ground_geometry, &ground_recipe, PrimitiveType::Triangles);
        cube_primitive = PrimitiveAsset(cube_geometry, &alpha_recipe, PrimitiveType::Triangles);
        return ground_primitive.IsValid() && cube_primitive.IsValid();
    }

    bool CreateScene()
    {
        // 地面：不投射阴影（规范化声明，防自遮挡）
        {
            Entity *e = ecs_context->CreateEntity<Entity>("Ground");
            ground_transform = e->AddComponent<TransformComponent>(Mobility::Static);
            ground_transform->SetLocalScale(glm::vec3(40.0f));

            auto shadow = e->AddComponent<ShadowComponent>();
            shadow->SetCastShadow(false);

            auto prim = e->AddComponent<PrimitiveComponent>();
            prim->SetPrimitiveAsset(&ground_primitive);
            prim->SetMaterialTextureResource("base_color", white_texture, pbr_sampler,
                PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray, "", 0);
            prim->SetMaterialDataResource(material_accessor.GetGlobalSSBOBinding());
            prim->SetVisible(true);
        }

        // CubeA：绑 opacity_mask → 影子应镂空
        // CubeB：不绑 opacity_mask → fallback 1.0 → 影子应实心（对照）
        // 悬空 z=2.6 使影子与本体在地面分离；两 cube 沿 x 并排便于同屏对比
        //
        // DIAG: ATS_ONLY_MASKED=1 只留 MaskedCube——区分"单物体链路问题"
        // 与"同 batch 双物体行交互问题"。
        const bool only_masked = []()
        {
            const char *v = getenv("ATS_ONLY_MASKED");
            return v && v[0] == '1';
        }();
        const bool bind_opacity[2] = { true, false };
        const char *names[2] = { "MaskedCube", "FallbackCube" };

        for (uint32_t i = 0; i < (only_masked ? 1u : 2u); ++i)
        {
            Entity *e = ecs_context->CreateEntity<Entity>(names[i]);

            // Movable：进 CSM 0 动态层（每帧全量重绘），验证不受静态缓存影响
            auto tf = e->AddComponent<TransformComponent>(Mobility::Movable);
            tf->SetLocalPosition(glm::vec3(i == 0 ? -1.6f : 1.6f, 0.0f, 2.6f));
            tf->SetLocalScale(glm::vec3(2.0f));

            auto prim = e->AddComponent<PrimitiveComponent>();
            prim->SetPrimitiveAsset(&cube_primitive);
            prim->SetMaterialTextureResource("base_color", alpha_base_texture, pbr_sampler,
                PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray, "", 0);
            if (bind_opacity[i])
            {
                prim->SetMaterialTextureResource("opacity_mask", alpha_base_texture, pbr_sampler,
                    PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray, "", 0);
            }
            prim->SetMaterialDataResource(material_accessor.GetGlobalSSBOBinding());
            prim->SetVisible(true);
        }

        return true;
    }

    bool SetupCameras()
    {
        if (!ecs_context->EnsureCameraSystem())
            return false;

        auto camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        main_camera = camera_entity->AddComponent<CameraComponent>();
        main_camera->is_main_camera = true;
        main_camera->control_mode = CameraComponent::ControlMode::LookAt;
        main_camera->position = math::Vector3f(0.0f, -7.5f, 5.5f);
        main_camera->target = math::Vector3f(0.0f, 0.0f, 1.2f);
        main_camera->world_up = math::Vector3f(0.0f, 0.0f, 1.0f);
        main_camera->yaw = 90.0f;    // 朝 +y（与 position/target 一致）
        main_camera->pitch = -22.0f; // 俯视：影子投在视野内地面上
        main_camera->fov = 55.0f;
        main_camera->near_plane = 0.1f;
        main_camera->far_plane = 200.0f;
        return true;
    }

    bool InitCSMTargets()
    {
        graph::CascadedShadowConfig cfg;
        cfg.cascade_count = 2;
        cfg.split_distances[0] = 20.0f;
        cfg.split_distances[1] = 40.0f;
        cfg.max_distance = 40.0f;
        cfg.use_custom_splits = true;
        cfg.c0_dynamic_overlay = true;   // C0 动态层（两 cube 所在），C1 静态层（仅地面）
        cfg.shadow_map_size = static_cast<float>(kShadowMapSize);
        cfg.caster_depth_margin = 30.0f;
        cfg.bias_world = -0.10f;
        cfg.normal_offset_world = 0.0f;  // 关闭法线偏移：排除干扰，镂空纯由 alpha 决定
        cfg.pcf_radius = 1.0f;           // 小半径：镂空图案尽量锐利
        cfg.darkness = 0.15f;

        return environment_system->EnableMainLightShadow(cfg, kShadowMapSize);
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.12f, 0.12f, 0.14f, 1.0f));

        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        ecs_context->SetResourceNamePrefix("AlphaTestShadow:MainScene");
        ecs_context->SetScenePipelineMode(ScenePipelineMode::StandardLitCSM);

        environment_system = ecs_context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<EnvironmentSystem>();
        if (!environment_system)
            return false;

        auto *sky_info = environment_system->EditSkyInfo();
        if (!sky_info)
            return false;

        const glm::vec3 sun_direction = glm::normalize(glm::vec3(0.4f, 0.5f, 0.76f));
        sky_info->sun_direction = math::Vector4f(sun_direction.x, sun_direction.y, sun_direction.z, 0.0f);
        sky_info->SetTime(10, 0, 0);
        environment_system->MarkSkyDirty();

        if (!InitTextures())
            return false;
        if (!InitVDM())
            return false;
        if (!InitMaterial())
            return false;
        if (!CreateGeometries())
            return false;
        if (!InitCSMTargets())
            return false;
        if (!CreateScene())
            return false;
        if (!SetupCameras())
            return false;

        GLogInfo(u8"=== AlphaTestShadow initialized (2 cubes: masked + fallback-opaque) ===");
        GLogInfo(u8"预期: MaskedCube 影子=棋盘镂空, FallbackCube 影子=实心方影, 两者本体均镂空");
        return true;
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<AlphaTestShadowApp>(
        OS_TEXT("Alpha Test Shadow (masked vs fallback-opaque cascade shadow)"),
        argc, argv, 1280, 720);
}
