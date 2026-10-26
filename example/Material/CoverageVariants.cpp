// 该范例演示**同一份材质配方的两个覆盖（coverage）变体**：
//   · Opaque      —— 无覆盖模式（实体，alpha 恒 1.0）
//   · DitherMask  —— HGL_ALPHA_DITHER（按 alpha 与位置阈值 discard 的**顺序无关半透明**；
//                    不混合、不排序，因此仍留在不透明 pass）
// 两个变体共用同一个 mtl_def（Lit）、同一份几何、同一套纹理；差别只在覆盖模式 ⇒
// **生成的 fragment 着色器不同 ⇒ 不同 ShaderProgram ⇒ 两个 MaterialBatch**。
// （这一点与"只改渲染状态"不同：EDS 之后 Pipeline 只保留 shader 部分，cull/blend/depth
//  之类状态在绘制时动态应用，改它们**不会**分出新批。）
//
// 因此本范例同时是 **GPU 剔除多批（T3i ③）的测试床**：env 门控打开剔除后，
// 日志给出接管批数（ENGAGED batches=2/2）与台架两本账/值链判据；dump 走像素全等判据。
//
// env（全部默认关 ⇒ 基线 = 纯 CPU 账、无剔除）：
//   CV_FRAMES=N   跑 N 帧后退出（CI / 性能对照）
//   CV_STRESS=N   A4 容量台架：N 个「同 def(Lit) + 仅 alpha_cutoff 不同」的配方
//                 ⇒ 生成的片元 GLSL 只差 `#define HGL_ALPHA_CUTOFF <v>` ⇒ N 个 program
//                 ⇒ N 个批 ⇒ 用来撞守卫的 kMaxBatches(256)：期望只接管前 256 个、
//                 超出的批 **fail-fast**（一次告警 + 本帧不绘制，不再回 CPU 账兜底，A6-d T4b）；
//                 overflow 必须为 0。
//                 （N 个实体各占一行材质行 ⇒ N ≤ 512）
//   CV_DUMP=<tag> 帧 8 帧内回读落原始字节后退出（像素判据）
//   CV_YAW/CV_PITCH/CV_DIST/CV_TX/CV_TY/CV_TZ  相机覆盖

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/color/Color.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/ecs/support/GPUCullSystem.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKTextureReadback.h>
#include "../Shared/FrameDigest.h"   // 帧摘要自检（A6-d 判据重建共用件）
#include<hgl/time/Time.h>

#include<glm/glm.hpp>
#include<memory>
#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<string>
#include<vector>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }

    constexpr uint    GRID_COLS     = 10;      // 每行实体数（两行 × 10 = 20 个实体）
    constexpr uint    GRID_ROWS     = 2;       // row 0 = Opaque / row 1 = DitherMask
    constexpr uint    ITEM_COUNT    = GRID_COLS * GRID_ROWS;
    constexpr float   ITEM_SPACING  = 2.4f;
    constexpr uint    SPHERE_SLICES = 48;
    constexpr float   ROW_OFFSET_Z  = 1.9f;    // 两行上下分开，便于肉眼对比

    // ⚠ 世界是 **Z 轴向上**（相机 `forward.z = sin(pitch)`、yaw 绕 Z 轴，见
    // src/ecs/systems/tick/CameraSystem.cpp:753-762）⇒ 想"横向摆一排、上下叠两排"，
    // 必须横向用 Y、纵向用 Z。摆在 XY 平面上是"地板"（X 是屏幕纵深）。
}

class CoverageVariantsApp : public WorkObject
{
private:

    ECSContext *  ecs_world     = nullptr;
    Entity *      camera_entity = nullptr;

    using MaterialDataAccessor = graph::GlobalSSBODataAccessor;

    // 一种配方 + 它的覆盖变体（变体 = 同源拷贝 + 覆盖字段）
    graph::mtl::MaterialRecipe opaque_recipe{};
    graph::mtl::MaterialRecipe dither_recipe{};

    // ── A4 容量台架（CV_STRESS=N）──
    static constexpr uint  STRESS_MAX         = 512;
    static constexpr float STRESS_CUTOFF_STEP = 0.001f;

    graph::mtl::MaterialRecipe stress_recipe[STRESS_MAX]{};
    PrimitiveAsset             stress_asset[STRESS_MAX]{};
    MaterialDataAccessor       stress_rows[STRESS_MAX]{};
    bool stress_enabled = false;
    uint stress_count   = 0;

    // 一份几何 + 两个 PrimitiveAsset（同一几何、两个配方变体）
    Geometry *      sphere_geometry = nullptr;
    PrimitiveAsset  opaque_asset{};
    PrimitiveAsset  dither_asset{};

    MaterialDataAccessor surface_rows[GRID_ROWS][GRID_COLS]{};

    Texture2D * base_texture   = nullptr;
    Texture2D * normal_texture = nullptr;
    Texture2D * mask_texture   = nullptr;   ///< DitherMask 变体的 alpha 来源（opacity_mask）
    Sampler *   sampler        = nullptr;

    hgl::ecs::TransformAccessor item_transforms[GRID_ROWS][GRID_COLS]{};

    std::string dump_tag;
    uint32_t    dump_frame      = 0;
    uint32_t    perf_frames     = 0;
    uint32_t    perf_frame_seen = 0;
    double      perf_total_us   = 0.0;

private:

    bool InitMaterial()
    {
        auto* texture_manager = GetManager<TextureManager>();
        auto* sampler_manager = GetManager<SamplerManager>();

        if (!texture_manager || !sampler_manager)
            return false;

        base_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
        normal_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"), true);

        // DitherMask 的 alpha 来源：黑白棋盘（白格 r=1 保留、黑格 r=0 必被阈值丢弃）
        // —— 与 AlphaTestShadow 的 opacity_mask 用法同源；Lit 定义的 alpha 真值**只来自
        // opacity_mask**（pbr_surface_source.glsl 的 EvalMaterialAlpha），base_color.a 不参与。
        mask_texture = texture_manager->LoadTexture2D(
            OS_TEXT("res/image/pbr/AlphaChecker/baseColor.Tex2D"), true);

        if (!base_texture || !normal_texture || !mask_texture)
        {
            printf("[ERROR] InitMaterial: failed to load textures\n");
            return false;
        }

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return false;

        // ── 基础配方（变体的"原始"形态）──
        opaque_recipe.recipe_name = "CoverageVariants.LitOpaque";
        opaque_recipe.mtl_def_id  = "Lit";
        opaque_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();

        // ── 变体：同源拷贝 + 覆盖模式（dither）──
        // dither 会进生成的 GLSL（#define HGL_ALPHA_DITHER）⇒ 不同 shader program。
        dither_recipe = opaque_recipe;
        dither_recipe.recipe_name = "CoverageVariants.LitDitherMask";
        dither_recipe.render_state_overrides.has_dither = true;
        dither_recipe.render_state_overrides.dither     = true;

        // ── A4：N 个「仅 alpha_cutoff 不同」的配方 ──
        // 唯一差异是阈值字面量（composer 落成 `#define HGL_ALPHA_CUTOFF <v>`）⇒ 片元 digest 不同
        // ⇒ program 不同 ⇒ 批不同。用于撞 kMaxBatches(256)。
        if (const char * v = getenv("CV_STRESS"))
        {
            stress_count = uint(atoi(v));
            if (stress_count < 2)              stress_count = 2;
            if (stress_count > STRESS_MAX)     stress_count = STRESS_MAX;
            stress_enabled = true;

            char rname[64];

            for (uint i = 0; i < stress_count; ++i)
            {
                graph::mtl::MaterialRecipe & r = stress_recipe[i];
                r = opaque_recipe;

                snprintf(rname, sizeof(rname), "CoverageVariants.StressCutoff%u", i);
                r.recipe_name = rname;

                r.render_state_overrides.has_alpha_test   = true;
                r.render_state_overrides.alpha_test       = true;
                r.render_state_overrides.has_alpha_cutoff = true;
                r.render_state_overrides.alpha_cutoff     = STRESS_CUTOFF_STEP * float(i + 1);
            }

            printf("[A4] CV_STRESS=%u：%u 个同 def、仅 alpha_cutoff 不同的配方"
                   "（期望 %u 个 program/批；守卫只接管前 256 个，其余 fail-fast 不绘制）\n",
                   stress_count, stress_count, stress_count);
        }

        return true;
    }

    bool CreateSurfaceRows()
    {
        auto* domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        for (uint row = 0; row < GRID_ROWS; ++row)
        {
            for (uint col = 0; col < GRID_COLS; ++col)
            {
                surface_rows[row][col] = domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
                if (!surface_rows[row][col])
                    return false;

                graph::ssbo::PBRSurfaceRow material_data{};
                material_data.base_color   = Color4f(0.72f, 0.72f, 0.72f, 1.0f);
                material_data.normal_scale = 0.35f;

                // 两行只差粗糙度：dither 行更粗（噪点镂空在粗糙面上更容易看清）
                material_data.metallic  = 0.10f;
                material_data.roughness = (row == 0) ? 0.45f : 0.85f;

                if (!surface_rows[row][col].Write(material_data))
                    return false;
            }
        }

        // A4：台架实体各占一行（cutoff 不同 ⇒ 颜色/粗糙度统一，便于逐像素对照）
        for (uint i = 0; i < stress_count; ++i)
        {
            stress_rows[i] = domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
            if (!stress_rows[i])
                return false;

            graph::ssbo::PBRSurfaceRow stress_data{};
            stress_data.base_color   = Color4f(0.72f, 0.72f, 0.72f, 1.0f);
            stress_data.normal_scale = 0.35f;
            stress_data.metallic     = 0.10f;
            stress_data.roughness    = 0.45f;

            if (!stress_rows[i].Write(stress_data))
                return false;
        }

        return true;
    }

    bool CreateSphereGeometry()
    {
        using namespace inline_geometry;

        auto* geometry_manager = GetManager<GeometryManager>();
        auto* device           = GetDevice();

        if (!geometry_manager || !device)
            return false;

        // 私有顶点缓冲路径：一份几何，两个配方变体共用
        auto geometry_creater = std::make_unique<GeometryCreater>(
            device,
            CreateStandardGeometryVertexFormat());

        sphere_geometry = CreateSphere(geometry_creater.get(), SPHERE_SLICES);
        if (!sphere_geometry)
            return false;

        geometry_manager->Add(sphere_geometry);
        return true;
    }

    bool InitScene()
    {
        // 配方绑定材质数据行池（行本身 per-entity，绑定只是"池身份"）
        if (!(opaque_recipe.material_ssbo_binding = surface_rows[0][0].GetGlobalSSBOBinding()).IsValid())
            return false;

        dither_recipe.material_ssbo_binding = opaque_recipe.material_ssbo_binding;

        if (!CreateSphereGeometry())
            return false;

        opaque_asset = PrimitiveAsset(sphere_geometry, &opaque_recipe, PrimitiveType::Triangles);
        dither_asset = PrimitiveAsset(sphere_geometry, &dither_recipe, PrimitiveType::Triangles);

        // ── A4 台架：N 个实体、每个一份配方（⇒ N 个批）──
        if (stress_enabled)
        {
            // 排成方阵（边长 = ceil(sqrt(N))，间距收紧）⇒ 相机默认取景能全覆盖
            uint side = 1;
            while (side * side < stress_count)
                ++side;

            const float step  = ITEM_SPACING * 0.55f;
            const float off   = float(side - 1) * 0.5f * step;

            for (uint i = 0; i < stress_count; ++i)
            {
                stress_asset[i] = PrimitiveAsset(sphere_geometry, &stress_recipe[i], PrimitiveType::Triangles);

                char name[64];
                snprintf(name, sizeof(name), "StressSphere_Cutoff%u", i);

                Entity * e = ecs_world->CreateEntity<Entity>(name);

                const float y = float(i % side) * step - off;
                const float z = float(i / side) * step - off;

                auto transform = ecs_world->GetTransform(
                    ecs_world->CreateTransform(e->GetEntityID(), Mobility::Static));
                transform.SetLocalPosition(glm::vec3(0.0f, y, z));
                transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
                transform.SetMobility(Mobility::Static);

                auto * geometry_data = e->GetContext()->GetOrCreateGeometryData(e);
                hgl::ecs::MaterialData * material_data = e->GetContext()->GetOrCreateMaterialData(e);

                geometry_data->GetOwner()->GetContext()->GetOrCreateGeometryData(
                    geometry_data->GetOwnerID())->SetPrimitiveAsset(&stress_asset[i]);

                if (!material_data->SetTextureResource("base_color", base_texture, sampler)
                 || !material_data->SetTextureResource("normal", normal_texture, sampler)
                 || !material_data->SetTextureResource("opacity_mask", mask_texture, sampler))
                    return false;

                hgl::ecs::MaterialData::MaterialDataAuthoringResource row_resource{};
                row_resource = stress_rows[i].GetGlobalSSBOBinding();
                material_data->SetDataResource(row_resource);
            }

            printf("[A4] 台架就绪：%u 实体 / %u 配方（方阵 %ux%u）⇒ 期望 batches=%u，"
                   "守卫接管上限 256\n",
                   stress_count, stress_count, side, side, stress_count);
            fflush(stdout);
            return true;
        }

        const float offset_x = (GRID_COLS - 1) * ITEM_SPACING * 0.5f;

        for (uint row = 0; row < GRID_ROWS; ++row)
        {
            for (uint col = 0; col < GRID_COLS; ++col)
            {
                char name[64];
                snprintf(name, sizeof(name), "Sphere_%s_C%u",
                         (row == 0) ? "Opaque" : "DitherMask", col);

                Entity * e = ecs_world->CreateEntity<Entity>(name);

                // 一排横着（Y）、两行竖着（Z），全部贴在同一纵深平面（X=0）⇒
                // 两排都不被遮挡，抖动镂空肉眼可辨。
                const float y = col * ITEM_SPACING - offset_x;
                const float z = (row == 0) ? ROW_OFFSET_Z : -ROW_OFFSET_Z;

                auto transform = ecs_world->GetTransform(
                    ecs_world->CreateTransform(e->GetEntityID(), Mobility::Static));
                transform.SetLocalPosition(glm::vec3(0.0f, y, z));
                transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
                transform.SetMobility(Mobility::Static);
                item_transforms[row][col] = transform;

                auto * geometry_data = e->GetContext()->GetOrCreateGeometryData(e);
                hgl::ecs::MaterialData * material_data = e->GetContext()->GetOrCreateMaterialData(e);

                geometry_data->GetOwner()->GetContext()->GetOrCreateGeometryData(
                    geometry_data->GetOwnerID())->SetPrimitiveAsset(
                        (row == 0) ? &opaque_asset : &dither_asset);

                if (!material_data->SetTextureResource("base_color", base_texture, sampler)
                 || !material_data->SetTextureResource("normal", normal_texture, sampler))
                    return false;

                // DitherMask 变体：绑 opacity_mask（alpha 真值来源；未绑 = 1.0 ⇒ dither 不裁剪，
                // 即"状态关掉"的那一端）—— 与 AlphaTestShadow 的 CubeA/CubeB 对照同款作者写法。
                if (row == 1
                 && !material_data->SetTextureResource("opacity_mask", mask_texture, sampler))
                    return false;

                hgl::ecs::MaterialData::MaterialDataAuthoringResource row_resource{};
                row_resource = surface_rows[row][col].GetGlobalSSBOBinding();
                material_data->SetDataResource(row_resource);
            }
        }

        printf("[CoverageVariants] %u 实体：Opaque=%u（Lit / coverage None）、DitherMask=%u"
               "（Lit + dither ⇒ HGL_ALPHA_DITHER）\n"
               "                    同一 mtl_def + 同一几何 + 同一纹理 ⇒ 期望 **2 个批**"
               "（不同 shader program）\n",
               ITEM_COUNT, GRID_COLS, GRID_COLS);
        fflush(stdout);

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_world || !ecs_world->EnsureCameraSystem())
            return false;

        camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        // 相机 = 世界级资源：经世界访问器创建/取回（实体只是宿主）
        auto * camera = ecs_world->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target       = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance     = 17.0f;
        camera->yaw          = 0.0f;
        camera->pitch        = -16.0f;

        // 取景覆盖（dump 模式也走这里 ⇒ 两次运行的像素可比）
        if (stress_enabled)   // 方阵展开后拉远取景（可用 CV_DIST 覆盖）
            camera->distance = 14.0f;

        if (const char * v = getenv("CV_YAW"))   camera->yaw         = (float)atof(v);
        if (const char * v = getenv("CV_PITCH")) camera->pitch       = (float)atof(v);
        if (const char * v = getenv("CV_DIST"))  camera->distance    = (float)atof(v);
        if (const char * v = getenv("CV_TX"))    camera->target.x    = (float)atof(v);
        if (const char * v = getenv("CV_TY"))    camera->target.y    = (float)atof(v);
        if (const char * v = getenv("CV_TZ"))    camera->target.z    = (float)atof(v);

        camera->is_main_camera = true;
        camera->matrix_dirty   = true;
        camera->viewport_info  = GetViewportInfo();

        return true;
    }

    bool InitCull()
    {
        // GPU 剔除 **opt-in**（默认全关 = 纯 CPU 账基线）：见 doc §8.6 两档开关。
        auto gpu_cull = ecs_world->GetSystem<GPUCullSystem>();
        if (!gpu_cull)
        {
            printf("[ERROR] InitCull: GPUCullSystem 未注册\n");
            return false;
        }

        return true;
    }

public:

    ~CoverageVariantsApp()
    {
        // sphere_geometry 已交给 GeometryManager（唯一持有者），此处不再 delete。
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.08f, 0.08f, 0.09f, 1.0f));

        ecs_world = GetECSContext();
        if (!ecs_world)
        {
            printf("[ERROR] Init: no ECSContext\n");
            return false;
        }

        if (!InitMaterial())      return false;
        if (!CreateSurfaceRows()) return false;
        if (!InitScene())         return false;
        if (!InitCamera())        return false;
        if (!InitCull())          return false;

        if (const char * tag = getenv("CV_DUMP"))
            dump_tag = tag;

        if (const char * v = getenv("CV_FRAMES"))
            perf_frames = (uint32_t)atoi(v);

        return true;
    }

    void Tick(double delta_time) override
    {
        const bool perf_on = perf_frames > 0;
        const uint64 perf_t0 = hgl::GetUptimeUs();

        // 静态场景（无动画）：相机与几何逐帧确定 ⇒ 像素判据不被时序干扰。

        WorkObject::Tick(delta_time);

        // CV_DUMP=<tag>：第 8 帧装一次性帧内回读钩子（钩子在本帧提交后、present 前执行
        // ⇒ 读到本帧画面），落原始 packed 字节后退出（同 PBRSpheres/AlphaTestShadow 先例）。
        // CVSELFCHECK=1：**帧摘要自检**（A6-d 判据重建；共用件 example/Shared/FrameDigest.h）——
        // 第 8 帧帧内回读颜色靶 ⇒ {不同字节取值数, FNV-1a 64} 比对下方内联常量：PASS 退出 0 / FAIL 退出 1。
        // 判据只建在**冻结相机 + 定帧**上；常量由实跑记录，改渲染结果（少画/错画/挪相机）必须复核重录。
        {
            static const bool s_selfcheck = (getenv("CV_SELFCHECK") != nullptr);
            static uint32_t   s_self_frame = 0;

            static constexpr uint32_t kSelfW           = 1280;
            static constexpr uint32_t kSelfH           = 720;
            static constexpr uint32_t kSelfMinDistinct = 2;   // 不空转守卫（恒定图会 FAIL）
            static constexpr uint64_t kSelfFNV         = 0x8eed38bad9b8181eull;  // 默认场景（2 变体）：冻结相机 + 接管下实跑记录
                                                        // （改渲染结果必须复核并重录）
            static constexpr uint64_t kSelfFNV_Stress  = 0x88fba02a89c2c41bull;  // CV_STRESS=260：压测方阵（260 配方／256 接管 +
                                                        // 超出批 fail-fast 不绘制）；同上，改结果必须重录

            if (s_selfcheck && ++s_self_frame == 8)
            {
                auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>( ecs_world ? ecs_world->GetRenderTarget() : nullptr);
                if (!rt)
                {
                    printf("[CVSELF] no swapchain RT\n");
                    std::exit(2);
                }

                ulre_example::FrameDigest expect;
                expect.width                = kSelfW;
                expect.height               = kSelfH;
                expect.distinct_byte_values = kSelfMinDistinct;
                // 期望摘要按场景模式取（同一份代码两种场景；压测模式画面本就不同）
                expect.fnv1a64              = getenv("CV_STRESS") ? kSelfFNV_Stress : kSelfFNV;

                ulre_example::InstallFrameDigestHook(rt, expect, "CVSELF");
            }
        }


        if (!dump_tag.empty() && ++dump_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                ecs_world ? ecs_world->GetRenderTarget() : nullptr);
            if (!rt)
            {
                printf("[CVDUMP] no swapchain RT\n");
                std::exit(2);
            }

            std::string tag = dump_tag;
            graph::IRenderTarget * rt_ptr = rt;
            rt->SetInFrameReadbackHook([tag, rt_ptr]()
            {
                std::vector<uint8_t> pixels;
                graph::TextureReadbackInfo info;
                if (!graph::ReadbackColorTarget(rt_ptr, pixels, 0, &info) || pixels.empty())
                {
                    printf("[CVDUMP] readback failed\n");
                    std::exit(2);
                }

                char name[256];
                snprintf(name, sizeof(name), "cvdump_%s_%ux%u_%s.raw",
                         tag.c_str(), info.width, info.height,
                         GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");
                FILE * fp = fopen(name, "wb");
                if (!fp)
                {
                    printf("[CVDUMP] open %s failed\n", name);
                    std::exit(2);
                }
                fwrite(pixels.data(), 1, pixels.size(), fp);
                fclose(fp);
                printf("[CVDUMP] wrote %s (%zu bytes)\n", name, pixels.size());
                std::exit(0);
            });
        }

        // CV_FRAMES=N：定帧退出（CI / 性能对照）
        if (perf_on)
        {
            perf_total_us += double(hgl::GetUptimeUs() - perf_t0);
            ++perf_frame_seen;

            if (perf_frame_seen >= perf_frames)
            {
                printf("[CV] done frames=%u\n", perf_frame_seen);
                fflush(stdout);
                std::exit(0);
            }
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<CoverageVariantsApp>(
        OS_TEXT("Material Coverage Variants: Opaque vs DitherMask (GPU cull multi-batch bed)"),
        argc, argv, 1280, 720);
}
