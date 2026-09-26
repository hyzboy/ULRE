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
#include <hgl/vk/VKTextureReadback.h>
#include <hgl/vk/VKFormat.h>
#include <hgl/graph/geo/InlineGeometry.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/render/lighting/CascadedShadowController.h>
#include <hgl/color/Color.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/filesystem/Filename.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/2d/BitmapSave.h>

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
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cmath>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // D1 契约自检开关：`ATS_SELFCHECK=1`（或命令行 `--selfcheck`）→ 第 45 帧
    // 深度图判读后按契约退出码结束进程（0=PASS / 1=FAIL）；默认不退出，供人工
    // 看图。注意：自检路径用 std::exit，会跳过框架析构与对象泄漏检查——这是
    // 有意为之（一次性验证工具，退出码优先）。
    bool g_selfcheck = false;
    bool g_d3_noknob = false;

    // 深度图几何像素阈值：reversed-Z 下 clear=0、几何=近处亮；取 0.02 而非 0，
    // 避免把量化/滤波残差算成几何。
    constexpr float kDepthGeometryThreshold = 0.02f;

    // D1 契约判读带：相对"非零像素包围盒"的填充率。棋盘镂空实测 57.6%，
    // 实心(~100%)与全空(~0%)都在带外。
    constexpr float kContractFillMin = 0.50f;
    constexpr float kContractFillMax = 0.65f;

    // ── D3 契约：接收侧旋钮（receive_shadow / bias_multiplier）──────────────
    // 判定方式是**逐像素对比**三帧颜色（相机/场景静止，除旋钮外逐帧一致；
    // ATS_D3_NOKNOB=1 的对照组实测三帧逐像素完全相同 = 噪声底 0）。
    // 注意本场景地面是饱和红(R≈198,G≈1,B≈31)、影子压在其上呈灰蓝——
    // 所以用"是否变红"判"是否还在受影"，而不是用亮度（红的 RGB 均值反而
    // 比灰影低，用亮度会得出反方向结论）。
    constexpr int kColorDeltaChan = 40;   // 单通道变化阈值（0..255）
    constexpr int kRedGap = 60;           // 判为"红地面"的 R-G / R-B 下限

    // 接收开关：关掉后原影子区域应大面积变红（实测 ~1.9 万像素外观变化）
    constexpr uint32_t kD3ReceiveMinRedPixels = 3000;
    // 偏差倍率：用极端倍率证明它真的进入偏差计算（地面不自投影、本场景无 acne，
    // 故只能用"外观是否随倍率改变"作判据；死旋钮会是 0 —— 对照组即 0）
    constexpr uint32_t kD3BiasMinChangedPixels = 3000;

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

    // ── D1 契约：深度图镂空自动判读 ─────────────────────────────────────────
    // 判读口径：**相对非零（几何）像素包围盒**的填充率。整图口径不可用——该
    // 场景包围盒仅占全图 ~0.6%，57.6% 会被稀释成 0.4%。期望带 50–65%。
    struct DepthFillStats
    {
        uint32_t bbox_w = 0;
        uint32_t bbox_h = 0;
        uint32_t filled = 0;      // 几何像素总数（= 包围盒内填充数）
        float    ratio = 0.0f;    // filled / (bbox_w * bbox_h)
        bool     empty = true;    // 整图没有任何几何像素
    };

    bool depth_dumped = false;
    bool contract_done = false;   // c0 契约已判读
    bool contract_ok = false;     // c0 契约结果（selfcheck 退出码依据）

    // ── D3 契约状态 ──
    std::shared_ptr<ShadowComponent> ground_shadow;   // 地面（接收面）的阴影组件
    std::vector<uint8_t> d3_lum[3];                   // A/B/C 三帧亮度图
    int  d3_frame = 0;                                // 深度判读之后的相对帧号
    bool d3_done = false;
    bool d3_noknob = false;                           // ATS_D3_NOKNOB=1 → 对照组（只读回不改旋钮）
    bool d3_receive_ok = false;
    bool d3_bias_ok = false;

    // ── 取证落盘 ───────────────────────────────────────────────────────────
    // 文件名一律自带 **宽x高 + 数据格式**（`<stem>_<W>x<H>_<tag>.<ext>`），不让读的人靠猜：
    //   _f32.raw = 裸 float32（零转换、行紧排、小端）——数值分析用，numpy:
    //              np.fromfile('..._1024x1024_f32.raw', dtype='<f4').reshape(1024,1024)
    //   _r8.tga  = 8bit 灰度可视化副本（人眼看镂空/实心；量化会压平反 Z 的远景层次）——走 CM2D
    AnsiString MakeDumpName(const char *stem, uint32_t w, uint32_t h, const char *tag, const char *ext)
    {
        return AnsiString(stem) + "_" + AnsiString::numberOf(w) + "x" + AnsiString::numberOf(h)
             + "_" + tag + "." + ext;
    }

    /// 裸数据落盘（零转换：读回得到的字节原样写盘）
    bool SaveRaw(const char *path, const void *data, size_t bytes)
    {
        if (!data || !bytes)
            return false;

        return filesystem::SaveMemoryToFile(ToOSString(AnsiString(path)), data, static_cast<int64>(bytes))
            == static_cast<int64>(bytes);
    }

    /// 8bit **单通道**灰度 TGA（CM2D：channels=1 ⇒ image_type=3 / 8bpp）——深度可视化用，
    /// 与文件名里的 `_r8` 严格对应（1 通道 1 字节/像素，无 3 通道复制）。
    bool SaveGrayTga(const char *filename, uint8 *gray, uint32_t w, uint32_t h)
    {
        io::OpenFileOutputStream out(ToOSString(AnsiString(filename)), io::FileOpenMode::CreateTrunc);

        if (!out)
            return false;

        return bitmap::SaveBitmapToTGA(&out, gray, w, h, 1, 8);
    }

    /// 3 通道 8bit TGA（color 附件用）：行序自上而下（与读回顺序一致，无需翻转）。
    bool SaveRgbTga(const char *filename, uint8 *rgb, uint32_t w, uint32_t h)
    {
        io::OpenFileOutputStream out(ToOSString(AnsiString(filename)), io::FileOpenMode::CreateTrunc);

        if (!out)
            return false;

        return bitmap::SaveBitmapToTGA(&out, rgb, w, h, 3, 8);
    }

    // ── 深度图读回取证（用户建议：直接看 shadow map depth）──────────────────
    // 回读走引擎基础功能（graph::ReadbackDepthTarget）：暂存缓冲、布局转换、围栏与释放全在
    // 引擎侧；示例把 float 深度按原精度落裸 .raw，另存一份 8bit 灰度 .tga 供人眼判读
    //（8bit 足够分辨"镂空（clear 值）vs 实心"，但会压平反 Z 的远景层次 ⇒ 数值结论一律用 .raw）。
    // out_stats != nullptr 时顺带算出 D1 契约用的填充统计（同一遍扫描，零额外读回）。
    bool DumpCascadeDepth(graph::IRenderTarget *rt, const char *filename,
                          DepthFillStats *out_stats = nullptr)
    {
        if (!rt)
            return false;

        std::vector<uint8_t> pixels;
        graph::TextureReadbackInfo info;

        if (!graph::ReadbackDepthTarget(rt, pixels, &info))
        {
            GLogWarning("[DepthDump] %s 读回失败", filename);
            return false;
        }

        if (info.pixel_size != sizeof(float))
        {
            GLogWarning("[DepthDump] %s 深度每像素 %u 字节，本示例只处理 32F",
                        filename, info.pixel_size);
            return false;
        }

        const uint32_t w = info.width;
        const uint32_t h = info.height;
        const float *depth = reinterpret_cast<const float *>(pixels.data());

        std::vector<uint8> gray(static_cast<size_t>(w) * h, 0);

        // D1：几何像素统计（reversed-Z → clear=0、几何=近处亮）。单遍即可——包围盒
        // 内不会出现包围盒外的几何像素，故 filled 总数就是框内填充数。
        uint32_t min_x = w, min_y = h, max_x = 0, max_y = 0, filled = 0;

        for (uint32_t y = 0; y < h; ++y)
        {
            for (uint32_t x = 0; x < w; ++x)
            {
                const float d = depth[static_cast<size_t>(y) * w + x];
                const uint8 g = uint8((std::min)(std::max(d, 0.0f), 1.0f) * 255.0f);

                gray[static_cast<size_t>(y) * w + x] = g;

                if (d > kDepthGeometryThreshold)
                {
                    ++filled;
                    if (x < min_x) min_x = x;
                    if (x > max_x) max_x = x;
                    if (y < min_y) min_y = y;
                    if (y > max_y) max_y = y;
                }
            }
        }

        if (out_stats)
        {
            out_stats->filled = filled;
            out_stats->empty = (filled == 0);
            out_stats->bbox_w = out_stats->empty ? 0 : (max_x - min_x + 1);
            out_stats->bbox_h = out_stats->empty ? 0 : (max_y - min_y + 1);
            out_stats->ratio = out_stats->empty
                                   ? 0.0f
                                   : static_cast<float>(filled) /
                                         static_cast<float>(out_stats->bbox_w * out_stats->bbox_h);
        }

        // 裸 F32（零转换：直接写读回的 GPU 字节）+ 8bit 灰度可视化副本
        const AnsiString raw_name = MakeDumpName(filename, w, h, "f32", "raw");
        const AnsiString tga_name = MakeDumpName(filename, w, h, "r8", "tga");   // 单通道灰度

        if (!SaveRaw(raw_name.c_str(), depth, static_cast<size_t>(w) * h * sizeof(float)))
            return false;

        if (!SaveGrayTga(tga_name.c_str(), gray.data(), w, h))
            return false;

        GLogInfo("[DepthDump] %s saved: %s (F32 全精度) + %s (8bit 灰度可视化)",
                 filename, raw_name.c_str(), tga_name.c_str());
        return true;
    }

    // ── D3 契约：颜色读回（主帧 → 引擎回读 → TGA + 亮度图）────────────────────
    // 与 DumpCascadeDepth 同构，换成颜色附件；交换链颜色图的真实布局（PRESENT_SRC_KHR）
    // 由引擎在渲染结束时同步进纹理跟踪布局，示例不再硬编码布局。
    // 亮度图取 3 个低字节的均值：与 RGBA/BGRA 通道顺序无关（alpha 恒在第 4 字节），
    // 因此"变亮/变暗"的判定不需要知道具体格式；TGA 按低字节顺序写出，纯作人工看图。
    bool DumpColorTarget(graph::IRenderTarget *rt, const char *filename,
                         std::vector<uint8_t> *out_lum)
    {
        if (!rt)
            return false;

        std::vector<uint8_t> pixels;
        graph::TextureReadbackInfo info;

        if (!graph::ReadbackColorTarget(rt, pixels, 0, &info))
        {
            GLogWarning("[ColorDump] %s 读回失败", filename);
            return false;
        }

        const uint32_t w = info.width;
        const uint32_t h = info.height;

        if (w == 0 || h == 0 || info.pixel_size < 4)
            return false;

        std::vector<uint8> rgb(static_cast<size_t>(w) * h * 3, 0);

        if (out_lum)
            out_lum->assign(static_cast<size_t>(w) * h * 3, 0);

        uint64_t lum_sum = 0;

        for (uint32_t y = 0; y < h; ++y)
        {
            for (uint32_t x = 0; x < w; ++x)
            {
                const uint8 *p = pixels.data() + (static_cast<size_t>(y) * w + x) * info.pixel_size;
                const uint8 lum = uint8((uint32(p[0]) + uint32(p[1]) + uint32(p[2])) / 3);
                lum_sum += lum;

                if (out_lum)
                {
                    const size_t m = (static_cast<size_t>(y) * w + x) * 3;
                    (*out_lum)[m + 0] = p[2];   // R
                    (*out_lum)[m + 1] = p[1];   // G
                    (*out_lum)[m + 2] = p[0];   // B
                }

                uint8 *o = rgb.data() + (static_cast<size_t>(y) * w + x) * 3;
                o[0] = p[0];
                o[1] = p[1];
                o[2] = p[2];
            }
        }

        // 颜色附件的真实格式由引擎回传：本管线是 A2BGR10UN（10bit/通道打包进 32bit）
        // ⇒ 8bit 图像只是**截断视图**，所以要另存裸包（零转换，保住 10bit）。文件名同时
        // 标注源格式与截断方式，避免把截断图当成精确数据。
        const VulkanFormat *vf = GetVulkanFormat(info.format);
        const char *fmt_tag = vf ? vf->name : "unknown";

        const AnsiString raw_name = MakeDumpName(filename, w, h, fmt_tag, "raw");
        const AnsiString trunc_tag = AnsiString(fmt_tag) + "_low8x3";
        const AnsiString tga_name = MakeDumpName(filename, w, h, trunc_tag.c_str(), "tga");

        if (!SaveRaw(raw_name.c_str(), pixels.data(), pixels.size()))
            return false;

        if (!SaveRgbTga(tga_name.c_str(), rgb.data(), w, h))
            return false;

        GLogInfo("[ColorDump] %s saved: %s (原生 %s 打包) + %s (%ux%u 低 3 字节截断视图) mean_lum=%.1f",
                 filename, raw_name.c_str(), fmt_tag, tga_name.c_str(), w, h,
                 float(double(lum_sum) / double(static_cast<size_t>(w) * h)));

        return true;
    }

    // 逐像素对比两帧的**三通道外观**：
    //   changed     = 任一通道变化 >= kColorDeltaChan 的像素数（位置证据）
    //   grey_to_red = "受影(非红) → 未受影(红)"的像素数（方向证据）
    //   red_to_grey = 反向
    // 本场景地面是饱和红、影子压在其上呈灰蓝 → "变红"即"该像素不再受影"。
    // 相机与场景静止，除被改动的旋钮外逐帧一致（对照组实测逐像素相同），
    // 故差值只可能来自该旋钮。
    static void CompareAppearance(const std::vector<uint8_t> &a,
                                  const std::vector<uint8_t> &b,
                                  uint32_t &out_changed,
                                  uint32_t &out_grey_to_red,
                                  uint32_t &out_red_to_grey)
    {
        out_changed = 0;
        out_grey_to_red = 0;
        out_red_to_grey = 0;

        if (a.size() != b.size() || a.size() % 3 != 0)
            return;

        for (size_t i = 0; i < a.size(); i += 3)
        {
            const int ar = a[i], ag = a[i + 1], ab = a[i + 2];
            const int br = b[i], bg = b[i + 1], bb = b[i + 2];

            if (std::abs(br - ar) >= kColorDeltaChan ||
                std::abs(bg - ag) >= kColorDeltaChan ||
                std::abs(bb - ab) >= kColorDeltaChan)
                ++out_changed;

            const bool a_red = (ar - ag) >= kRedGap && (ar - ab) >= kRedGap;
            const bool b_red = (br - bg) >= kRedGap && (br - bb) >= kRedGap;

            if (!a_red && b_red)
                ++out_grey_to_red;
            else if (a_red && !b_red)
                ++out_red_to_grey;
        }
    }

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
            ground_shadow = shadow;   // D3 契约：运行期拨 receive_shadow / bias_multiplier

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
    void Tick(double delta) override
    {
        WorkObject::Tick(delta);

        // 深度图直接读回取证（第 45 帧，一次性）+ D1 契约自判
        if (!depth_dumped)
        {
            static int dump_frame = 0;
            if (++dump_frame >= 45)
            {
                depth_dumped = true;

                for (uint32_t c = 0; c < 2; ++c)
                {
                    auto *rt = environment_system->GetCascadeRenderTarget(c);
                    if (!rt)
                        continue;

                    const AnsiString fn = AnsiString("cascade_depth_c") +
                        AnsiString::numberOf(c);

                    DepthFillStats stats;
                    const bool dumped = DumpCascadeDepth(rt, fn.c_str(), &stats);

                    // D1 契约只判 c0：动态层含两个 cube（棋盘镂空 + 无 mask 实心）；
                    // c1 静态层在本场景无 caster（实测全空），判它无意义。
                    if (c != 0)
                        continue;

                    contract_done = true;

                    if (!dumped)
                    {
                        GLogError(u8"[D1-CONTRACT] c0 FAIL: 深度图读回失败");
                    }
                    else if (stats.empty)
                    {
                        GLogError(u8"[D1-CONTRACT] c0 FAIL: 深度图全空（片元被剥离后 alpha 恒 0，"
                                  u8"或 caster 未进入深度图）");
                    }
                    else if (stats.ratio < kContractFillMin)
                    {
                        GLogError(u8"[D1-CONTRACT] c0 FAIL: 包围盒填充率 %.1f%% < %.0f%% "
                                  u8"(bbox=%ux%u filled=%u) -- 镂空过度或几何缺失",
                                  stats.ratio * 100.0f, kContractFillMin * 100.0f,
                                  stats.bbox_w, stats.bbox_h, stats.filled);
                    }
                    else if (stats.ratio > kContractFillMax)
                    {
                        GLogError(u8"[D1-CONTRACT] c0 FAIL: 包围盒填充率 %.1f%% > %.0f%% "
                                  u8"(bbox=%ux%u filled=%u) -- mask 未生效，影子退化为实心",
                                  stats.ratio * 100.0f, kContractFillMax * 100.0f,
                                  stats.bbox_w, stats.bbox_h, stats.filled);
                    }
                    else
                    {
                        contract_ok = true;
                        GLogInfo(u8"[D1-CONTRACT] c0 PASS: bbox=%ux%u filled=%u 填充率 %.1f%% "
                                 u8"(期望 %.0f-%.0f%%)",
                                 stats.bbox_w, stats.bbox_h, stats.filled,
                                 stats.ratio * 100.0f,
                                 kContractFillMin * 100.0f, kContractFillMax * 100.0f);
                    }
                }

                if (g_selfcheck)
                {
                    // 自检模式不再立即退出：D1 结果记录后转入 D3 相位
                    //（接收侧旋钮端到端验证），最终由 D3 相位统一给出退出码。
                    GLogInfo(u8"[D1-CONTRACT] c0: %s（自检将在 D3 相位结束后退出）",
                             contract_ok ? "PASS" : "FAIL");
                }
            }
        }

        // ── D3 契约：接收侧旋钮（receive_shadow / bias_multiplier）端到端相位 ──
        // 三帧各读回一次颜色，**逐像素**与基准帧比较。相机与场景静止，除被改动的
        // 旋钮外逐帧一致（对照组 ATS_D3_NOKNOB=1 实测逐像素相同）→ 差值只能来自
        // 该旋钮；判据用"是否变红"而非亮度，理由见文件头常量处的注释。
        //   A（基准）  ：地面 ShadowComponent 默认值（接收 + 倍率 1.0）
        //   B（不接收）：SetReceiveShadow(false) → 地面上的影子应整体消失
        //                （影子区域大面积由"灰蓝"变回"红地面"）
        //   C（倍率无关）：SetReceiveShadow(true) + SetBiasMultiplier(1000×) →
        //                偏差被放大 1000 倍，影子必然改变（证明倍率进入了偏差计算）
        if (g_selfcheck && depth_dumped && !d3_done)
        {
            ++d3_frame;

            auto *main_rt = ecs_context ? ecs_context->GetRenderTarget() : nullptr;
            d3_noknob = g_d3_noknob;

            if (d3_frame == 1)
            {
                DumpColorTarget(main_rt, "ats_d3_A_receive_on", &d3_lum[0]);
            }
            else if (d3_frame == 2)
            {
                // ATS_D3_NOKNOB=1：对照组——三帧都读回、但**什么都不改**。
                // 用来测"读回+对比"这条链自身的噪声底：若三帧仍逐像素相同，
                // 则后续任何差异都只能来自旋钮；若不同，则读回不可信（换帧缓冲/
                // 与渲染竞争），必须先把工具修好再看结论。
                if (ground_shadow && !d3_noknob)
                {
                    ground_shadow->SetReceiveShadow(false);
                    GLogInfo(u8"[D3-ROW] 地面 receive_shadow -> %d, bias_multiplier = %.6f",
                             ground_shadow->CanReceiveShadow() ? 1 : 0,
                             ground_shadow->GetBiasMultiplier());
                }
            }
            else if (d3_frame == 4)
            {
                DumpColorTarget(main_rt, "ats_d3_B_receive_off", &d3_lum[1]);
            }
            else if (d3_frame == 5)
            {
                if (ground_shadow && !d3_noknob)
                {
                    // 极端倍率（1000×）：本场景地面**不自投影**（例子里显式
                    // SetCastShadow(false)），静态级联里没有地面自身，因此
                    // "倍率调小 → acne 重现"这条观察在这里不存在。改用极端倍率
                    // 证明倍率确实进入了偏差计算——死旋钮的像素变化恒为 0，
                    // 而对照组(ATS_D3_NOKNOB=1)实测就是 0。
                    ground_shadow->SetReceiveShadow(true);
                    ground_shadow->SetBiasMultiplier(1000.0f);
                    GLogInfo(u8"[D3-ROW] 地面 receive_shadow -> %d, bias_multiplier = %.6f",
                             ground_shadow->CanReceiveShadow() ? 1 : 0,
                             ground_shadow->GetBiasMultiplier());
                }
            }
            else if (d3_frame == 8)
            {
                DumpColorTarget(main_rt, "ats_d3_C_bias_x1000", &d3_lum[2]);
                d3_done = true;

                // 对照组（ATS_D3_NOKNOB=1）：三帧都读回、但一个旋钮都不拨。
                // 它自己也是断言：外观变化必须为 0——否则"读回+对比"这条链有噪声，
                // 后面任何旋钮差异都不能单独归因给旋钮。此处退出码 0 = 工具可信。
                if (d3_noknob)
                {
                    uint32_t c_ab = 0, g_ab = 0, r_ab = 0;
                    uint32_t c_ac = 0, g_ac = 0, r_ac = 0;
                    CompareAppearance(d3_lum[0], d3_lum[1], c_ab, g_ab, r_ab);
                    CompareAppearance(d3_lum[0], d3_lum[2], c_ac, g_ac, r_ac);
                    const bool clean = (c_ab + c_ac) == 0;
                    GLogInfo(u8"[D3-CONTRACT] 对照组(ATS_D3_NOKNOB=1) 噪声底 %s: "
                             u8"逐像素外观变化 %u px（A→B %u / A→C %u，期望 0）",
                             clean ? "PASS" : "FAIL", c_ab + c_ac, c_ab, c_ac);
                    GLogInfo(u8"[D3-CONTRACT] selfcheck: %s (control, exit %d)",
                             clean ? "PASS" : "FAIL", clean ? 0 : 1);
                    std::exit(clean ? 0 : 1);
                }

                uint32_t changed = 0, g2r = 0, r2g = 0;
                CompareAppearance(d3_lum[0], d3_lum[1], changed, g2r, r2g);
                d3_receive_ok = (g2r >= kD3ReceiveMinRedPixels)
                             && (r2g <= g2r / 4);

                uint32_t bias_changed = 0, bias_g2r = 0, bias_r2g = 0;
                CompareAppearance(d3_lum[0], d3_lum[2], bias_changed, bias_g2r, bias_r2g);
                d3_bias_ok = (bias_changed >= kD3BiasMinChangedPixels);

                // 三次读回两两完全一致 ⇒ 更可能是读回本身失败（布局/同步），
                // 而不是"旋钮无效"——单独报出来，避免把工具问题当成引擎结论。
                if ((changed + bias_changed) == 0)
                    GLogError(u8"[D3-CONTRACT] 三帧读回两两逐像素相同——请先确认颜色读回"
                              u8"（PRESENT_SRC→TRANSFER_SRC）是否真的取到了画面，再看旋钮；"
                              u8"若本来就是对照组(ATS_D3_NOKNOB=1)，0 变化正是预期");

                GLogInfo(u8"[D3-CONTRACT] receive_shadow %s: 关掉后 受影→未受影(变红) %u px / "
                         u8"未受影→受影 %u px（同帧外观变化共 %u px；期望变红 >= %u 且反向 <= 变红/4）",
                         d3_receive_ok ? "PASS" : "FAIL", g2r, r2g, changed,
                         kD3ReceiveMinRedPixels);
                GLogInfo(u8"[D3-CONTRACT] bias_multiplier %s: 倍率×1000 后外观变化 %u px"
                         u8"（变红 %u / 变灰 %u；期望 >= %u，方向随 bias 符号而定）",
                         d3_bias_ok ? "PASS" : "FAIL", bias_changed, bias_g2r, bias_r2g,
                         kD3BiasMinChangedPixels);

                const bool ok = contract_done && contract_ok
                             && d3_receive_ok && d3_bias_ok;
                GLogInfo(u8"[D3-CONTRACT] selfcheck: %s (exit %d)",
                         ok ? "PASS" : "FAIL", ok ? 0 : 1);
                std::exit(ok ? 0 : 1);
            }
        }
    }

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
        GLogInfo(u8"D1 契约: 第 45 帧读回 c0 深度图并判读包围盒内填充率(期望 50-65%%); "
                 u8"设 ATS_SELFCHECK=1 或加 --selfcheck 则按契约退出码结束(0=PASS/1=FAIL)");
        return true;
    }
};

int os_main(int argc, os_char **argv)
{
    // D1 契约自检：ATS_SELFCHECK=1（或命令行 --selfcheck）→ 第 45 帧深度图判读后
    // 按契约退出码结束进程（0=通过 / 1=失败），便于脚本化回归；不带则保持交互，
    // 供人工看图。
    if (const char *env = std::getenv("ATS_SELFCHECK"))
        g_selfcheck = (env[0] != '\0' && env[0] != '0');

    for (int i = 1; i < argc; ++i)
        if (argv[i] && OSString(argv[i]) == OSString(OS_TEXT("--selfcheck")))
            g_selfcheck = true;

    // D3 对照组：照常读回三帧颜色，但不拨任何旋钮——用于确认"读回+逐像素对比"
    // 这条链的噪声底为 0（三帧逐像素相同），否则任何差异都不能归因于旋钮。
    if (const char *env = std::getenv("ATS_D3_NOKNOB"))
        g_d3_noknob = (env[0] != '\0' && env[0] != '0');

    return RunFramework<AlphaTestShadowApp>(
        OS_TEXT("Alpha Test Shadow (masked vs fallback-opaque cascade shadow)"),
        argc, argv, 1280, 720);
}
