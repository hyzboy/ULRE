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
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

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
    constexpr float kShadowNormalOffsetWorld = 0.10f;  // 米；用户实测最佳值（按键微调确立；消掠射角 acne 且不导致悬浮）
    constexpr float kShadowBiasWorld         = -0.20f; // 米；负值 = 贴合遮挡体（避免过负导致 Cube/球受光面自遮挡；运行时可用 [ / ] 调）
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
    Texture2DArray *alpha_base_texture = nullptr; // A1-4: alpha test 物体专用（1 层 RGBA8 棋盘）
    Sampler *pbr_sampler = nullptr;

    Geometry *builtin_geometries[kBuiltinGeomCount]{};
    PrimitiveAsset builtin_primitives[kBuiltinGeomCount]{};

    Geometry *ground_geometry = nullptr;
    PrimitiveAsset ground_primitive{};
    Entity *ground_entity = nullptr;
    std::shared_ptr<TransformComponent> ground_transform;
    std::shared_ptr<PrimitiveComponent> ground_prim;

    Geometry *alpha_geometry = nullptr;
    PrimitiveAsset alpha_primitive{};

    graph::mtl::MaterialRecipe lit_recipe{};
    graph::mtl::MaterialRecipe alpha_recipe{}; // A1-4: alpha_test=true → ShadowCasterMasked 路由
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

    // ── CSM 滚动缓存窗口统计（每帧采样 controller 的 CascadeUpdateStats，1s 汇总）──
    // 判读：strips>0 ⇒ 环形滚动条带；fulls>0 ⇒ 整级重建；两者皆 0 ⇒ 纯命中（零绘制）。
    struct CacheWindow
    {
        uint32_t strips       = 0;  // 窗口内条带矩形数累计
        uint32_t strip_texels = 0;  // 窗口内条带面积累计（texel）
        uint32_t frames       = 0;  // 窗口内采样帧数
        uint32_t map_texels   = 0;  // 该级贴图纹素数（供窗口平均重画占比）
        uint32_t last_off_x   = 0;  // 最近一帧环形偏移（texel）
        uint32_t last_off_y   = 0;
        uint32_t band_texels  = 0;  // 该级配置的锚定步长 B（texel）
        // 单调计数差分（免疫"同帧多次 Update 覆盖 GetUpdateStats"盲点）：本窗口真实次数
        uint32_t call_full    = 0;
        uint32_t call_strip   = 0;
        uint32_t call_hit     = 0;
    };
    CacheWindow cache_win[kMaxShadowCascades];

    // 单调计数器差分基准（Update()/InvalidateStaticCache() 累计调用次数）
    uint32_t cache_prev_calls = 0;
    uint32_t cache_prev_invs  = 0;
    uint32_t cache_prev_full[kMaxShadowCascades]  = {};
    uint32_t cache_prev_strip[kMaxShadowCascades] = {};
    uint32_t cache_prev_hit[kMaxShadowCascades]   = {};
    uint32_t win_calls = 0;   // 本窗口 Update 调用累计（> frames ⇒ 每帧被多次调用）
    uint32_t win_invs  = 0;   // 本窗口 InvalidateStaticCache 调用累计

    // ── S5：整级 vs 条带 深度图对拍（`CSM_CACHE_DIFF=1`，配 `CSM_AUTOWALK=<m/s>`）──
    // 条带/滚动是 GPU 侧增量：CPU 契约（Test 17/18）看不到光栅化结果与 scissor 坐标系。
    // 本诊断在同一相机、同一静态内容下取两张物理深度图：
    //   A) 环形滚动帧（offset≠0：内容 = 旧内容 + 新暴露条带）
    //   B) 强制整级重建帧（InvalidateMainLightStaticShadowCache，offset=0）
    // 按读侧映射比对：A 的物理 ((x+Ox) mod M, (y+Oy) mod M) 必须等于 B 的 (x, y)
    // （静态内容与布局矩阵都没变）。差异若**成片**出现 = 条带漏画/错位（scissor
    // 坐标、写侧平移、偏移累加之一错）；若**只落在剪影边** = 平移过的投影矩阵在
    // 光栅化时非位精确（顶点投影被扰动 ⇒ 边函数取整不同），属预期。
    // 判据：B 的 3x3 邻域深度跨度 > kCacheDiffEdgeEps 即算"剪影边"。
    static constexpr float kCacheDiffEdgeEps = 1.0e-3f;

    struct CacheDiff
    {
        bool  enabled  = false;
        bool  freeze   = true;    // 对拍期间冻结相机与可移动物体动画（内容不变是比对前提）
        float autowalk = 0.0f;   // 主相机自动前进速度（m/s，沿 +x）；0 = 关
        int    state    = 0;      // 0=等滚动帧 1=已读回A 2=等整级重建帧 3=自查B vs B2
        uint32_t rounds = 0;      // 已完成的轮数
        uint32_t frame_no = 0;                    // 每帧 ++（年龄基准）
        uint32_t full_seen[4] = {};               // 上次观察到的该级整级重建累计计数
        uint32_t last_full_frame[4] = {};         // 该级最近一次整级重建发生的帧号
        uint32_t strip_at_full[4] = {};           // 上次整级重建时的条带累计计数
        uint32_t upd_at_full[4] = {};             // 上次整级重建时的 Update 累计计数
        bool     pending = false;                 // 已看到条带帧（已停走），等静置结束
        uint32_t walk_target = 1;                 // 本轮要先走过去几次跨格再取 A（逐轮 +1，覆盖环形回绕）
        uint32_t walk_done = 0;                   // 本轮已跨格次数
        uint32_t strip_seen = 0;                  // c1 条带累计计数快照（探测跨格事件）
        uint32_t settle = 0;                      // 停走后的静置帧计数（读回前先稳定）
        uint32_t age_frames[4]  = {};             // A 帧：距上次整级重建的帧数
        uint32_t age_strips[4]  = {};             // A 帧：自上次重建以来的条带次数
        uint32_t age_updates[4] = {};             // A 帧：自上次重建以来的 Update 次数
        uint32_t rolling_mask = 0;                    // A 帧处于条带滚动的级联位掩码
        uint32_t offset[4][2] = {};                   // A 帧各级环形偏移（texel）
        std::vector<float> roll[4];                   // A：滚动帧深度（物理贴图）
        std::vector<float> full[4];                   // B：整级重建帧深度
    };
    CacheDiff cache_diff;

    // ── 阴影深度 bias（背面渲染的贴合补偿，运行时可调）──
    CascadedShadowConfig csm_config{};
    float cascade_radius0 = 0.0f; // 级联 0 包围球半径（供上层换算世界单位用）
    // 各级联正交投影的深度范围（米），由 RenderCSM 里那次真实 Update 记录；
    // bias_world 除以它即得该级需要写入的归一化 bias
    float cascade_depth_range[kMaxShadowCascades]{};
    bool bias_key_prev[2]{};      // [0]=[ 减小, [1]=] 增大，边沿触发
    bool no_key_prev[2]{};        // [0]=- 减小, [1]== 增大（normal-offset），边沿触发
    bool f_key_prev[4]{};         // F1..F4 级联屏蔽切换边沿触发
    bool r_key_prev = false;      // R：手动失效静态级联缓存（A3 API 演示）

private:

    /// 读回某级联深度图（物理贴图，float 行主序 y*M+x）。诊断用：immediate submit。
    bool ReadbackCascadeDepth(graph::IRenderTarget *rt, std::vector<float> &out)
    {
        auto *gc = GetGraphicsContext();
        auto *device = gc ? gc->GetDevice() : nullptr;
        if (!device || !rt)
            return false;

        auto *tex = rt->GetDepthTexture();
        if (!tex)
            return false;

        const uint32_t w = tex->GetWidth();
        const uint32_t h = tex->GetHeight();
        const VkDeviceSize bytes = VkDeviceSize(w) * h * sizeof(float);
        out.assign(static_cast<size_t>(w) * h, 0.0f);

        device->WaitIdle();   // 诊断：先在途帧收尾再动这张图（一次性开销，可接受）

        auto *staging = device->CreateBuffer(
            ObjectNameBuilder(AnsiString("CascadeShadowMap:CacheDiff")),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT, bytes, bytes, nullptr,
            BufferAllocPolicy::Readback, SharingMode::Exclusive);
        if (!staging)
            return false;

        VkCommandBuffer cmd = device->CreateCommandBuffer(
            AnsiString("CascadeShadowMap:CacheDiffCmd"));
        if (!cmd)
        {
            delete staging;
            return false;
        }

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);

        const VkImageLayout cur_layout = tex->GetImageLayout() != VK_IMAGE_LAYOUT_UNDEFINED
                                             ? tex->GetImageLayout()
                                             : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

        VkImageMemoryBarrier to_src{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        to_src.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        to_src.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        to_src.oldLayout = cur_layout;
        to_src.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to_src.image = tex->GetImage();
        to_src.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_src);

        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
        region.imageExtent = {w, h, 1};
        vkCmdCopyImageToBuffer(cmd, tex->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               staging->GetBuffer(), 1, &region);

        VkImageMemoryBarrier to_attach = to_src;
        to_attach.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        to_attach.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        to_attach.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to_attach.newLayout = cur_layout;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &to_attach);

        vkEndCommandBuffer(cmd);

        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence fence;
        vkCreateFence(device->GetDevice(), &fi, nullptr, &fence);

        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        vkQueueSubmit(device->GetGraphicsQueue(), 1, &si, fence);
        vkWaitForFences(device->GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(device->GetDevice(), fence, nullptr);

        bool ok = false;
        if (const float *src = static_cast<const float *>(staging->GetGPUBuffer()->Map(0, bytes)))
        {
            std::memcpy(out.data(), src, static_cast<size_t>(bytes));
            staging->GetGPUBuffer()->Unmap();
            ok = true;
        }

        delete staging;
        return ok;
    }

    /// S5 对拍状态机（Tick 每帧调用；`CSM_CACHE_DIFF=1` 才活）。
    /// 完成后自动回到 state 0 ⇒ 一轮跑多级/多次（每次抓到"恰好滚动的那几级"）。
    void RunCacheDiff()
    {
        if (!cache_diff.enabled || !environment_system)
            return;

        auto *ctrl = environment_system->GetShadowController();
        if (!ctrl)
            return;

        // 年龄基准：每当某级的"整级重建"累计计数增加，就记录该帧与当时的条带/Update 计数。
        // 用来自证"对拍差异是否随缓存年龄增长"——若差异随年龄增长 ⇒ 缓存在渐进腐化；
        // 若固定量级 ⇒ 每次跨格只注入同等小误差。
        ++cache_diff.frame_no;
        for (uint32_t c = 1; c < kMaxShadowCascades; ++c)
        {
            const uint32_t fc = ctrl->GetFullUpdateCallCount(c);
            if (fc != cache_diff.full_seen[c])
            {
                cache_diff.full_seen[c]       = fc;
                cache_diff.last_full_frame[c] = cache_diff.frame_no;
                cache_diff.strip_at_full[c]   = ctrl->GetStripUpdateCallCount(c);
                cache_diff.upd_at_full[c]     = ctrl->GetUpdateCallCount();
            }
        }

        if (cache_diff.state == 0)   // 等"至少一级处于条带滚动"
        {
            uint32_t mask = 0;
            for (uint32_t c = 1; c < kMaxShadowCascades; ++c)
            {
                const auto &s = ctrl->GetUpdateStats(c);
                if (s.strip_count > 0 && (s.offset.x != 0 || s.offset.y != 0))
                    mask |= (1u << c);
            }
            if (!cache_diff.pending)
            {
                // 逐轮把"先走过去的跨格次数"递增（1,2,3,…），让 A 帧的环形偏移逐轮
                // 变成 16,32,48,… 直到越过贴图尺寸发生**回绕**——否则每轮都被强制整级
                // 重建清零，永远只测到 offset=16 这一种位置。
                cache_diff.walk_target = cache_diff.rounds + 1 < 70 ? cache_diff.rounds + 1 : 70;
                const uint32_t sc = ctrl->GetStripUpdateCallCount(1);
                if (cache_diff.walk_done == 0 && cache_diff.strip_seen == 0)
                    cache_diff.strip_seen = sc;                  // 本轮起点
                cache_diff.walk_done = sc - cache_diff.strip_seen;   // 本轮已跨格次数
                if (cache_diff.walk_done < cache_diff.walk_target)
                    return;                                       // 继续前进（mask 可能为 0，无妨）

                if (!mask)
                    return;                                       // 刚跨格但本帧没有可取的条带状态，下帧再来

                // 关键：行走中直接读回会"内容跨帧错配"——读回的深度图是"停走前那一帧"
                // 渲染的，而 offset 状态可能已经又跨了一格 ⇒ 对拍退化成整图错位（假差异）。
                // 所以达到目标跨格数后立刻停走（pending=true），静置 5 帧（相机与光照盒
                // 都不动）再读回。
                cache_diff.rolling_mask = mask;
                cache_diff.pending = true;
                cache_diff.settle = 0;
                return;
            }

            if (cache_diff.settle < 5)
            {
                ++cache_diff.settle;
                return;
            }
            cache_diff.pending = false;
            cache_diff.settle = 0;
            const uint32_t roll_mask = cache_diff.rolling_mask;

            for (uint32_t c = 1; c < kMaxShadowCascades; ++c)
            {
                if ((roll_mask & (1u << c)) == 0)
                    continue;
                auto *rt = environment_system->GetCascadeRenderTarget(c);
                if (!rt || !ReadbackCascadeDepth(rt, cache_diff.roll[c]))
                    return;                       // 读回失败：留在 state 0，下帧重试

                const auto &s = ctrl->GetUpdateStats(c);
                cache_diff.offset[c][0] = s.offset.x;
                cache_diff.offset[c][1] = s.offset.y;
                cache_diff.age_frames[c]  = cache_diff.frame_no - cache_diff.last_full_frame[c];
                cache_diff.age_strips[c]  = ctrl->GetStripUpdateCallCount(c) - cache_diff.strip_at_full[c];
                cache_diff.age_updates[c] = ctrl->GetUpdateCallCount() - cache_diff.upd_at_full[c];
            }

            cache_diff.rolling_mask = mask;
            cache_diff.state = 1;
            GLogInfo(u8"[CSM-CACHE-DIFF] A 帧已读回：滚动级联掩码=0x%X offset c1=(%u,%u) c2=(%u,%u) c3=(%u,%u) "
                     u8"年龄 c1=%u帧/%u条带/%uUpd c2=%u帧/%u条带/%uUpd c3=%u帧/%u条带/%uUpd",
                     roll_mask, cache_diff.offset[1][0], cache_diff.offset[1][1],
                     cache_diff.offset[2][0], cache_diff.offset[2][1],
                     cache_diff.offset[3][0], cache_diff.offset[3][1],
                     cache_diff.age_frames[1], cache_diff.age_strips[1], cache_diff.age_updates[1],
                     cache_diff.age_frames[2], cache_diff.age_strips[2], cache_diff.age_updates[2],
                     cache_diff.age_frames[3], cache_diff.age_strips[3], cache_diff.age_updates[3]);
            return;
        }

        if (cache_diff.state == 1)
        {
            // 相机保持不动（本轮不再自动前进），强制下一帧整级重建、offset 归零
            environment_system->InvalidateMainLightStaticShadowCache();
            cache_diff.state = 2;
            return;
        }

        if (cache_diff.state == 2)   // 等整级重建帧落地（full_update 且 offset==0）
        {
            uint32_t need = 0, ready = 0;
            for (uint32_t c = 1; c < kMaxShadowCascades; ++c)
            {
                if ((cache_diff.rolling_mask & (1u << c)) == 0)
                    continue;
                ++need;
                const auto &s = ctrl->GetUpdateStats(c);
                if (s.full_update && s.offset.x == 0 && s.offset.y == 0)
                    ++ready;
            }
            if (ready < need)
                return;

            const uint32_t M = kShadowMapSize;
            uint32_t total_mismatch = 0;
            uint32_t total_flat = 0;

            for (uint32_t c = 1; c < kMaxShadowCascades; ++c)
            {
                if ((cache_diff.rolling_mask & (1u << c)) == 0)
                    continue;

                auto *rt = environment_system->GetCascadeRenderTarget(c);
                if (!rt || !ReadbackCascadeDepth(rt, cache_diff.full[c]))
                    return;

                const auto &A = cache_diff.roll[c];
                const auto &B = cache_diff.full[c];
                if (A.size() != B.size() || B.size() != static_cast<size_t>(M) * M)
                {
                    GLogError(u8"[CSM-CACHE-DIFF] c=%u 读回尺寸不符（A=%zu B=%zu，期望 %u）",
                              c, A.size(), B.size(), M * M);
                    cache_diff.state = 0;
                    return;
                }

                const uint32_t ox = cache_diff.offset[c][0] % M;
                const uint32_t oy = cache_diff.offset[c][1] % M;

                uint32_t mismatch = 0;
                uint32_t edge_mismatch = 0;   // 差异落在深度跳变处（剪影边）
                uint32_t flat_mismatch = 0;   // 差异落在平坦区（= 内容真缺失/错位）
                uint32_t a_missing = 0;       // A 空、B 有几何 ⇒ 条带该画没画
                uint32_t a_extra = 0;         // A 有几何、B 空 ⇒ A 多了内容（旧内容残留？）
                uint32_t both_geom = 0;       // 两边都有几何但深度不同
                float max_abs = 0.0f;
                uint32_t bx0 = M, by0 = M, bx1 = 0, by1 = 0;

                // 差异的 32x32 格分布：成片 / 成线 / 散布一眼可辨（每格 M/32 纹素）
                constexpr uint32_t kGridN = 32;
                const uint32_t cell = (M + kGridN - 1) / kGridN;
                uint32_t cells[kGridN][kGridN] = {};

                for (uint32_t y = 0; y < M; ++y)
                {
                    const uint32_t ay = (y + oy) % M;
                    for (uint32_t x = 0; x < M; ++x)
                    {
                        const uint32_t ax = (x + ox) % M;
                        const float a = A[static_cast<size_t>(ay) * M + ax];
                        const float b = B[static_cast<size_t>(y) * M + x];
                        const float d = (std::fabs)(a - b);
                        if (d > 1.0e-6f)
                        {
                            ++mismatch;
                            if (d > max_abs)
                                max_abs = d;
                            if (x < bx0) bx0 = x;
                            if (x > bx1) bx1 = x;
                            if (y < by0) by0 = y;
                            if (y > by1) by1 = y;

                            // 分类：取 B 的 3x3 邻域深度跨度。跨度大 = 该处是深度跳变
                            // （剪影边），平移过的投影矩阵在光栅化时本就不保证位精确
                            // （顶点投影被扰动 ⇒ 边函数取整不同）；跨度小仍在差异才是
                            // "整片内容写错"的特征（条带漏画/错位/scissor 错）。
                            float bmin = b, bmax = b;
                            for (int dy = -1; dy <= 1; ++dy)
                            {
                                const uint32_t yy = static_cast<uint32_t>((static_cast<int>(y) + dy + M) % M);
                                for (int dx = -1; dx <= 1; ++dx)
                                {
                                    const uint32_t xx = static_cast<uint32_t>((static_cast<int>(x) + dx + M) % M);
                                    const float v = B[static_cast<size_t>(yy) * M + xx];
                                    if (v < bmin) bmin = v;
                                    if (v > bmax) bmax = v;
                                }
                            }

                            if (bmax - bmin > kCacheDiffEdgeEps)
                            {
                                ++edge_mismatch;
                            }
                            else
                            {
                                ++flat_mismatch;
                            }

                            // 三分类：反 Z 下 0 = 空（清屏/远平面）
                            const bool a_empty = a <= 1.0e-6f;
                            const bool b_empty = b <= 1.0e-6f;
                            if (a_empty && !b_empty)
                            {
                                ++a_missing;
                            }
                            else if (!a_empty && b_empty)
                            {
                                ++a_extra;
                            }
                            else
                            {
                                ++both_geom;
                            }

                            ++cells[y / cell < kGridN ? y / cell : kGridN - 1]
                                   [x / cell < kGridN ? x / cell : kGridN - 1];
                        }
                    }
                }

                total_mismatch += mismatch;
                total_flat += flat_mismatch;

                if (flat_mismatch > 0)
                {
                    // 注意：GLog* 展开为 {...} 块 ⇒ 这里必须显式花括号（否则 else 报 C2181）
                    GLogError(u8"[CSM-CACHE-DIFF] c=%u offset=(%u,%u) 纹素=%u 不一致=%u(剪影边 %u / 平坦区 %u) 形状分类[A缺 %u / A多 %u / 双方有几何 %u] max|Δ|=%.6e bbox=(%u,%u)-(%u,%u) ⇒ 平坦区有差异，内容真的错位/缺失",
                              c, ox, oy, M * M, mismatch, edge_mismatch, flat_mismatch,
                              a_missing, a_extra, both_geom, max_abs, bx0, by0, bx1, by1);

                    // 差异分布图（32x32 格）：`.`=0，`1..9`=个位，`a..z`=10..35，`#`=>35
                    for (uint32_t gy = 0; gy < kGridN; ++gy)
                    {
                        char line[kGridN + 1];
                        for (uint32_t gx = 0; gx < kGridN; ++gx)
                        {
                            const uint32_t v = cells[gy][gx];
                            line[gx] = (v == 0) ? '.'
                                                : (v < 10 ? static_cast<char>('0' + v)
                                                          : (v <= 35 ? static_cast<char>('a' + v - 10) : '#'));
                        }
                        line[kGridN] = '\0';
                        GLogInfo(u8"[CSM-CACHE-DIFF] c=%u 分布[%02u] %s", c, gy, line);
                    }

                    // 落图看形态：A 按偏移映射回布局坐标系，B 原样，D = |A-B|×5
                    std::vector<float> mapped(A.size(), 0.0f);
                    std::vector<float> diff(A.size(), 0.0f);
                    for (uint32_t y = 0; y < M; ++y)
                    {
                        for (uint32_t x = 0; x < M; ++x)
                        {
                            const float a = A[static_cast<size_t>((y + oy) % M) * M + ((x + ox) % M)];
                            const float b = B[static_cast<size_t>(y) * M + x];
                            mapped[static_cast<size_t>(y) * M + x] = a;
                            const float dv = (std::fabs)(a - b) * 5.0f;
                            diff[static_cast<size_t>(y) * M + x] = dv > 1.0f ? 1.0f : dv;
                        }
                    }

                    char fn[256];
                    snprintf(fn, sizeof(fn), "csm_cachediff_c%u_A.bmp", c);
                    SaveDepthBmp(fn, mapped, M);
                    snprintf(fn, sizeof(fn), "csm_cachediff_c%u_B.bmp", c);
                    SaveDepthBmp(fn, B, M);
                    snprintf(fn, sizeof(fn), "csm_cachediff_c%u_D.bmp", c);
                    SaveDepthBmp(fn, diff, M);
                    GLogWarning(u8"[CSM-CACHE-DIFF] c=%u 已落图 csm_cachediff_c%u_{A,B,D}.bmp（A=按偏移映射 / B=整级重建 / D=差异x5）",
                                c, c);

                    // 位移扫描：差异能否用一个整体整数位移解释？（沿 x/y 各扫 ±24：
                    // 条带宽 B=16，若"清晰区与内容错开一个条带"则应命中 ±B）
                    const uint32_t wx0 = bx0 > 4 ? bx0 - 4 : 0;
                    const uint32_t wy0 = by0 > 4 ? by0 - 4 : 0;
                    const uint32_t wx1 = (std::min)(bx1 + 4, M - 1);
                    const uint32_t wy1 = (std::min)(by1 + 4, M - 1);

                    auto count_at = [&](int dx, int dy) -> uint32_t {
                        uint32_t cnt = 0;
                        for (uint32_t y = wy0; y <= wy1; ++y)
                        {
                            for (uint32_t x = wx0; x <= wx1; ++x)
                            {
                                const uint32_t ax = static_cast<uint32_t>(
                                    (static_cast<int>(x) + static_cast<int>(ox) + dx + 8 * static_cast<int>(M)) % static_cast<int>(M));
                                const uint32_t ay = static_cast<uint32_t>(
                                    (static_cast<int>(y) + static_cast<int>(oy) + dy + 8 * static_cast<int>(M)) % static_cast<int>(M));
                                if ((std::fabs)(A[static_cast<size_t>(ay) * M + ax] - B[static_cast<size_t>(y) * M + x]) > 1.0e-6f)
                                    ++cnt;
                            }
                        }
                        return cnt;
                    };

                    const uint32_t cnt0 = count_at(0, 0);
                    uint32_t best_cnt = cnt0;
                    int best_dx = 0, best_dy = 0;

                    for (int d = -24; d <= 24; ++d)
                    {
                        if (d == 0)
                            continue;

                        const uint32_t cx = count_at(d, 0);
                        if (cx < best_cnt)
                        {
                            best_cnt = cx;
                            best_dx = d;
                            best_dy = 0;
                        }

                        const uint32_t cy = count_at(0, d);
                        if (cy < best_cnt)
                        {
                            best_cnt = cy;
                            best_dx = 0;
                            best_dy = d;
                        }
                    }

                    const uint32_t window = (wx1 - wx0 + 1) * (wy1 - wy0 + 1);
                    GLogInfo(u8"[CSM-CACHE-DIFF] c=%u 年龄=%u帧/%u条带 位移扫描（窗口 %u 纹素，x/y 各 ±24）：d=0 不一致=%u(%.1f%%) / 最优位移=(%d,%d) 残余=%u(%.1f%%)",
                             c, cache_diff.age_frames[c], cache_diff.age_strips[c],
                             window, cnt0, 100.0f * static_cast<float>(cnt0) / static_cast<float>(window),
                             best_dx, best_dy, best_cnt,
                             100.0f * static_cast<float>(best_cnt) / static_cast<float>(window));
                }
                else
                {
                    GLogInfo(u8"[CSM-CACHE-DIFF] c=%u 年龄=%u帧/%u条带/%uUpd offset=(%u,%u) 纹素=%u 不一致=%u(全部落在剪影边，平坦区 0) max|Δ|=%.6e ⇒ 条带路径与整级重建几何一致",
                             c, cache_diff.age_frames[c], cache_diff.age_strips[c], cache_diff.age_updates[c],
                             ox, oy, M * M, mismatch, max_abs);
                }

                cache_diff.roll[c].clear();
                cache_diff.roll[c].shrink_to_fit();
                // full[c] 留给 state 3 自查用，暂不清
            }

            ++cache_diff.rounds;
            cache_diff.walk_done = 0;
            cache_diff.strip_seen = 0;
            if (total_flat > 0)
            {
                GLogError(u8"[CSM-CACHE-DIFF] 第 %u 轮汇总：不一致纹素总数=%u（平坦区 %u）⇒ 条带路径有问题（查 scissor 坐标/写侧平移/偏移方向）",
                          cache_diff.rounds, total_mismatch, total_flat);
            }
            else
            {
                GLogInfo(u8"[CSM-CACHE-DIFF] 第 %u 轮汇总：不一致纹素总数=%u（平坦区 0）⇒ 差异只在剪影边（平移矩阵下光栅化非位精确，预期行为；条带路径无内容缺失/错位）",
                         cache_diff.rounds, total_mismatch);
            }
            // 自查：紧接着再强一次整级重建取 B2，比 B vs B2。
            // 同一相机、同一内容时两次整级重建必须逐纹素相同；若这里也有差异
            // ⇒ 帧间内容在变（如可移动物体动画），上面 A vs B 的数字就不作数。
            cache_diff.state = 3;
            environment_system->InvalidateMainLightStaticShadowCache();
            return;
        }

        if (cache_diff.state == 3)
        {
            uint32_t need = 0, ready = 0;
            for (uint32_t c = 1; c < kMaxShadowCascades; ++c)
            {
                if ((cache_diff.rolling_mask & (1u << c)) == 0)
                    continue;
                ++need;
                const auto &s = ctrl->GetUpdateStats(c);
                if (s.full_update && s.offset.x == 0 && s.offset.y == 0)
                    ++ready;
            }
            if (ready < need)
                return;

            const uint32_t M = kShadowMapSize;
            uint32_t self_mismatch = 0, self_edge = 0, self_flat = 0;

            for (uint32_t c = 1; c < kMaxShadowCascades; ++c)
            {
                if ((cache_diff.rolling_mask & (1u << c)) == 0)
                    continue;

                auto *rt = environment_system->GetCascadeRenderTarget(c);
                std::vector<float> b2;
                if (!rt || !ReadbackCascadeDepth(rt, b2))
                    return;

                uint32_t mism = 0, edge = 0, flat = 0;
                float mx = 0.0f;
                CountDepthDiff(cache_diff.full[c], b2, M, 0, 0, mism, edge, flat, mx);
                self_mismatch += mism;
                self_edge += edge;
                self_flat += flat;
                GLogInfo(u8"[CSM-CACHE-DIFF] 自查 c=%u（连续两次整级重建、偏移 0）：不一致=%u(剪影边 %u / 平坦区 %u) max|Δ|=%.6e",
                         c, mism, edge, flat, mx);

                cache_diff.full[c].clear();
                cache_diff.full[c].shrink_to_fit();
            }

            if (self_flat > 0)
            {
                GLogError(u8"[CSM-CACHE-DIFF] 自查汇总：不一致=%u（平坦区 %u）⇒ 帧间内容在变（动画/矩阵漂移），A vs B 的数字不能当条带路径的判据",
                          self_mismatch, self_flat);
            }
            else
            {
                GLogInfo(u8"[CSM-CACHE-DIFF] 自查汇总：不一致=%u（平坦区 0）⇒ 整级重建可复现，A vs B 的差异只可能来自条带路径",
                         self_mismatch);
            }
            cache_diff.state = 0;   // 继续抓下一轮（可能是别的级联）
        }
    }

    /// 统计"按偏移映射后的 A"与"B"的不一致数（B 侧 3x3 邻域判剪影边）。自查用。
    void CountDepthDiff(const std::vector<float> &A, const std::vector<float> &B, uint32_t M,
                        uint32_t ox, uint32_t oy, uint32_t &mismatch, uint32_t &edge,
                        uint32_t &flat, float &max_abs)
    {
        mismatch = edge = flat = 0;
        max_abs = 0.0f;

        if (A.size() != B.size() || B.size() != static_cast<size_t>(M) * M)
            return;

        for (uint32_t y = 0; y < M; ++y)
        {
            for (uint32_t x = 0; x < M; ++x)
            {
                const float a = A[static_cast<size_t>((y + oy) % M) * M + ((x + ox) % M)];
                const float b = B[static_cast<size_t>(y) * M + x];
                const float d = (std::fabs)(a - b);
                if (d <= 1.0e-6f)
                    continue;

                ++mismatch;
                if (d > max_abs)
                    max_abs = d;

                float bmin = b, bmax = b;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    const uint32_t yy = static_cast<uint32_t>((static_cast<int>(y) + dy + M) % M);
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const uint32_t xx = static_cast<uint32_t>((static_cast<int>(x) + dx + M) % M);
                        const float v = B[static_cast<size_t>(yy) * M + xx];
                        if (v < bmin)
                            bmin = v;
                        if (v > bmax)
                            bmax = v;
                    }
                }

                if (bmax - bmin > kCacheDiffEdgeEps)
                    ++edge;
                else
                    ++flat;
            }
        }
    }

    /// 诊断：float 深度写 24bit 灰度 BMP（反 Z ⇒ 近处亮）。仅对拍失败时用。
    bool SaveDepthBmp(const char *path, const std::vector<float> &d, uint32_t M)
    {
        if (d.size() != static_cast<size_t>(M) * M)
            return false;

        const uint32_t row_bytes = ((M * 3 + 3) / 4) * 4;   // BMP 每行 4 字节对齐
        const uint32_t pix_bytes = row_bytes * M;
        const uint32_t file_bytes = 54 + pix_bytes;

        std::vector<uint8_t> buf(file_bytes, 0);
        buf[0] = 'B';
        buf[1] = 'M';

        auto put32 = [&buf](uint32_t off, uint32_t v) {
            buf[off] = static_cast<uint8_t>(v & 0xFF);
            buf[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
            buf[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
            buf[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
        };
        put32(2, file_bytes);
        put32(10, 54);   // 像素数据偏移
        put32(14, 40);   // BITMAPINFOHEADER
        put32(18, M);
        put32(22, M);
        buf[26] = 1;     // planes
        buf[28] = 24;    // bpp

        for (uint32_t y = 0; y < M; ++y)
        {
            uint8_t *row = buf.data() + 54 + static_cast<size_t>(M - 1 - y) * row_bytes;   // BMP 自底向上
            for (uint32_t x = 0; x < M; ++x)
            {
                float v = d[static_cast<size_t>(y) * M + x];
                if (!(v >= 0.0f))   // 兼容 NaN
                    v = 0.0f;
                if (v > 1.0f)
                    v = 1.0f;
                const uint8_t g = static_cast<uint8_t>(v * 255.0f + 0.5f);
                row[x * 3 + 0] = g;
                row[x * 3 + 1] = g;
                row[x * 3 + 2] = g;
            }
        }

        FILE *fp = fopen(path, "wb");
        if (!fp)
            return false;
        const size_t wrote = fwrite(buf.data(), 1, buf.size(), fp);
        fclose(fp);
        return wrote == buf.size();
    }

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

        // A1-4：alpha test 物体专用 baseColor（1 层 RGBA8 UNORM 棋盘，黑格
        // alpha=0）。ShadowCasterMasked 的 EvalAlpha 采样该纹理丢黑格。
        alpha_base_texture = texture_manager->CreateTexture2DArray(
            "csm_alpha_baseColor_array", 256, 256, 1,
            VK_FORMAT_R8G8B8A8_UNORM, 1);
        if (!alpha_base_texture)
            return false;

        if (!texture_manager->LoadTexture2DArray(
                alpha_base_texture, 0,
                filesystem::JoinPathWithFilename(
                    OS_TEXT("res/image/pbr/AlphaChecker"), OS_TEXT("baseColor.Tex2D"))))
        {
            GLogError("InitTextures: Failed to load alpha checker texture");
            return false;
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
        if (!lit_recipe.material_ssbo_binding.IsValid())
            return false;

        // A1-4：alpha test 材质——resolve 侧据此路由 ShadowCasterMasked（阴影）
        // 并在片元里 EvalAlpha 丢弃低于阈值的纹素。与 lit 共享材质 SSBO 行。
        alpha_recipe = lit_recipe;
        alpha_recipe.recipe_name = "CascadeShadowMap.AlphaTest";
        alpha_recipe.render_state_overrides.has_alpha_test = true;
        alpha_recipe.render_state_overrides.alpha_test = true;
        alpha_recipe.render_state_overrides.has_alpha_cutoff = true;
        alpha_recipe.render_state_overrides.alpha_cutoff = 0.5f;
        return true;
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
        cfg.blend_width = 0.05f;   // 动态层(CSM 0)边界淡出带 + 末级 300m 边缘淡出带（占区间比例）
        cfg.blend_distance = 1.5f; // 相邻级联交界带（世界米）: 只做取暗叠加, 近级不淡出

        GLogInfo(u8"[CSM] shadow bias_world=%.2fm normal_offset=%.2fm (back-face shadow map; press [ / ] and - / = to tune)",
                 cfg.bias_world, cfg.normal_offset_world);

        // S5 诊断开关（语义见 CacheDiff 注释）：CSM_CACHE_DIFF=1 + CSM_AUTOWALK=<m/s>
        if (const char *env = std::getenv("CSM_CACHE_DIFF"))
            cache_diff.enabled = (env[0] == '1' || env[0] == 't' || env[0] == 'T' ||
                                  env[0] == 'y' || env[0] == 'Y');
        if (const char *env = std::getenv("CSM_AUTOWALK"))
            cache_diff.autowalk = static_cast<float>(std::atof(env));
        // 默认冻结（内容不变是比对前提）；CSM_CACHE_DIFF_FREEZE=0 可关（用来自证"帧间内容在变"）
        if (const char *env = std::getenv("CSM_CACHE_DIFF_FREEZE"))
            cache_diff.freeze = !(env[0] == '0' || env[0] == 'n' || env[0] == 'N' || env[0] == 'f' || env[0] == 'F');
        if (cache_diff.enabled)
            GLogInfo(u8"[CSM-CACHE-DIFF] 对拍诊断已启用（autowalk=%.1f m/s）：等条带滚动帧取 A → 强制整级重建取 B → 逐纹素比对",
                     cache_diff.autowalk);

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

        // ── A1-4：alpha test 物体（Static，近景，验证 ShadowCasterMasked 的
        // 镂空阴影与纹理行物化接线）。棋盘黑格 alpha=0 → 阴影应同样镂空。──
        {
            alpha_geometry = builtin_geometries[9]; // 复用 Cube 几何
            alpha_primitive = PrimitiveAsset(alpha_geometry, &alpha_recipe, PrimitiveType::Triangles);
            if (!alpha_primitive.IsValid())
                return false;

            const glm::vec3 alpha_positions[4] = {
                glm::vec3(-4.0f, -13.0f, 1.52f),
                glm::vec3( 6.0f,  -6.0f, 8.0f),  // 悬空：影子投在视野中央地面，俯视可见镂空
                glm::vec3( 2.0f,   6.0f, 1.22f),
                glm::vec3(-6.0f,  14.0f, 1.22f),
            };

            for (uint32_t i = 0; i < 4; ++i)
            {
                Entity *e = ecs_context->CreateEntity<Entity>(
                    (AnsiString("AlphaCube_") + AnsiString::numberOf(i)).c_str());

                auto tf = e->AddComponent<TransformComponent>(Mobility::Static);
                tf->SetLocalPosition(alpha_positions[i]);
                const float s = (i == 0) ? 3.0f : 2.4f;
                tf->SetLocalScale(glm::vec3(s));
                tf->SetLocalRotation(glm::quat(glm::vec3(0.0f, 0.4f + 0.2f * i, 0.0f)));

                auto prim = e->AddComponent<PrimitiveComponent>();
                prim->SetPrimitiveAsset(&alpha_primitive);
                // base_color 驱动本体棋盘外观；opacity_mask 驱动 ShadowCasterMasked
                // 的 EvalAlpha（采样 .r，0 = 镂空）——影子应呈同图案棋盘孔。
                prim->SetMaterialTextureResource("base_color", alpha_base_texture, pbr_sampler,
                    PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray, "", 0);
                prim->SetMaterialTextureResource("opacity_mask", alpha_base_texture, pbr_sampler,
                    PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray, "", 0);
                prim->SetMaterialDataResource(material_accessors[0].GetGlobalSSBOBinding());
                prim->SetVisible(true);
            }
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
            // Static transform 重复 set 同值也会被判为变更（A3 revision 链会
            // 据此重建静态级联）——吸附格未跨时不得重设。
            //
            // 有意保持 Static（2026-09-26 与 D4 一并裁决）：这张超大平面虽然确实
            // 在运行期被重定位，但吸附带同值守卫、只在跨格时写一次，改成 Movable
            // 反而让它每帧进 movable ring 段；此处接受「每跨格 1 次整级静态级联
            // 全量重建」，也接受 D4 那条一次性"运行期写入 Static transform"告警
            //（每组件只报一次）。
            const glm::vec3 snapped(gx, gy, 0.0f);
            if (snapped != ground_transform->GetLocalPosition())
                ground_transform->SetLocalPosition(snapped);
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
            const float d0 = environment_system ? environment_system->GetCascadeDepthRange(0) : 0.0f;
            const float d1 = environment_system ? environment_system->GetCascadeDepthRange(1) : 0.0f;
            const float d2 = environment_system ? environment_system->GetCascadeDepthRange(2) : 0.0f;
            const float d3 = environment_system ? environment_system->GetCascadeDepthRange(3) : 0.0f;

            GLogInfo(u8"[Shadow Bias] bias_world=%.2fm normalized=[%.5f %.5f %.5f %.5f] (per-cascade depth range=[%.0f %.0f %.0f %.0f]m)",
                     w,
                     ResolvedBiasOf(w, d0), ResolvedBiasOf(w, d1),
                     ResolvedBiasOf(w, d2), ResolvedBiasOf(w, d3),
                     d0, d1, d2, d3);
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

    /// 级联调试开关：F1..F4 切换屏蔽对应级联（边沿触发）
    void TuneCascadeMask()
    {
        if (!ecs_context || !environment_system)
            return;

        auto input_system = ecs_context->GetSystem<InputSystem>();
        if (!input_system)
            return;

        // R：手动失效静态级联滚动缓存（A3 转发 API）。可用于：静态物体
        // 增删/换材质后强制重画；以及诊断"影子固化"类问题。
        {
            const bool r_down = input_system->IsKeyDown(io::KeyboardButton::R);
            if (r_down && !r_key_prev)
            {
                environment_system->InvalidateMainLightStaticShadowCache();
                GLogInfo(u8"[Cascade Switch] static cascade cache invalidated (R)");
            }
            r_key_prev = r_down;
        }

        const bool keys[4] = {
            input_system->IsKeyDown(io::KeyboardButton::F1),
            input_system->IsKeyDown(io::KeyboardButton::F2),
            input_system->IsKeyDown(io::KeyboardButton::F3),
            input_system->IsKeyDown(io::KeyboardButton::F4)
        };

        for (uint32_t c = 0; c < 4; ++c)
        {
            if (keys[c] && !f_key_prev[c])
            {
                bool cur_enabled = environment_system->IsCascadeEnabled(c);
                environment_system->SetCascadeEnabled(c, !cur_enabled);

                GLogInfo(u8"[Cascade Switch] Cascade %u -> %s | State: [C0:%s C1:%s C2:%s C3:%s]",
                         c, !cur_enabled ? u8"ENABLED" : u8"MASKED/OFF",
                         environment_system->IsCascadeEnabled(0) ? "ON" : "OFF",
                         environment_system->IsCascadeEnabled(1) ? "ON" : "OFF",
                         environment_system->IsCascadeEnabled(2) ? "ON" : "OFF",
                         environment_system->IsCascadeEnabled(3) ? "ON" : "OFF");
            }
            f_key_prev[c] = keys[c];
        }
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

        // 对拍期间冻结可移动物体动画：必须**从第 0 帧**就冻结（否则缓存里已存在
        // 动画中的旧内容，A 永远无法与 B 一致）；CSM_CACHE_DIFF_FREEZE=0 可关。
        if (!(cache_diff.enabled && cache_diff.freeze))
            UpdateMovableAnimation(static_cast<float>(elapsed_time));

        TuneShadowBias();
        TuneShadowNormalOffset();
        TuneCascadeMask();

        // 采样各级联"最近一帧"的更新形态（条带数/条带面积/环形偏移/是否整级重建）
        if (environment_system)
        {
            if (auto *ctrl = environment_system->GetShadowController())
            {
                // 单调计数器差分：Update 调用次数（> 帧数 ⇒ 同帧被多次调用，stats 会被覆盖）、
                // 失效调用次数、各级真实的整级重建/条带/命中累计次数
                const uint32_t calls = ctrl->GetUpdateCallCount();
                const uint32_t invs  = ctrl->GetInvalidateCallCount();
                win_calls += calls - cache_prev_calls;
                win_invs  += invs - cache_prev_invs;
                cache_prev_calls = calls;
                cache_prev_invs  = invs;

                for (uint32_t c = 1; c < kMaxShadowCascades; ++c)
                {
                    const auto &s = ctrl->GetUpdateStats(c);
                    CacheWindow &w = cache_win[c];

                    ++w.frames;
                    w.strips += s.strip_count;
                    w.strip_texels += s.strip_texels;

                    const uint32_t f  = ctrl->GetFullUpdateCallCount(c);
                    const uint32_t st = ctrl->GetStripUpdateCallCount(c);
                    const uint32_t h  = ctrl->GetHitUpdateCallCount(c);
                    w.call_full  += f  - cache_prev_full[c];
                    w.call_strip += st - cache_prev_strip[c];
                    w.call_hit   += h  - cache_prev_hit[c];
                    cache_prev_full[c]  = f;
                    cache_prev_strip[c] = st;
                    cache_prev_hit[c]   = h;

                    w.map_texels   = s.map_texels;
                    w.last_off_x   = s.offset.x;
                    w.last_off_y   = s.offset.y;
                    w.band_texels  = csm_config.cache_scroll_band_texels[c];
                }
            }
        }

        // ── 诊断/冒烟辅助：相机自动前进 + "整级 vs 条带"对拍 ──
        // 对拍进行中（state != 0）冻结相机：A/B 两帧的静态内容与布局矩阵必须一致。
        if (cache_diff.autowalk > 0.0f && main_camera && cache_diff.state == 0 && !cache_diff.pending)
            main_camera->position.x += cache_diff.autowalk * static_cast<float>(delta);

        RunCacheDiff();

        if (stats_timer >= 1.0)
        {
            const uint32_t win_strips = cache_win[1].strips + cache_win[2].strips + cache_win[3].strips;
            const uint32_t win_fulls  = cache_win[1].call_full + cache_win[2].call_full + cache_win[3].call_full;

            // 只有"条带与整级重建都为 0"才是纯命中窗口：整级重建同样是有绘制的帧，
            // 旧版仅按 strips==0 判定 ⇒ 会把整级重建的窗口误报成 100% Cached。
            // （u8"..." 是 const char8_t*，变量类型必须跟字面量一致）
            const char8_t *status = (win_strips == 0 && win_fulls == 0)
                                        ? u8"100% Cached (ZERO DrawCalls!)"
                                        : (win_fulls == 0 ? u8"Rolling strips only"
                                                          : u8"Full redraw + rolling strips");

            // 窗口平均：每帧重画纹素占贴图的比例（滚动方案真正的收益指标）
            auto avg_area_pct = [](const CacheWindow &w) -> float {
                const uint64_t denom = static_cast<uint64_t>(w.frames) * w.map_texels;
                return denom > 0 ? 100.0f * static_cast<float>(w.strip_texels) / static_cast<float>(denom) : 0.0f;
            };

            GLogInfo(u8"[CSM Rolling Cache Stats] Cam=(%.1f, %.1f, %.1f) | "
                     u8"C1: %u strips(band=%ut avg=%.2f%% off=(%u,%u) full=%u) | "
                     u8"C2: %u strips(band=%ut avg=%.2f%% off=(%u,%u) full=%u) | "
                     u8"C3: %u strips(band=%ut avg=%.2f%% off=(%u,%u) full=%u) | "
                     u8"Mid/Far Status: %s",
                     main_camera->position.x, main_camera->position.y, main_camera->position.z,
                     cache_win[1].strips, cache_win[1].band_texels, avg_area_pct(cache_win[1]),
                     cache_win[1].last_off_x, cache_win[1].last_off_y, cache_win[1].call_full,
                     cache_win[2].strips, cache_win[2].band_texels, avg_area_pct(cache_win[2]),
                     cache_win[2].last_off_x, cache_win[2].last_off_y, cache_win[2].call_full,
                     cache_win[3].strips, cache_win[3].band_texels, avg_area_pct(cache_win[3]),
                     cache_win[3].last_off_x, cache_win[3].last_off_y, cache_win[3].call_full,
                     status);

            // 计数器差分（关键诊断）：`Upd > frames` ⇒ 同帧被多次 Update（stats 会被覆盖）；
            // `Inv > 0` 而 `fullC == 0` ⇒ 失效调用没作用到这一份控制器状态（另有实例/未生效）。
            GLogInfo(u8"[CSM Cache Counters] Upd=%u Inv=%u frames=%u | "
                     u8"C1: fullC=%u stripC=%u hitC=%u | "
                     u8"C2: fullC=%u stripC=%u hitC=%u | "
                     u8"C3: fullC=%u stripC=%u hitC=%u",
                     win_calls, win_invs, cache_win[1].frames,
                     cache_win[1].call_full, cache_win[1].call_strip, cache_win[1].call_hit,
                     cache_win[2].call_full, cache_win[2].call_strip, cache_win[2].call_hit,
                     cache_win[3].call_full, cache_win[3].call_strip, cache_win[3].call_hit);

            for (uint32_t c = 0; c < kMaxShadowCascades; ++c)
                cache_win[c] = CacheWindow{};
            win_calls = 0;
            win_invs  = 0;
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
        OS_TEXT("Cascaded Shadow Map (CSM Rolling Cache & Mobility Stream)"),
        argc, argv, 1600, 900);
}
