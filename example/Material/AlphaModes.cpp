// 该范例用 CUBE 演示四种 alpha 呈现方式。有效 alpha = **材质指定比例 × 纹理采样**：
//   · 比例 = 材质行 `PBRSurfaceRow.base_color.a`（列 = 档位 100/85/70/55/40/25/10%，步长 −15%）
//   · 采样 = opacity_mask（这里就用同一张单通道 gradient：逐像素 alpha 图案）
// ⇒ 同一列内 alpha 随纹理明暗起伏，列间整体被比例缩放（低档位整格趋于透/空）。
// 引擎侧（2026-10-05）：`EvalMaterialAlpha` 由「只读 opacity_mask」改为
// `base_color.a × opacity_mask.r`——`.a` 此前对 Lit 完全无效，现给它「混合比例」的正当语义；
// 既有材质 `.a` 全为 1.0 ⇒ 行为中性。
//   · Opaque      —— 完全忽略 alpha（实心）
//   · AlphaBlend  —— 标准 alpha 混合（blend_src=SRC_ALPHA / blend_dst=ONE_MINUS_SRC_ALPHA）：
//                    平滑透色，不做丢弃
//   · AlphaTest   —— 固定阈值（alpha_cutoff）硬边丢弃：alpha < 阈值 ⇒ discard（边缘是硬的）
//   · AlphaDither —— HGL_ALPHA_DITHER：按 alpha 与**逐像素**位置阈值 discard
//                    （用 dither 模拟 alpha_blend，顺序无关、留在不透明 pass）
// 四者对比（列 = 100→10%）：Opaque 全部一样实心 / AlphaBlend 逐级变透 /
// AlphaTest 阈值 0.5 ⇒ 前 4 档保留、后 3 档整块消失（硬边）/ AlphaDither 逐级变稀疏
//（**存活比例 ≈ alpha** ⇒ 空洞比例 ≈ 1−alpha：0/15/30/45/60/75/90%）。
//
// 引擎侧前提（2026-10-05 落地）：AlphaBlend 只有在**曲面 alpha 写进片元输出**时才生效
//（blend 因子 SRC_ALPHA 取输出 alpha）。前向片元模板此前恒写光照 alpha（= 1）⇒ blend 空转；
// 现由 `#define HGL_ALPHA_OUTPUT`（composer 在 alpha_blend / alpha_to_coverage 时注入）
// 分支改写成 `WriteMaterialOutput(vec4(rgb, EvalAlpha(...)))`。
//
// 布局：横向 7 列 = alpha 档位 100% → 10%（步长 −15%）；
//       纵向 4 行 = 四种方式（自上而下 Opaque / AlphaBlend / AlphaTest / AlphaDither）。
//
// alpha 真值 = `base_color.a`（该列比例）× `opacity_mask.r`（纹理图案）——见
// pbr_surface_source.glsl 的 EvalMaterialAlpha。颜色侧绑同一张单通道 gradient
//（BC4 只有 R 通道 ⇒ 采样得 (r,0,0,1) ⇒ 红调渐变）⇒ 每格都能看到纹理，
// 列间只差比例、列内 alpha 随纹理起伏。
//
// 顺带是本引擎"批从哪来"的实测点：Opaque 与 AlphaBlend 的 recipe 只差 pipeline 配置
//（alpha_blend 在 EDS 下是**动态状态**，见 VKCommandBufferRender.cpp 的
// cmd_set_color_blend_enable）⇒ 预计**同批**；Dither 改的是生成 GLSL（#define
// HGL_ALPHA_DITHER）⇒ 不同 shader program ⇒ **单独一批**。配合 GPU 剔除接管
//（AM_FORM_B=1）可直接在日志里看 batches=x/y 验证。
//
// env（全部默认关 ⇒ 基线 = 纯 CPU 账、无剔除）：
//   AM_CULL=1     启用 GPU 剔除管线（dispatch + 台架对拍；绘制仍走 CPU 账）
//   AM_FORM_B=1   启用 + gather 输出接管绘制（形态 B）
//   AM_PARTIAL=1  A3 台架：把 Opaque 行最后一格改成**独立 program + 实例化项**
//                 （billboard 顶点节点 ⇒ 独立批；AllocateContiguousInstances(…,1) ⇒
//                  InstancedPrimitiveRenderItem ⇒ ④ 类项）⇒ 该批不可接管、其余 27 项接管
//                 ⇒ 期望 ENGAGED batches=1/2（部分接管路径首次被真实触发）
//   AM_FRAMES=N   跑 N 帧后退出（CI / 性能对照）
//   AM_DUMP=<tag> 帧 8 帧内回读落原始字节后退出（像素判据）
//   AM_YAW/AM_PITCH/AM_DIST/AM_TX/AM_TY/AM_TZ  相机覆盖

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/filesystem/FileSystem.h>
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

    constexpr uint ALPHA_STEPS = 7;           // 横向：7 档固定 alpha
    constexpr uint MODE_ROWS   = 4;           // 纵向：Opaque / AlphaBlend / AlphaTest / AlphaDither
    constexpr uint ITEM_COUNT  = ALPHA_STEPS * MODE_ROWS;

    constexpr float ITEM_SPACING = 3.2f;
    constexpr float ROW_SPACING  = 3.2f;
    constexpr float CUBE_SCALE   = 1.25f;

    // 每列 alpha：100% → 10%，步长 −15%（1.00 0.85 0.70 0.55 0.40 0.25 0.10）
    constexpr float ALPHA_STEP_DELTA = 0.15f;
    constexpr float AlphaAt(const uint column)
    {
        return 1.0f - ALPHA_STEP_DELTA * float(column);
    }

    // 颜色/图案纹理（所有格共用一张；单通道 BC4 ⇒ 采样 (r,0,0,1)）：
    // 既作 base_color（可见图案），也作 opacity_mask（逐像素 alpha 图案）
    constexpr const os_char * PATTERN_TEXTURE_NAME = OS_TEXT("0.Tex2D");

    const char * MODE_NAME[MODE_ROWS] = { "Opaque", "AlphaBlend", "AlphaTest", "AlphaDither" };

    // AlphaTest 用的**固定**阈值（与 dither 的逐像素阈值形成对照：硬边 vs 噪点）
    constexpr float ALPHA_TEST_CUTOFF = 0.5f;
}

class AlphaModesApp : public WorkObject
{
private:

    ECSContext *  ecs_world     = nullptr;
    Entity *      camera_entity = nullptr;

    using MaterialDataAccessor = graph::GlobalSSBODataAccessor;

    // 一份基础配方 + 三种变体（第一种 = 基础配方本身）
    graph::mtl::MaterialRecipe opaque_recipe{};
    graph::mtl::MaterialRecipe blend_recipe{};
    graph::mtl::MaterialRecipe alpha_test_recipe{};
    graph::mtl::MaterialRecipe dither_recipe{};

    // A3（AM_PARTIAL=1）：独立 program 的"不可接管批"配方（Lit + billboard 顶点节点）
    graph::mtl::MaterialRecipe partial_recipe{};
    PrimitiveAsset partial_asset{};
    bool partial_enabled = false;

    // 一份几何（CUBE）+ 三个 PrimitiveAsset（同一几何、三种 alpha 方式）
    Geometry *      cube_geometry = nullptr;
    PrimitiveAsset  mode_asset[MODE_ROWS]{};

    MaterialDataAccessor surface_rows[MODE_ROWS][ALPHA_STEPS]{};

    Texture2D * pattern_texture = nullptr;   ///< 单通道 gradient：base_color + opacity_mask
    Sampler *   sampler = nullptr;

    hgl::ecs::TransformAccessor item_transforms[MODE_ROWS][ALPHA_STEPS]{};

    std::string dump_tag;
    uint32_t    dump_frame      = 0;
    uint32_t    perf_frames     = 0;
    uint32_t    perf_frame_seen = 0;
    double      perf_total_us   = 0.0;

private:

    bool InitTextures()
    {
        auto* texture_manager = GetManager<TextureManager>();
        auto* sampler_manager = GetManager<SamplerManager>();

        if (!texture_manager || !sampler_manager)
            return false;

        pattern_texture = texture_manager->LoadTexture2D(
            filesystem::JoinPathWithFilename(OS_TEXT("res/image/gradient"), PATTERN_TEXTURE_NAME),
            true);
        if (!pattern_texture)
        {
            printf("[ERROR] InitTextures: failed to load gradient pattern\n");
            return false;
        }

        sampler = sampler_manager->CreateSampler();
        return sampler != nullptr;
    }

    bool InitMaterial()
    {
        // 基础配方：Lit，实心（Opaque 就是它）
        opaque_recipe.recipe_name = "AlphaModes.LitOpaque";
        opaque_recipe.mtl_def_id  = "Lit";
        opaque_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();

        // 变体二：alpha 混合（其余同基础配方）
        blend_recipe = opaque_recipe;
        blend_recipe.recipe_name = "AlphaModes.LitAlphaBlend";
        blend_recipe.render_state_overrides.pipeline_config = mtl::MakeAlpha3DConfig();

        // 变体三：alpha test（固定阈值硬边丢弃；同样进生成的 GLSL）
        alpha_test_recipe = opaque_recipe;
        alpha_test_recipe.recipe_name = "AlphaModes.LitAlphaTest";
        alpha_test_recipe.render_state_overrides.has_alpha_test = true;
        alpha_test_recipe.render_state_overrides.alpha_test     = true;
        alpha_test_recipe.render_state_overrides.has_alpha_cutoff = true;
        alpha_test_recipe.render_state_overrides.alpha_cutoff     = ALPHA_TEST_CUTOFF;

        // 变体四：dither 覆盖（逐像素阈值，用 dither 模拟 alpha_blend）
        dither_recipe = opaque_recipe;
        dither_recipe.recipe_name = "AlphaModes.LitAlphaDither";
        dither_recipe.render_state_overrides.has_dither = true;
        dither_recipe.render_state_overrides.dither     = true;

        // A3（AM_PARTIAL=1）：Lit + **不同顶点节点**（billboard）⇒ 不同 shader program
        // ⇒ 独立批；该批的实体再被标成实例化项 ⇒ 该批命中 guard 的 ④ 拒绝条件。
        partial_enabled = (getenv("AM_PARTIAL") != nullptr);

        if (partial_enabled)
        {
            partial_recipe = opaque_recipe;
            partial_recipe.recipe_name = "AlphaModes.LitOpaqueBillboard";
            partial_recipe.vertex_node_config.orientation = mtl::OrientationMode::CameraFacingFree;

            printf("[A3] AM_PARTIAL=1：Opaque 行最后一格改走第二套配方（Lit + billboard 顶点节点）"
                   "并标为实例化项 ⇒ 该批**不可接管**（④ 类项），其余 27 项仍接管"
                   "⇒ 期望 ENGAGED batches=1/2\n");
        }

        return true;
    }

    bool CreateSurfaceRows()
    {
        auto* domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        for (uint row = 0; row < MODE_ROWS; ++row)
        {
            for (uint col = 0; col < ALPHA_STEPS; ++col)
            {
                surface_rows[row][col] = domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
                if (!surface_rows[row][col])
                    return false;

                // 基础色 = 白（RGB 交给纹理），**alpha = 该列的混合比例**（= 有效 alpha 的比例项）
                graph::ssbo::PBRSurfaceRow material_data{};
                material_data.base_color   = Color4f(1.0f, 1.0f, 1.0f, AlphaAt(col));
                material_data.metallic     = 0.0f;
                material_data.roughness    = 0.75f;
                material_data.normal_scale = 0.0f;

                if (!surface_rows[row][col].Write(material_data))
                    return false;
            }
        }

        return true;
    }

    bool CreateCubeGeometry()
    {
        using namespace inline_geometry;

        auto* geometry_manager = GetManager<GeometryManager>();
        auto* device           = GetDevice();

        if (!geometry_manager || !device)
            return false;

        auto geometry_creater = std::make_unique<GeometryCreater>(
            device,
            CreateStandardGeometryVertexFormat());

        CubeCreateInfo cci;
        cci.segments_x = 1;
        cci.segments_y = 1;
        cci.segments_z = 1;

        cube_geometry = CreateCube(geometry_creater.get(), &cci);
        if (!cube_geometry)
            return false;

        geometry_manager->Add(cube_geometry);
        return true;
    }

    bool InitScene()
    {
        if (!(opaque_recipe.material_ssbo_binding = surface_rows[0][0].GetGlobalSSBOBinding()).IsValid())
            return false;

        blend_recipe.material_ssbo_binding      = opaque_recipe.material_ssbo_binding;
        alpha_test_recipe.material_ssbo_binding = opaque_recipe.material_ssbo_binding;
        dither_recipe.material_ssbo_binding     = opaque_recipe.material_ssbo_binding;

        if (partial_enabled)
            partial_recipe.material_ssbo_binding = opaque_recipe.material_ssbo_binding;

        if (!CreateCubeGeometry())
            return false;

        mode_asset[0] = PrimitiveAsset(cube_geometry, &opaque_recipe,     PrimitiveType::Triangles);
        mode_asset[1] = PrimitiveAsset(cube_geometry, &blend_recipe,      PrimitiveType::Triangles);
        mode_asset[2] = PrimitiveAsset(cube_geometry, &alpha_test_recipe, PrimitiveType::Triangles);
        mode_asset[3] = PrimitiveAsset(cube_geometry, &dither_recipe,     PrimitiveType::Triangles);

        if (partial_enabled)
            partial_asset = PrimitiveAsset(cube_geometry, &partial_recipe, PrimitiveType::Triangles);

        const float offset_y = (ALPHA_STEPS - 1) * ITEM_SPACING * 0.5f;

        for (uint row = 0; row < MODE_ROWS; ++row)
        {
            for (uint col = 0; col < ALPHA_STEPS; ++col)
            {
                char name[64];
                snprintf(name, sizeof(name), "Cube_%s_A%u", MODE_NAME[row],
                         uint(AlphaAt(col) * 100.0f + 0.5f));

                Entity * e = ecs_world->CreateEntity<Entity>(name);

                // ⚠ 世界 Z 轴向上（相机 forward.z=sin(pitch)、yaw 绕 Z）⇒ 横向摆 Y、纵向摆 Z；
                // 且 yaw=0 时屏幕横向 = **−Y**（forward=+X、up=+Z ⇒ right=−Y）⇒ 用
                // `offset_y − col*间距` 才能让画面**从左到右**依次是 100% → 10%。
                const float y = offset_y - col * ITEM_SPACING;
                const float z = float(MODE_ROWS - 1 - row) * ROW_SPACING
                              - float(MODE_ROWS - 1) * 0.5f * ROW_SPACING;

                auto transform = ecs_world->GetTransform(
                    ecs_world->CreateTransform(e->GetEntityID(), Mobility::Static));
                transform.SetLocalPosition(glm::vec3(0.0f, y, z));
                transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                transform.SetLocalScale(glm::vec3(CUBE_SCALE, CUBE_SCALE, CUBE_SCALE));
                transform.SetMobility(Mobility::Static);
                item_transforms[row][col] = transform;

                auto * geometry_data = e->GetContext()->GetOrCreateGeometryData(e);
                hgl::ecs::MaterialData * material_data = e->GetContext()->GetOrCreateMaterialData(e);

                // A3：Opaque 行最后一格 = 不可接管批的样本（独立 program + 实例化项）
                const bool as_partial =
                    partial_enabled && (row == 0) && (col == ALPHA_STEPS - 1);

                geometry_data->GetOwner()->GetContext()->GetOrCreateGeometryData(
                    geometry_data->GetOwnerID())->SetPrimitiveAsset(
                        as_partial ? &partial_asset : &mode_asset[row]);

                // 同一张单通道 gradient 同时当 base_color（可见图案）与 opacity_mask
                //（逐像素 alpha 图案）⇒ 有效 alpha = 本列比例 × 纹理采样
                if (!material_data->SetTextureResource("base_color", pattern_texture, sampler)
                 || !material_data->SetTextureResource("opacity_mask", pattern_texture, sampler))
                    return false;

                hgl::ecs::MaterialData::MaterialDataAuthoringResource row_resource{};
                row_resource = surface_rows[row][col].GetGlobalSSBOBinding();
                material_data->SetDataResource(row_resource);

                if (as_partial)
                {
                    // capacity = 1 ⇒ 仍是"单实例"（CPU 账照常画），但项类型变
                    // InstancedPrimitiveRenderItem ⇒ 该批命中 ④ 拒绝条件（gather Y=1 不适配）。
                    // 注意：SetInstanceCount/SetMaxInstances(1) 本身**不分配**（只有 >1 才分配）
                    // ⇒ 必须显式 AllocateContiguousInstances(…, 1) 才会置 capacity。
                    if (!hgl::ecs::AllocateContiguousInstances(*ecs_world, e->GetEntityID(), 1))
                    {
                        printf("[ERROR] A3: AllocateContiguousInstances failed\n");
                        return false;
                    }
                    hgl::ecs::SetInstanceCount(*ecs_world, e->GetEntityID(), 1);
                }
            }
        }

        printf("[AlphaModes] %u CUBE = %u 列（alpha 档位 100%% → 10%%，步长 -15%%）× %u 行"
               "（自上而下 %s / %s / %s / %s）\n"
               "            有效 alpha = 材质行 base_color.a（该列比例）× opacity_mask 采样"
               "（同一张 gradient）\n"
               "            预期：AlphaBlend 逐级变透；AlphaTest(阈值 %.2f) 只保 100/85/70/55%%、"
               "后三档整块消失；\n"
               "                  AlphaDither 存活比例 ≈ alpha ⇒ 空洞 ≈ 0/15/30/45/60/75/90%%\n"
               "            批预期：AlphaBlend 带 HGL_ALPHA_OUTPUT ⇒ 4 个 program ⇒ 4 个批\n",
               ITEM_COUNT, ALPHA_STEPS, MODE_ROWS,
               MODE_NAME[0], MODE_NAME[1], MODE_NAME[2], MODE_NAME[3],
               double(ALPHA_TEST_CUTOFF));
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
        camera->distance     = 23.0f;
        camera->yaw          = 0.0f;
        camera->pitch        = -8.0f;

        if (const char * v = getenv("AM_YAW"))   camera->yaw      = (float)atof(v);
        if (const char * v = getenv("AM_PITCH")) camera->pitch    = (float)atof(v);
        if (const char * v = getenv("AM_DIST"))  camera->distance = (float)atof(v);
        if (const char * v = getenv("AM_TX"))    camera->target.x = (float)atof(v);
        if (const char * v = getenv("AM_TY"))    camera->target.y = (float)atof(v);
        if (const char * v = getenv("AM_TZ"))    camera->target.z = (float)atof(v);

        camera->is_main_camera = true;
        camera->matrix_dirty   = true;
        camera->viewport_info  = GetViewportInfo();

        return true;
    }

    bool InitCull()
    {
        // GPU 剔除 **opt-in**（默认全关 = 纯 CPU 账基线）
        if (!getenv("AM_CULL") && !getenv("AM_FORM_B"))
            return true;

        auto gpu_cull = ecs_world->GetSystem<GPUCullSystem>();
        if (!gpu_cull)
        {
            printf("[ERROR] InitCull: GPUCullSystem 未注册\n");
            return false;
        }

        gpu_cull->SetEnabled(true);

        if (getenv("AM_FORM_B"))
            gpu_cull->SetDrawOverrideEnabled(true);

        return true;
    }

public:

    ~AlphaModesApp()
    {
        SAFE_CLEAR(cube_geometry)
    }

    bool Init() override
    {
        // 背景用中灰蓝：AlphaBlend 的透色与 Dither 的镂空都能从背景反衬出来
        //（纯黑背景上"混合=变暗"，差异远不如透出色差明显）
        SetClearColor(Color4f(0.22f, 0.26f, 0.34f, 1.0f));

        ecs_world = GetECSContext();
        if (!ecs_world)
        {
            printf("[ERROR] Init: no ECSContext\n");
            return false;
        }

        if (!InitTextures())      return false;
        if (!InitMaterial())      return false;
        if (!CreateSurfaceRows()) return false;
        if (!InitScene())         return false;
        if (!InitCamera())        return false;
        if (!InitCull())          return false;

        if (const char * tag = getenv("AM_DUMP"))
            dump_tag = tag;

        if (const char * v = getenv("AM_FRAMES"))
            perf_frames = (uint32_t)atoi(v);

        return true;
    }

    void Tick(double delta_time) override
    {
        const bool perf_on = perf_frames > 0;
        const uint64 perf_t0 = hgl::GetUptimeUs();

        // 静态场景：相机与几何逐帧确定 ⇒ 像素判据不被时序干扰。

        WorkObject::Tick(delta_time);

        // AM_DUMP=<tag>：第 8 帧装一次性帧内回读钩子（钩子在本帧提交后、present 前执行
        // ⇒ 读到本帧画面），落原始 packed 字节后退出。
        if (!dump_tag.empty() && ++dump_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                ecs_world ? ecs_world->GetRenderTarget() : nullptr);
            if (!rt)
            {
                printf("[AMDUMP] no swapchain RT\n");
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
                    printf("[AMDUMP] readback failed\n");
                    std::exit(2);
                }

                char name[256];
                snprintf(name, sizeof(name), "amdump_%s_%ux%u_%s.raw",
                         tag.c_str(), info.width, info.height,
                         GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");
                FILE * fp = fopen(name, "wb");
                if (!fp)
                {
                    printf("[AMDUMP] open %s failed\n", name);
                    std::exit(2);
                }
                fwrite(pixels.data(), 1, pixels.size(), fp);
                fclose(fp);
                printf("[AMDUMP] wrote %s (%zu bytes)\n", name, pixels.size());
                std::exit(0);
            });
        }

        if (perf_on)
        {
            perf_total_us += double(hgl::GetUptimeUs() - perf_t0);
            ++perf_frame_seen;

            if (perf_frame_seen >= perf_frames)
            {
                printf("[AlphaModes] done frames=%u\n", perf_frame_seen);
                fflush(stdout);
                std::exit(0);
            }
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<AlphaModesApp>(
        OS_TEXT("Cube x Gradient(BC4 single-channel) x Opaque / AlphaBlend / Dither"),
        argc, argv, 1280, 720);
}
