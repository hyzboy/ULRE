// 该范例用**单通道**（BC4）gradient 纹理贴 CUBE，并把**同一份内容**用三种 alpha 呈现方式显示：
//   · Opaque     —— 完全忽略 alpha（实心）
//   · AlphaBlend —— 标准 alpha 混合（blend_src=SRC_ALPHA / blend_dst=ONE_MINUS_SRC_ALPHA）
//   · Dither     —— HGL_ALPHA_DITHER：按 alpha 与位置阈值 discard 的**顺序无关半透明**
//
// 布局：横向 6 列 = 6 张 gradient（取自 res/image/gradient 的 23 张单通道纹理，均匀取样）；
//       纵向 3 行 = 三种方式（上 Opaque / 中 AlphaBlend / 下 Dither）。
//
// 单通道 gradient **只绑 opacity_mask**：Lit 的 alpha 真值**只出自 opacity_mask**
//（pbr_surface_source.glsl 的 EvalMaterialAlpha = tex_opacity_mask.r；base_color.a 不参与），
// base_color 留空 ⇒ 采样回退 1.0 ⇒ 立方体是亮实心，三种方式的差别只落在 alpha 行为上。
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
//   AM_FRAMES=N   跑 N 帧后退出（CI / 性能对照）
//   AM_DUMP=<tag> 帧 8 帧内回读落原始字节后退出（像素判据）
//   AM_YAW/CV_PITCH/...  相机覆盖：AM_YAW / AM_PITCH / AM_DIST / AM_TX / AM_TY / AM_TZ

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

    constexpr uint GRADIENT_COLUMNS = 6;      // 横向：6 张 gradient 纹理
    constexpr uint MODE_ROWS        = 3;      // 纵向：Opaque / AlphaBlend / Dither
    constexpr uint ITEM_COUNT       = GRADIENT_COLUMNS * MODE_ROWS;

    constexpr float ITEM_SPACING = 3.2f;
    constexpr float ROW_SPACING  = 3.2f;
    constexpr float CUBE_SCALE   = 1.25f;

    // 取自 res/image/gradient（23 张单通道 BC4，256²，含 mip）的均匀取样
    // 选片依据（实测 23 张的像素亮度分布）：**暗区占比高**的才看得出透明/镂空——
    // 15/16/20/21 几乎全亮（暗区 0~2%）⇒ alpha≈1 ⇒ 三种方式看不出差别；下列 6 张暗区
    // 占比 45~71%（2/5/6/10/14/18），跨度也大（min≈0、max≈255）。
    constexpr const os_char * GRADIENT_TEXTURE_NAME[GRADIENT_COLUMNS]=
    {
        OS_TEXT("2.Tex2D"),  OS_TEXT("5.Tex2D"),  OS_TEXT("6.Tex2D"),
        OS_TEXT("10.Tex2D"), OS_TEXT("14.Tex2D"), OS_TEXT("18.Tex2D")
    };

    const char * MODE_NAME[MODE_ROWS] = { "Opaque", "AlphaBlend", "Dither" };
}

class AlphaModesApp : public WorkObject
{
private:

    ECSContext *  ecs_world     = nullptr;
    Entity *      camera_entity = nullptr;

    using MaterialDataAccessor = graph::GlobalSSBODataAccessor;

    // 一份基础配方 + 两种变体（第三种 = 基础配方本身）
    graph::mtl::MaterialRecipe opaque_recipe{};
    graph::mtl::MaterialRecipe blend_recipe{};
    graph::mtl::MaterialRecipe dither_recipe{};

    // 一份几何（CUBE）+ 三个 PrimitiveAsset（同一几何、三种 alpha 方式）
    Geometry *      cube_geometry = nullptr;
    PrimitiveAsset  mode_asset[MODE_ROWS]{};

    MaterialDataAccessor surface_rows[MODE_ROWS][GRADIENT_COLUMNS]{};

    Texture2D * gradient_texture[GRADIENT_COLUMNS]{};
    Sampler *   sampler = nullptr;

    hgl::ecs::TransformAccessor item_transforms[MODE_ROWS][GRADIENT_COLUMNS]{};

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

        for (uint col = 0; col < GRADIENT_COLUMNS; ++col)
        {
            const OSString file = filesystem::JoinPathWithFilename(
                OS_TEXT("res/image/gradient"), GRADIENT_TEXTURE_NAME[col]);

            gradient_texture[col] = texture_manager->LoadTexture2D(file, true);
            if (!gradient_texture[col])
            {
                printf("[ERROR] InitTextures: failed to load gradient[%u]\n", col);
                return false;
            }
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

        // 变体三：dither 覆盖（进生成的 GLSL ⇒ 不同 shader program）
        dither_recipe = opaque_recipe;
        dither_recipe.recipe_name = "AlphaModes.LitDitherMask";
        dither_recipe.render_state_overrides.has_dither = true;
        dither_recipe.render_state_overrides.dither     = true;

        return true;
    }

    bool CreateSurfaceRows()
    {
        auto* domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        for (uint row = 0; row < MODE_ROWS; ++row)
        {
            for (uint col = 0; col < GRADIENT_COLUMNS; ++col)
            {
                surface_rows[row][col] = domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
                if (!surface_rows[row][col])
                    return false;

                // 基础色 = 白：立方体的颜色完全来自单通道 gradient（采样得 (r,0,0,1) ⇒ 红调）
                graph::ssbo::PBRSurfaceRow material_data{};
                material_data.base_color   = Color4f(1.0f, 1.0f, 1.0f, 1.0f);
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

        blend_recipe.material_ssbo_binding  = opaque_recipe.material_ssbo_binding;
        dither_recipe.material_ssbo_binding = opaque_recipe.material_ssbo_binding;

        if (!CreateCubeGeometry())
            return false;

        mode_asset[0] = PrimitiveAsset(cube_geometry, &opaque_recipe, PrimitiveType::Triangles);
        mode_asset[1] = PrimitiveAsset(cube_geometry, &blend_recipe,  PrimitiveType::Triangles);
        mode_asset[2] = PrimitiveAsset(cube_geometry, &dither_recipe, PrimitiveType::Triangles);

        const float offset_y = (GRADIENT_COLUMNS - 1) * ITEM_SPACING * 0.5f;

        for (uint row = 0; row < MODE_ROWS; ++row)
        {
            for (uint col = 0; col < GRADIENT_COLUMNS; ++col)
            {
                char name[64];
                snprintf(name, sizeof(name), "Cube_%s_G%u", MODE_NAME[row], col);

                Entity * e = ecs_world->CreateEntity<Entity>(name);

                // ⚠ 世界 Z 轴向上（相机 forward.z=sin(pitch)、yaw 绕 Z）：横向摆 Y、纵向摆 Z
                const float y = col * ITEM_SPACING - offset_y;
                const float z = float(MODE_ROWS - 1 - row) * ROW_SPACING - ROW_SPACING;

                auto transform = ecs_world->GetTransform(
                    ecs_world->CreateTransform(e->GetEntityID(), Mobility::Static));
                transform.SetLocalPosition(glm::vec3(0.0f, y, z));
                transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                transform.SetLocalScale(glm::vec3(CUBE_SCALE, CUBE_SCALE, CUBE_SCALE));
                transform.SetMobility(Mobility::Static);
                item_transforms[row][col] = transform;

                auto * geometry_data = e->GetContext()->GetOrCreateGeometryData(e);
                hgl::ecs::MaterialData * material_data = e->GetContext()->GetOrCreateMaterialData(e);

                geometry_data->GetOwner()->GetContext()->GetOrCreateGeometryData(
                    geometry_data->GetOwnerID())->SetPrimitiveAsset(&mode_asset[row]);

                // 单通道 gradient 只当 **opacity_mask**（alpha 真值来源）：base_color 留空 ⇒
                // 采样回退 1.0 ⇒ 立方体是**亮实心**，三种方式的差别只体现在 alpha 行为上
                //（纹理内容 = 明暗渐变的 alpha 分布）。若同时绑 base_color，图案噪声会与
                // alpha 效果混在一起，反而看不清三种方式的差别。
                if (!material_data->SetTextureResource("opacity_mask", gradient_texture[col], sampler))
                    return false;

                hgl::ecs::MaterialData::MaterialDataAuthoringResource row_resource{};
                row_resource = surface_rows[row][col].GetGlobalSSBOBinding();
                material_data->SetDataResource(row_resource);
            }
        }

        printf("[AlphaModes] %u CUBE = %u 列（res/image/gradient 单通道 BC4 取样）× %u 行"
               "（上 %s / 中 %s / 下 %s）\n"
               "            单通道纹理只绑 opacity_mask（Lit 的 alpha 只出自 opacity_mask）\n"
               "            批预期：Opaque 与 AlphaBlend 只差 pipeline 配置（alpha_blend 是动态状态）\n"
               "                    ⇒ 同批；Dither 改生成 GLSL ⇒ 不同 shader program ⇒ 单独一批\n",
               ITEM_COUNT, GRADIENT_COLUMNS, MODE_ROWS,
               MODE_NAME[0], MODE_NAME[1], MODE_NAME[2]);
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
