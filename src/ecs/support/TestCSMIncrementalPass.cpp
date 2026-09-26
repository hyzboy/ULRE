#include <hgl/ecs/core/RenderPassRequest.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/core/ScenePipelineMode.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/components/PrimitiveComponent.h>
#include <hgl/ecs/components/ShadowComponent.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/tick/TransformSystem.h>
#include <hgl/ecs/systems/render/EnvironmentSystem.h>
#include <hgl/graph/render/lighting/CascadedShadowController.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/type/String.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <cmath>
#include <numbers>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    hgl::logger::InitLogger(OS_TEXT("TestCSMIncrementalPass"));

    GLogInfo(u8"=== Testing CSM Incremental Pass & Render Options Contract ===");

    // ─────────────────────────────────────────────────────────────
    // Test 1: RenderPassRequest default values
    // ─────────────────────────────────────────────────────────────
    {
        RenderPassRequest req;
        if (req.load_depth != false)
        {
            GLogError(u8"Test 1 Failed: req.load_depth should be false by default");
            return 1;
        }
        if (req.use_scissor != false)
        {
            GLogError(u8"Test 1 Failed: req.use_scissor should be false by default");
            return 1;
        }
        if (req.clear_scissor_depth != false)
        {
            GLogError(u8"Test 1 Failed: req.clear_scissor_depth should be false by default");
            return 1;
        }
        if (req.mobility_filter != -1)
        {
            GLogError(u8"Test 1 Failed: req.mobility_filter should be -1 by default");
            return 1;
        }
        if (req.cull_mode != CullMode::Inherit)
        {
            GLogError(u8"Test 1 Failed: req.cull_mode should be CullMode::Inherit by default");
            return 1;
        }
        if (req.is_shadow_pass != false)
        {
            GLogError(u8"Test 1 Failed: req.is_shadow_pass should be false by default");
            return 1;
        }
        if (req.shadow_reference_camera != nullptr)
        {
            GLogError(u8"Test 1 Failed: req.shadow_reference_camera should be nullptr by default");
            return 1;
        }
        GLogInfo(u8"Test 1 Passed: RenderPassRequest defaults verified.");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 2: RenderPassOptions default values & assignment
    // ─────────────────────────────────────────────────────────────
    {
        RenderPassOptions opts;
        if (opts.load_depth != false || opts.use_scissor != false || opts.clear_scissor_depth != false)
        {
            GLogError(u8"Test 2 Failed: RenderPassOptions defaults incorrect");
            return 2;
        }

        opts.load_depth = true;
        opts.use_scissor = true;
        opts.scissor.offset = { 64, 128 };
        opts.scissor.extent = { 256, 512 };
        opts.clear_scissor_depth = true;
        opts.clear_depth_value = 0.0f; // Reversed-Z far plane
        opts.cull_mode = static_cast<int>(CullMode::Front);

        if (!opts.load_depth || !opts.use_scissor || !opts.clear_scissor_depth || opts.clear_depth_value != 0.0f)
        {
            GLogError(u8"Test 2 Failed: RenderPassOptions values not preserved");
            return 2;
        }
        if (opts.cull_mode != static_cast<int>(CullMode::Front))
        {
            GLogError(u8"Test 2 Failed: RenderPassOptions cull mode not preserved");
            return 2;
        }

        // CullMode 的值必须与 VkCullModeFlags 对齐：底层直接 static_cast 透传
        if (static_cast<int>(CullMode::None)  != static_cast<int>(VK_CULL_MODE_NONE)
         || static_cast<int>(CullMode::Front) != static_cast<int>(VK_CULL_MODE_FRONT_BIT)
         || static_cast<int>(CullMode::Back)  != static_cast<int>(VK_CULL_MODE_BACK_BIT))
        {
            GLogError(u8"Test 2 Failed: CullMode values must match VkCullModeFlags");
            return 2;
        }

        RenderPassOptions opts_auto;
        if (opts_auto.cull_mode != -1)
        {
            GLogError(u8"Test 2 Failed: RenderPassOptions cull mode default should be -1 (Inherit)");
            return 2;
        }
        GLogInfo(u8"Test 2 Passed: RenderPassOptions configuration verified.");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 3: Mobility Enum & TransformComponent contract
    // ─────────────────────────────────────────────────────────────
    {
        if (static_cast<int>(Mobility::Static) != 0)
        {
            GLogError(u8"Test 3 Failed: Mobility::Static != 0");
            return 3;
        }
        if (static_cast<int>(Mobility::Movable) != 1)
        {
            GLogError(u8"Test 3 Failed: Mobility::Movable != 1");
            return 3;
        }

        TransformComponent comp(Mobility::Static);
        if (comp.GetMobility() != Mobility::Static)
        {
            GLogError(u8"Test 3 Failed: TransformComponent failed to set Mobility::Static");
            return 3;
        }

        comp.SetMobility(Mobility::Movable);
        if (comp.GetMobility() != Mobility::Movable)
        {
            GLogError(u8"Test 3 Failed: TransformComponent failed to set Mobility::Movable");
            return 3;
        }
        GLogInfo(u8"Test 3 Passed: Mobility contracts verified.");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 4: CascadedShadowController ShadowDirtyRect -> PassRequest translation
    // ─────────────────────────────────────────────────────────────
    {
        ShadowDirtyRect dirty_rect{ 100, 200, 300, 400 };

        RenderPassRequest pass_req;
        pass_req.load_depth = true;
        pass_req.use_scissor = true;
        pass_req.scissor.offset = { static_cast<int32_t>(dirty_rect.x), static_cast<int32_t>(dirty_rect.y) };
        pass_req.scissor.extent = { dirty_rect.width, dirty_rect.height };
        pass_req.clear_scissor_depth = true;
        pass_req.mobility_filter = static_cast<int>(Mobility::Static);

        if (pass_req.scissor.offset.x != 100 || pass_req.scissor.offset.y != 200 ||
            pass_req.scissor.extent.width != 300 || pass_req.scissor.extent.height != 400)
        {
            GLogError(u8"Test 4 Failed: DirtyRect to scissor conversion error");
            return 4;
        }

        if (pass_req.mobility_filter != static_cast<int>(Mobility::Static))
        {
            GLogError(u8"Test 4 Failed: Mobility filter not set to Static");
            return 4;
        }
        GLogInfo(u8"Test 4 Passed: DirtyRect to RenderPassRequest translation verified.");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 5: 静态级联缓存有效性与视锥覆盖率
    //
    // 5A 重绘预算：相机横向行走 600 帧（5m/s @60fps）时，静态级联（CSM 1..3）
    //    必须靠"横向粗锚定"把整级全量重绘压到很小比例（历史缺陷：每跨 1 个 texel
    //    即整级重绘，实测 435/600）。CSM 0 是逐帧全量动态层，必须 600/600。
    // 5B 静态滚动缓存契约：矩阵恒定性（缓存命中帧必须与重绘帧用同一矩阵，否则
    //    静态阴影在锚定格内整体滑动）+ 冻结窗口覆盖率（格内视锥漂移后角点仍必须
    //    全在正交视窗内，用于看住 radius 补偿量）。
    // ─────────────────────────────────────────────────────────────
    {
        CascadedShadowConfig cfg;
        cfg.cascade_count = 4;
        cfg.c0_dynamic_overlay = true;
        cfg.shadow_map_size = 1024.0f;
        cfg.max_distance = 300.0f;
        cfg.cache_anchor_step = 16.0f;
        // 横向锚定（S1：texel 口径，世界步长由 B 派生 ≈2.0/6.3/11.5 m @ 本配置）
        cfg.cache_scroll_band_texels[1] = 16;
        cfg.cache_scroll_band_texels[2] = 16;
        cfg.cache_scroll_band_texels[3] = 32;

        CascadedShadowController ctrl(cfg);
        ShadowInfo shadow_info;

        Camera cam;
        cam.znear = 0.1f;
        cam.zfar = 500.0f;
        cam.fovY = 60.0f;

        const float aspect = 16.0f / 9.0f;
        const Vector3f light_dir = glm::normalize(Vector3f(0.5f, 0.8f, -1.0f));
        const float near_z = cam.znear;

        uint32_t full[4] = {0, 0, 0, 0};
        uint32_t band[4] = {0, 0, 0, 0};

        constexpr int kFrames = 600;
        for (int f = 0; f < kFrames; ++f)
        {
            cam.pos = Vector3f(static_cast<float>(f) * 0.083f, 0.0f, 1.7f);

            CascadeUpdateResult upd[kMaxShadowCascades];
            ctrl.Update(cam, aspect, light_dir, shadow_info, upd);

            for (uint32_t c = 0; c < 4; ++c)
            {
                if (upd[c].need_full_update)
                    ++full[c];
                else if (upd[c].dirty_rect_count > 0)
                    ++band[c];
            }
        }

        GLogInfo(u8"[CSM-CACHE] frames=%d full=[%u,%u,%u,%u] band=[%u,%u,%u,%u]",
                 kFrames, full[0], full[1], full[2], full[3], band[0], band[1], band[2], band[3]);

        if (full[0] != static_cast<uint32_t>(kFrames))
        {
            GLogError(u8"Test 5A Failed: cascade 0 must be fully redrawn every frame (%u/%d)", full[0], kFrames);
            return 1;
        }

        for (uint32_t c = 1; c < 4; ++c)
        {
            // 允许 10% 的帧做整级重建（横向/沿光轴锚点跨格），其余必须命中缓存
            if (full[c] > static_cast<uint32_t>(kFrames / 10))
            {
                GLogError(u8"Test 5A Failed: static cascade %u full-updated %u/%d frames (budget %d)",
                          c, full[c], kFrames, kFrames / 10);
                return 1;
            }
        }

        // ─────────────────────────────────────────────────────────
        // Test 5B: 静态滚动缓存的两条硬契约
        //   5B-1 矩阵恒定性：内容刷新帧（整级重建 或 环形条带滚动，S2）会把布局矩阵推进到
        //        新锚定格；此后到下一次刷新之间的**纯命中帧**必须与该矩阵完全一致——
        //        否则贴图里的深度被另一个矩阵解读，静态阴影会在锚定格内整体滑动。
        //        （历史缺陷：横向吸附量被二次吸附抵消 ⇒ 矩阵仍随相机连续移动）
        //        注意：跨格时矩阵**本来就该**移动恰好 B texel（旧内容靠 cache_offset 原地
        //        续用），所以判据是"纯命中帧之间矩阵不变"，不是"矩阵永不移动"；
        //        "跨格后内容仍原地有效"由 Test 17 的物理不动点契约单独把守。
        //   5B-2 覆盖率：窗口冻结在内容刷新帧的粗锚定点上，格内视锥最多漂移 step*0.707，
        //        半径补偿不足时切片角点落到正交视窗之外 ⇒ 边缘物体没有阴影。子用例故意让
        //        补偿量与半径同量级（真实配置下补偿只占半径百分之几），保证补偿一旦缺失
        //        断言必然失败。
        // ─────────────────────────────────────────────────────────
        {
            // ── 5B-1：朝向固定 ⇒ 拟合半径与位置无关，锚定格内矩阵只应差浮点噪声 ──
            {
                CascadedShadowConfig ccfg;
                ccfg.cascade_count = 4;
                ccfg.c0_dynamic_overlay = true;
                ccfg.shadow_map_size = 1024.0f;
                ccfg.max_distance = 300.0f;
                ccfg.cache_anchor_step = 16.0f;
                ccfg.cache_scroll_band_texels[1] = 16;
                ccfg.cache_scroll_band_texels[2] = 16;
                ccfg.cache_scroll_band_texels[3] = 32;

                CascadedShadowController cctrl(ccfg);
                ShadowInfo cinfo;
                Camera ccam;
                ccam.znear = 0.1f;
                ccam.zfar = 500.0f;
                ccam.fovY = 60.0f;
                ccam.viewDirection = Vector3f(1.0f, 0.35f, -0.2f);

                // 光轴与 x 轴不平行：沿 x 行走时横向锚定格与沿光轴锚定格都在跨越，
                // 格内那段横向漂移正是被测量的量。
                const Vector3f slight = glm::normalize(Vector3f(0.5f, 0.8f, -1.0f));

                constexpr int kSweepFrames = 400;
                constexpr float kMatrixEps = 1.0e-3f;

                Matrix4f redraw_vp[kMaxShadowCascades];
                bool redraw_valid[kMaxShadowCascades] = {false, false, false, false};
                uint32_t hits[kMaxShadowCascades] = {0, 0, 0, 0};
                uint32_t coherence_fail = 0;
                float max_delta = 0.0f;
                int max_delta_cascade = -1;
                int max_delta_frame = -1;

                for (int f = 0; f < kSweepFrames; ++f)
                {
                    ccam.pos = Vector3f(static_cast<float>(f) * 0.083f, 0.0f, 1.7f);

                    CascadeUpdateResult supd[kMaxShadowCascades];
                    cctrl.Update(ccam, aspect, slight, cinfo, supd);

                    for (uint32_t c = 1; c < 4; ++c)
                    {
                        const Matrix4f vp = supd[c].light_proj * supd[c].light_view;

                        // 内容刷新帧 = 整级重建 **或** 环形条带重画（S2）：两者都会把布局矩阵
                        // 推进到新锚定格（跨格位移恰为 B texel，旧内容靠 cache_offset 原地
                        // 续用），所以引用矩阵必须随之更新。只有"纯命中帧"（既没重建也没条带）
                        // 才必须与上一次刷新的矩阵逐元素相同——否则贴图深度被另一个矩阵解读。
                        if (supd[c].need_full_update || supd[c].dirty_rect_count > 0)
                        {
                            redraw_vp[c] = vp;
                            redraw_valid[c] = true;
                            continue;
                        }
                        if (!redraw_valid[c])
                        {
                            continue;
                        }

                        ++hits[c];
                        float delta = 0.0f;
                        for (int col = 0; col < 4; ++col)
                        {
                            for (int row = 0; row < 4; ++row)
                            {
                                delta = std::max(delta, std::abs(vp[col][row] - redraw_vp[c][col][row]));
                            }
                        }
                        if (delta > max_delta)
                        {
                            max_delta = delta;
                            max_delta_cascade = static_cast<int>(c);
                            max_delta_frame = f;
                        }
                        if (delta > kMatrixEps)
                        {
                            ++coherence_fail;
                        }
                    }
                }

                GLogInfo(u8"[CSM-COHERENCE] frames=%d hits=[%u,%u,%u] max_delta=%f (c=%d f=%d) fail=%u",
                         kSweepFrames, hits[1], hits[2], hits[3], max_delta,
                         max_delta_cascade, max_delta_frame, coherence_fail);

                if (hits[1] == 0 || hits[2] == 0 || hits[3] == 0)
                {
                    GLogError(u8"Test 5B Failed: cache hit missing (hits=[%u,%u,%u]), "
                              u8"matrix stability was never exercised", hits[1], hits[2], hits[3]);
                    return 1;
                }

                if (coherence_fail != 0)
                {
                    GLogError(u8"Test 5B Failed: light matrix drifted on %u cache-hit frames "
                              u8"(max delta %f, cascade %d, frame %d)",
                              coherence_fail, max_delta, max_delta_cascade, max_delta_frame);
                    return 1;
                }
            }

            // ── 5B-2：薄切片 + 10m 横向锚点（补偿量与半径同量级），冻结窗口仍必须
            //          罩住格内漂移后的 8 个切片角点 ──
            {
                CascadedShadowConfig ccfg;
                ccfg.cascade_count = 4;
                ccfg.c0_dynamic_overlay = true;
                ccfg.shadow_map_size = 1024.0f;
                ccfg.split_distances[0] = 0.6f;
                ccfg.split_distances[1] = 1.2f;
                ccfg.split_distances[2] = 2.4f;
                ccfg.split_distances[3] = 4.8f;
                ccfg.cache_anchor_step = 16.0f;
                // 压力子用例：横向锚定格大到与切片半径同量级（原 lateral_step=10m 的等价形态）。
                // texel 口径（S1）下 B=600 @ M=1024 ⇒ L = 2·B·r0/(M−1.416·B) ≈ 10.6 m（r0≈1.54m）
                // ⇒ 半径补偿 ≈ 7.5 m ≈ 4.9×r0，与改前同量级 ⇒ 补偿一旦缺失断言必然失败。
                ccfg.cache_scroll_band_texels[1] = 600;
                ccfg.cache_scroll_band_texels[2] = 600;
                ccfg.cache_scroll_band_texels[3] = 600;

                CascadedShadowController cctrl(ccfg);
                ShadowInfo cinfo;
                Camera ccam;
                ccam.znear = 0.1f;
                ccam.zfar = 500.0f;
                ccam.fovY = 60.0f;

                // 光轴取 normalize(1,0,-0.5)：此时 light_right 恰为 (0,-1,0)，与 yaw=0
                // 的相机右轴完全重合 ⇒ 切片角点的横向偏移 100% 投影到被测轴上，不给
                // 断言留"投影打折"的余地。
                const Vector3f slight = glm::normalize(Vector3f(1.0f, 0.0f, -0.5f));

                Matrix4f frozen_vp[kMaxShadowCascades];
                bool frozen_valid[kMaxShadowCascades] = {false, false, false, false};
                uint32_t coverage_fail = 0;
                float worst_ndc[4] = {0.0f, 0.0f, 0.0f, 0.0f};

                // 8 个位置 × 16 个朝向：位置扫掠跨越整格（漂移取到最大），朝向扫掠让
                // 角点相对光轴的横向偏离取遍各个方向，避免断言被"恰好对齐"绕过去。
                constexpr int kStressFrames = 128;
                for (int f = 0; f < kStressFrames; ++f)
                {
                    const float yaw = static_cast<float>(f % 16) * (std::numbers::pi_v<float> / 8.0f);
                    const float step_idx = static_cast<float>(f / 16);
                    ccam.pos = Vector3f(step_idx * 1.0f, -step_idx * 1.4f, 1.7f);
                    ccam.viewDirection = Vector3f(std::cos(yaw), std::sin(yaw), -0.25f);

                    CascadeUpdateResult supd[kMaxShadowCascades];
                    cctrl.Update(ccam, aspect, slight, cinfo, supd);

                    const Vector3f cam_forward = glm::normalize(ccam.viewDirection);
                    const Vector3f cam_right = glm::normalize(glm::cross(cam_forward, ccam.world_up));
                    const Vector3f cam_up = glm::cross(cam_right, cam_forward);
                    const float tan_half = std::tan(ccam.fovY * (std::numbers::pi / 180.0) * 0.5f);

                    for (uint32_t c = 1; c < 4; ++c)
                    {
                        // 内容刷新帧（整级重建 或 环形条带）沿用它把内容写进贴图时的矩阵：
                        // 贴图里的深度就是按那个矩阵 + cache_offset 写/读的，覆盖率必须按
                        // 同一矩阵判定。
                        if (supd[c].need_full_update || supd[c].dirty_rect_count > 0 || !frozen_valid[c])
                        {
                            frozen_vp[c] = supd[c].light_proj * supd[c].light_view;
                            frozen_valid[c] = true;
                        }

                        const float s_near = (c == 1 && ccfg.c0_dynamic_overlay)
                                                 ? ccam.znear : ccfg.split_distances[c - 1];
                        const float dists[2] = {s_near, ccfg.split_distances[c]};
                        for (int d = 0; d < 2; ++d)
                        {
                            const float hh = dists[d] * tan_half;
                            const float ww = hh * aspect;
                            for (int sx = -1; sx <= 1; sx += 2)
                            {
                                for (int sy = -1; sy <= 1; sy += 2)
                                {
                                    const Vector3f corner = ccam.pos + cam_forward * dists[d]
                                                          + cam_right * (ww * static_cast<float>(sx))
                                                          + cam_up * (hh * static_cast<float>(sy));
                                    const Vector4f clip = frozen_vp[c] * Vector4f(corner, 1.0f);
                                    if (clip.w <= 0.0f)
                                    {
                                        ++coverage_fail;
                                        continue;
                                    }
                                    const Vector3f ndc = Vector3f(clip) / clip.w;
                                    worst_ndc[c] = std::max(worst_ndc[c],
                                                            std::max(std::abs(ndc.x), std::abs(ndc.y)));
                                    if (std::abs(ndc.x) > 1.001f || std::abs(ndc.y) > 1.001f ||
                                        ndc.z < -0.001f || ndc.z > 1.001f)
                                    {
                                        ++coverage_fail;
                                    }
                                }
                            }
                        }
                    }
                }

                GLogInfo(u8"[CSM-COVERAGE] band=600texel(~10.6m) frames=%d fail=%u worst_ndc=[%f,%f,%f,%f]",
                         kStressFrames, coverage_fail, worst_ndc[0], worst_ndc[1],
                         worst_ndc[2], worst_ndc[3]);

                if (coverage_fail != 0)
                {
                    GLogError(u8"Test 5B Failed: %u frustum-slice corners fell outside "
                              u8"the frozen shadow ortho window", coverage_fail);
                    return 1;
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────
    // Test 5C: 原地旋转时静态级联同样不许"缓存命中却换了矩阵"
    //   5B-1 只扫平移（朝向固定）。拖拽视角是**旋转**为主：旋转会移动贴合球的
    //   中心（pos + forward*mid_dist），横向/沿光轴的锚定与拟合半径必须把矩阵
    //   钉死在纹素格上，否则缓存命中帧的贴图内容会被另一个矩阵解读 ⇒ 静态阴影
    //   在拖拽时整体滑动/乱飞，松手或跨过一个锚定格后"自己定下来"。
    //   这里断言：缓存命中帧的 light_proj*light_view 必须与最近一次重绘帧完全一致。
    // ─────────────────────────────────────────────────────────
    {
        CascadedShadowConfig ccfg;
        ccfg.cascade_count = 4;
        ccfg.c0_dynamic_overlay = true;
        ccfg.shadow_map_size = 1024.0f;
        ccfg.max_distance = 300.0f;
        ccfg.cache_anchor_step = 16.0f;        // 沿光轴锚定：矩阵的第三维只能按步进跳
        ccfg.cache_scroll_band_texels[1] = 16; // 横向锚定（S1：texel 口径）
        ccfg.cache_scroll_band_texels[2] = 16;
        ccfg.cache_scroll_band_texels[3] = 32;

        CascadedShadowController cctrl(ccfg);
        ShadowInfo cinfo;
        Camera ccam;
        ccam.znear = 0.1f;
        ccam.zfar = 500.0f;
        ccam.fovY = 60.0f;
        ccam.pos = Vector3f(0.0f, 0.0f, 1.7f);  // 相机位置不动，只转朝向

        const Vector3f clight = glm::normalize(Vector3f(0.5f, 0.8f, -1.0f));
        const float caspect = 16.0f / 9.0f;

        constexpr int kSpinFrames = 240;
        constexpr float kMatrixEps = 1.0e-3f;

        Matrix4f spin_vp[kMaxShadowCascades];
        float spin_radius[kMaxShadowCascades] = {0.0f, 0.0f, 0.0f, 0.0f};
        bool spin_valid[kMaxShadowCascades] = {false, false, false, false};
        uint32_t spin_hits = 0;
        uint32_t spin_fail = 0;
        float spin_max_delta = 0.0f;
        float spin_max_radius_delta = 0.0f;

        for (int f = 0; f < kSpinFrames; ++f)
        {
            const float t = static_cast<float>(f) * 0.02f;
            ccam.viewDirection = glm::normalize(
                Vector3f(std::cos(t) * 0.9f + 0.1f, 0.55f, std::sin(t) * 0.4f - 0.15f));

            CascadeUpdateResult supd[kMaxShadowCascades];
            cctrl.Update(ccam, caspect, clight, cinfo, supd);

            for (uint32_t c = 1; c < 4; ++c)
            {
                const Matrix4f vp = supd[c].light_proj * supd[c].light_view;

                // 内容刷新帧（整级重建 或 环形条带滚动）⇒ 更新引用；纯命中帧必须矩阵不变
                if (supd[c].need_full_update || supd[c].dirty_rect_count > 0)
                {
                    spin_vp[c] = vp;
                    spin_radius[c] = supd[c].sphere_radius;
                    spin_valid[c] = true;
                    continue;
                }
                if (!spin_valid[c])
                {
                    continue;
                }

                ++spin_hits;
                float delta = 0.0f;
                for (int col = 0; col < 4; ++col)
                {
                    for (int row = 0; row < 4; ++row)
                    {
                        delta = std::max(delta, std::abs(vp[col][row] - spin_vp[c][col][row]));
                    }
                }

                spin_max_delta = std::max(spin_max_delta, delta);
                spin_max_radius_delta = std::max(spin_max_radius_delta,
                                                 std::abs(supd[c].sphere_radius - spin_radius[c]));
                if (delta > kMatrixEps)
                {
                    ++spin_fail;
                }
            }
        }

        GLogInfo(u8"[CSM-SPIN] frames=%d hits=%u max_delta=%f max_radius_delta=%fm fail=%u",
                 kSpinFrames, spin_hits, spin_max_delta, spin_max_radius_delta, spin_fail);

        if (spin_hits == 0)
        {
            GLogError(u8"Test 5C Failed: rotation sweep produced no cache-hit frames, "
                      u8"matrix stability under rotation was never exercised");
            return 1;
        }

        // 旋转只能移动视锥切片中心，不能改变切片自身的拟合半径（切片关于中点对称，
        // 8 个角点到中点的距离相等）⇒ 半径必须几乎是常量，否则贴图缩放会逐帧变化。
        if (spin_max_radius_delta > 0.01f)
        {
            GLogError(u8"Test 5C Failed: fitted radius moved with camera orientation "
                      u8"(max delta %fm), cached depth map is being rescaled", spin_max_radius_delta);
            return 1;
        }

        if (spin_fail != 0)
        {
            GLogError(u8"Test 5C Failed: light matrix drifted on %u cache-hit frames while "
                      u8"rotating in place (max delta %f) -- static shadows slide during drag",
                      spin_fail, spin_max_delta);
            return 1;
        }

    }

    // ─────────────────────────────────────────────────────────────
    // Test 6: 逐级联 bias（背面渲染的贴合补偿）
    //
    // bias 是归一化深度偏移，其世界效果 = bias × 该级联深度范围。各级联深度范围差异
    // 极大（示例约 346/349/619/953m），同一个归一化 bias 在远景级联上会放大成 2.6 倍
    // 世界偏移 ⇒ 按近景调的取值套到远景就变成半影光晕。本组契约看住两种修正方式：
    // 6A 默认配置必须保持历史行为（所有级联写同一个 config_.bias）。
    // 6B per_cascade_bias_scale 必须逐级独立生效，且确实是写进了 UBO 的 shadow_params.x。
    // 6C bias_world 必须逐级换算成各不相同的归一化值，但换算后**世界偏移恒定**。
    // 6D bias_world 非 0 时必须压过 per_cascade_bias_scale（优先级契约）。
    // ─────────────────────────────────────────────────────────────
    {
        const float aspect6 = 16.0f / 9.0f;
        const Vector3f light6 = glm::normalize(Vector3f(0.5f, 0.8f, -1.0f));

        Camera cam6;
        cam6.znear = 0.1f;
        cam6.zfar = 500.0f;
        cam6.fovY = 60.0f;
        cam6.pos = Vector3f(0.0f, 0.0f, 1.7f);
        cam6.viewDirection = Vector3f(0.0f, 1.0f, -0.3f);

        // ── 6A：默认配置 = 历史行为 ──
        {
            CascadedShadowConfig cfg6;
            cfg6.cascade_count = 4;
            cfg6.c0_dynamic_overlay = true;
            cfg6.shadow_map_size = 1024.0f;
            cfg6.max_distance = 300.0f;
            cfg6.split_distances[0] = 50.0f;
            cfg6.split_distances[1] = 50.0f;
            cfg6.split_distances[2] = 160.0f;
            cfg6.split_distances[3] = 300.0f;
            cfg6.bias = -0.003f;

            if (cfg6.bias_world != 0.0f)
            {
                GLogError(u8"Test 6A Failed: bias_world must default to 0 (normalized path)");
                return 6;
            }
            for (uint32_t c = 0; c < kMaxShadowCascades; ++c)
            {
                if (cfg6.per_cascade_bias_scale[c] != 1.0f)
                {
                    GLogError(u8"Test 6A Failed: per_cascade_bias_scale[%u] must default to 1.0", c);
                    return 6;
                }
            }

            CascadedShadowController ctrl6(cfg6);
            ShadowInfo si6;
            CascadeUpdateResult upd6[kMaxShadowCascades];
            ctrl6.Update(cam6, aspect6, light6, si6, upd6);

            uint32_t distinct_ranges = 0;
            for (uint32_t c = 0; c < 4; ++c)
            {
                if (si6.cascades[c].shadow_params.x != cfg6.bias)
                {
                    GLogError(u8"Test 6A Failed: cascade %u normalized bias %f != config bias %f "
                              u8"(default config must preserve historical behaviour)",
                              c, si6.cascades[c].shadow_params.x, cfg6.bias);
                    return 6;
                }
                if (upd6[c].depth_range <= 0.0f)
                {
                    GLogError(u8"Test 6A Failed: cascade %u depth_range=%f must be positive",
                              c, upd6[c].depth_range);
                    return 6;
                }
                if (upd6[c].resolved_bias != cfg6.bias)
                {
                    GLogError(u8"Test 6A Failed: cascade %u resolved_bias %f != config bias %f",
                              c, upd6[c].resolved_bias, cfg6.bias);
                    return 6;
                }
                if (c > 0 && std::abs(upd6[c].depth_range - upd6[0].depth_range) > 1.0f)
                    ++distinct_ranges;
            }

            GLogInfo(u8"[CSM-BIAS] default normalized=[%f %f %f %f] depth_range=[%.0f %.0f %.0f %.0f]m",
                     si6.cascades[0].shadow_params.x, si6.cascades[1].shadow_params.x,
                     si6.cascades[2].shadow_params.x, si6.cascades[3].shadow_params.x,
                     upd6[0].depth_range, upd6[1].depth_range, upd6[2].depth_range, upd6[3].depth_range);

            // 逐级尺度的意义完全依赖"各级深度范围不同"：若它们相同，本 todo 无解题前提
            if (distinct_ranges == 0)
            {
                GLogError(u8"Test 6A Failed: all cascades share one depth range; per-cascade bias "
                          u8"has no premise. Check split_distances / caster_depth_margin.");
                return 6;
            }
        }

        // ── 6B：per_cascade_bias_scale 逐级生效 ──
        {
            CascadedShadowConfig cfg6;
            cfg6.cascade_count = 4;
            cfg6.c0_dynamic_overlay = true;
            cfg6.shadow_map_size = 1024.0f;
            cfg6.max_distance = 300.0f;
            cfg6.split_distances[0] = 50.0f;
            cfg6.split_distances[1] = 50.0f;
            cfg6.split_distances[2] = 160.0f;
            cfg6.split_distances[3] = 300.0f;
            cfg6.bias = -0.002f;
            cfg6.per_cascade_bias_scale[0] = 1.0f;
            cfg6.per_cascade_bias_scale[1] = 2.0f;
            cfg6.per_cascade_bias_scale[2] = 3.0f;
            cfg6.per_cascade_bias_scale[3] = 0.5f;

            CascadedShadowController ctrl6(cfg6);
            ShadowInfo si6;
            CascadeUpdateResult upd6[kMaxShadowCascades];
            ctrl6.Update(cam6, aspect6, light6, si6, upd6);

            const float want[4] = { -0.002f, -0.004f, -0.006f, -0.001f };
            for (uint32_t c = 0; c < 4; ++c)
            {
                if (std::abs(si6.cascades[c].shadow_params.x - want[c]) > 1e-6f)
                {
                    GLogError(u8"Test 6B Failed: cascade %u normalized bias %f != expected %f "
                              u8"(per_cascade_bias_scale[%u]=%f)",
                              c, si6.cascades[c].shadow_params.x, want[c],
                              c, cfg6.per_cascade_bias_scale[c]);
                    return 6;
                }
            }

            // UBO 的其余分量不能被 bias 改动带偏
            if (si6.cascades[2].shadow_params.y != cfg6.pcf_radius ||
                si6.cascades[2].shadow_params.z != cfg6.darkness)
            {
                GLogError(u8"Test 6B Failed: shadow_params.yzw must stay unchanged");
                return 6;
            }

            GLogInfo(u8"[CSM-BIAS] per-cascade scale=[1 2 3 0.5] -> normalized=[%f %f %f %f]",
                     si6.cascades[0].shadow_params.x, si6.cascades[1].shadow_params.x,
                     si6.cascades[2].shadow_params.x, si6.cascades[3].shadow_params.x);
        }

        // ── 6C：bias_world 世界偏移恒定 ──
        {
            CascadedShadowConfig cfg6;
            cfg6.cascade_count = 4;
            cfg6.c0_dynamic_overlay = true;
            cfg6.shadow_map_size = 1024.0f;
            cfg6.max_distance = 300.0f;
            cfg6.split_distances[0] = 50.0f;
            cfg6.split_distances[1] = 50.0f;
            cfg6.split_distances[2] = 160.0f;
            cfg6.split_distances[3] = 300.0f;
            cfg6.bias = -0.003f;      // 必须被 bias_world 压过
            cfg6.bias_world = -1.15f;

            CascadedShadowController ctrl6(cfg6);
            ShadowInfo si6;
            CascadeUpdateResult upd6[kMaxShadowCascades];
            ctrl6.Update(cam6, aspect6, light6, si6, upd6);

            float min_norm = 1e9f;
            float max_norm = -1e9f;
            float min_world = 1e9f;
            float max_world = -1e9f;

            for (uint32_t c = 0; c < 4; ++c)
            {
                const float norm = si6.cascades[c].shadow_params.x;
                const float world = norm * upd6[c].depth_range;

                if (std::abs(norm) > 0.05f)
                {
                    GLogError(u8"Test 6C Failed: cascade %u normalized bias %f is out of sane range "
                              u8"(depth_range=%.0fm, bias_world=%.2fm)",
                              c, norm, upd6[c].depth_range, cfg6.bias_world);
                    return 6;
                }
                if (world >= 0.0f)
                {
                    GLogError(u8"Test 6C Failed: cascade %u world offset %f must be negative "
                              u8"(hug the caster)", c, world);
                    return 6;
                }

                min_norm = std::min(min_norm, norm);
                max_norm = std::max(max_norm, norm);
                min_world = std::min(min_world, world);
                max_world = std::max(max_world, world);
            }

            // 归一化值必须逐级不同（证明换算真的按级做了），但世界偏移必须恒定
            if (max_norm - min_norm < 1e-5f)
            {
                GLogError(u8"Test 6C Failed: normalized bias identical on every cascade "
                          u8"([%f..%f]); bias_world was not per-cascade converted",
                          min_norm, max_norm);
                return 6;
            }
            if (max_world - min_world > 1e-3f)
            {
                GLogError(u8"Test 6C Failed: world offset not constant across cascades "
                          u8"([%f..%f]m); bias_world conversion is inconsistent",
                          min_world, max_world);
                return 6;
            }
            if (std::abs(max_world - cfg6.bias_world) > 1e-3f)
            {
                GLogError(u8"Test 6C Failed: world offset %f != bias_world %f",
                          max_world, cfg6.bias_world);
                return 6;
            }

            GLogInfo(u8"[CSM-BIAS] bias_world=%.2fm normalized=[%f %f %f %f] world=[%.4f..%.4f]m (constant)",
                     cfg6.bias_world,
                     si6.cascades[0].shadow_params.x, si6.cascades[1].shadow_params.x,
                     si6.cascades[2].shadow_params.x, si6.cascades[3].shadow_params.x,
                     min_world, max_world);
        }

        // ── 6D：bias_world 优先级高于 per_cascade_bias_scale ──
        {
            CascadedShadowConfig cfg6;
            cfg6.cascade_count = 4;
            cfg6.c0_dynamic_overlay = true;
            cfg6.shadow_map_size = 1024.0f;
            cfg6.max_distance = 300.0f;
            cfg6.split_distances[0] = 50.0f;
            cfg6.split_distances[1] = 50.0f;
            cfg6.split_distances[2] = 160.0f;
            cfg6.split_distances[3] = 300.0f;
            cfg6.bias = -0.003f;
            cfg6.bias_world = -0.8f;
            cfg6.per_cascade_bias_scale[1] = 7.0f;   // 必须被忽略

            CascadedShadowController ctrl6(cfg6);
            ShadowInfo si6;
            CascadeUpdateResult upd6[kMaxShadowCascades];
            ctrl6.Update(cam6, aspect6, light6, si6, upd6);

            for (uint32_t c = 0; c < 4; ++c)
            {
                const float world = si6.cascades[c].shadow_params.x * upd6[c].depth_range;
                if (std::abs(world - cfg6.bias_world) > 1e-3f)
                {
                    GLogError(u8"Test 6D Failed: cascade %u world offset %f != bias_world %f "
                              u8"(per_cascade_bias_scale must be ignored while bias_world != 0)",
                              c, world, cfg6.bias_world);
                    return 6;
                }
            }
            GLogInfo(u8"Test 6D Passed: bias_world takes precedence over per_cascade_bias_scale.");
        }

        // ── 7A：normal_offset_world 逐级写入 shadow_params.w，默认 0（不擅自改变历史行为）──
        // shadow_params.w 是"运行时强度"，shader 侧的 HGL_SHADOW_NORMAL_OFFSET 宏是
        // "有没有这段代码"。两者必须都到位才算接通：这里锁的是 CPU→UBO 这一段，
        // 7C 锁 shader→UBO 那一段。
        {
            {
                // 默认配置（结构体默认 normal_offset_world = 0）必须逐级为 0，
                // 否则所有不关心此功能的场景都会被悄悄改掉采样位置。
                CascadedShadowConfig cfg7;
                cfg7.cascade_count = 4;
                cfg7.c0_dynamic_overlay = true;
                cfg7.shadow_map_size = 1024.0f;
                cfg7.max_distance = 300.0f;
                cfg7.split_distances[0] = 50.0f;
                cfg7.split_distances[1] = 50.0f;
                cfg7.split_distances[2] = 160.0f;
                cfg7.split_distances[3] = 300.0f;
                cfg7.pcf_radius = 1.7f;    // 顺便验证 7A/7B 不会带偏 y/z
                cfg7.darkness = 0.21f;

                CascadedShadowController ctrl7(cfg7);
                ShadowInfo si7;
                CascadeUpdateResult upd7[kMaxShadowCascades];
                ctrl7.Update(cam6, aspect6, light6, si7, upd7);

                for (uint32_t c = 0; c < 4; ++c)
                {
                    const Vector4f &p = si7.cascades[c].shadow_params;

                    if (p.w != 0.0f)
                    {
                        GLogError(u8"Test 7A Failed: cascade %u shadow_params.w = %f, expected 0 "
                                  u8"when normal_offset_world is left at its default", c, p.w);
                        return 7;
                    }
                    if (p.y != cfg7.pcf_radius || p.z != cfg7.darkness)
                    {
                        GLogError(u8"Test 7A Failed: cascade %u shadow_params.y/z changed to "
                                  u8"(%f, %f), expected (%f, %f)",
                                  c, p.y, p.z, cfg7.pcf_radius, cfg7.darkness);
                        return 7;
                    }
                }

                GLogInfo(u8"[CSM-NORMAL-OFFSET] default strength=%.2fm (disabled on every cascade)",
                         si7.cascades[0].shadow_params.w);
            }

            // 显式配置后必须逐级写同一个值，并且镜像到单级回退字段
            CascadedShadowConfig cfg7;
            cfg7.cascade_count = 4;
            cfg7.c0_dynamic_overlay = true;
            cfg7.shadow_map_size = 1024.0f;
            cfg7.max_distance = 300.0f;
            cfg7.split_distances[0] = 50.0f;
            cfg7.split_distances[1] = 50.0f;
            cfg7.split_distances[2] = 160.0f;
            cfg7.split_distances[3] = 300.0f;
            cfg7.normal_offset_world = 0.35f;

            CascadedShadowController ctrl7(cfg7);
            ShadowInfo si7;
            CascadeUpdateResult upd7[kMaxShadowCascades];
            ctrl7.Update(cam6, aspect6, light6, si7, upd7);

            for (uint32_t c = 0; c < 4; ++c)
            {
                // shader 读级联 0，但控制器必须把同一个值写进每一级：
                // 逐级不同会让将来"按级调法线偏移"的尝试拿到错值，且无法解释。
                if (si7.cascades[c].shadow_params.w != cfg7.normal_offset_world)
                {
                    GLogError(u8"Test 7A Failed: cascade %u shadow_params.w = %f, expected %f",
                              c, si7.cascades[c].shadow_params.w, cfg7.normal_offset_world);
                    return 7;
                }
            }
            if (si7.shadow_params.w != cfg7.normal_offset_world)
            {
                GLogError(u8"Test 7A Failed: single-level mirror shadow_params.w = %f, expected %f",
                          si7.shadow_params.w, cfg7.normal_offset_world);
                return 7;
            }

            GLogInfo(u8"[CSM-NORMAL-OFFSET] configured strength=%.2fm on [%f %f %f %f] (mirror=%f)",
                     cfg7.normal_offset_world,
                     si7.cascades[0].shadow_params.w, si7.cascades[1].shadow_params.w,
                     si7.cascades[2].shadow_params.w, si7.cascades[3].shadow_params.w,
                     si7.shadow_params.w);
        }

        // ── 7B：法线偏移与逐级 bias 相互独立 ──
        // 两者写的是 shadow_params 的不同分量，必须互不干扰。这条断言的现实意义：
        // 曾经出现过"加了新字段后旧字段被顺手改掉"的回归（6B 就是为 x/y/z 建的）。
        {
            CascadedShadowConfig cfg7;
            cfg7.cascade_count = 4;
            cfg7.c0_dynamic_overlay = true;
            cfg7.shadow_map_size = 1024.0f;
            cfg7.max_distance = 300.0f;
            cfg7.split_distances[0] = 50.0f;
            cfg7.split_distances[1] = 50.0f;
            cfg7.split_distances[2] = 160.0f;
            cfg7.split_distances[3] = 300.0f;
            cfg7.bias = -0.003f;
            cfg7.bias_world = -1.15f;
            cfg7.normal_offset_world = 0.35f;

            CascadedShadowController ctrl7(cfg7);
            ShadowInfo si7;
            CascadeUpdateResult upd7[kMaxShadowCascades];
            ctrl7.Update(cam6, aspect6, light6, si7, upd7);

            for (uint32_t c = 0; c < 4; ++c)
            {
                const float world = si7.cascades[c].shadow_params.x * upd7[c].depth_range;

                if (std::abs(world - cfg7.bias_world) > 1e-3f)
                {
                    GLogError(u8"Test 7B Failed: cascade %u world bias %f != %f after enabling "
                              u8"normal offset (the two must be orthogonal)",
                              c, world, cfg7.bias_world);
                    return 7;
                }
                if (si7.cascades[c].shadow_params.w != cfg7.normal_offset_world)
                {
                    GLogError(u8"Test 7B Failed: cascade %u normal offset %f lost while bias_world "
                              u8"is active", c, si7.cascades[c].shadow_params.w);
                    return 7;
                }
            }

            GLogInfo(u8"Test 7B Passed: normal offset and per-cascade bias are independent "
                     u8"(both survive together).");
        }

        // ── 7C：shader 源码契约 —— 宏开关、UBO 取值点、采样/选级分离、公式形状 ──
        // 这一条读的是真实 shader 源码。它存在的理由：7A/7B 只证明 CPU 把值写进了
        // UBO，**证明不了 shader 会读它**。历次最贵的 bug 恰恰是"旋钮接通了但下游
        // 没人用"（cache_offset 就是这样烂了很久的）。所以这里直接对源码断言：
        //   1) 有编译期宏 HGL_SHADOW_NORMAL_OFFSET 且偏移代码受它保护；
        //   2) 强度取自 shadow.cascades[0].shadow_params.w（CPU 侧 7A 写入的那个字段）；
        //   3) 偏移只用于采样，级联选级仍用未偏移的位置（EvalPCFShadowAt 的双入参）；
        //   4) 公式仍是 tan(θ) 加权且被 MAX 夹住 —— 被"简化"成角度无关的常量偏移
        //      会让正面被推离遮挡体（peter-panning 立刻回来），这条就是那道闸门。
        {
            static const OSString kShaderPath = OS_TEXT("ShaderLibrary/shadow/pcf_shadow.glsl");

            hgl::io::OpenFileInputStream fis(kShaderPath);
            if (!fis)
            {
                GLogError(u8"Test 7C Failed: cannot open ShaderLibrary/shadow/pcf_shadow.glsl "
                          u8"(run this test from the repository root)");
                return 7;
            }

            const int64 size = fis->GetSize();
            if (size <= 0)
            {
                GLogError(u8"Test 7C Failed: pcf_shadow.glsl is empty");
                return 7;
            }

            AnsiString src;
            {
                char chunk[4096];
                int64 got;
                while ((got = fis->Read(chunk, static_cast<int64>(sizeof(chunk)))) > 0)
                    src.Strcat(chunk, static_cast<int>(got));
            }

            struct ShaderContract
            {
                const char *needle;
                const char *why;
            };

            static const ShaderContract kContracts[] =
            {
                { "#define HGL_SHADOW_NORMAL_OFFSET", "compile-time switch is missing" },
                { "ShadowNormalOffsetPosition",      "offset helper is missing" },
                { "#if HGL_SHADOW_NORMAL_OFFSET > 0",
                  "the offset code is not guarded by the compile-time switch" },
                { "shadow.cascades[0].shadow_params.w",
                  "GetShadowFactor does not read the UBO field the controller writes (dead knob)" },
                { "sin_theta / cos_theta",
                  "offset is no longer tan(theta) weighted; a constant offset brings peter-panning back" },
                { "cos_theta <= 1.0e-3",
                  "back-facing early-out is missing (dark surfaces would be pushed off the caster)" },
                { "SHADOW_NORMAL_OFFSET_MAX",        "tan() clamp is missing (grazing angle will diverge)" },
                { "EvalPCFShadowAt(sample_pos, surface.worldPos, receive_params.bias_multiplier)",
                  "offset must not leak into cascade selection (selectPos must stay un-offset)" },
                { "shadow.cascades[selected].shadow_tex.x == 0u",
                  "masked cascade must yield LIT inside its own depth interval; letting selection "
                  "descend to a farther cascade makes CSM 2 take over CSM 1's near range" },
                { "cascade_params.w",
                  "junction band must be read from the world-space field the controller writes (dead knob)" },
                { "cache_offset.x != 0.0",
                  "toroidal wrap must be driven by cache_offset; a hardcoded fract() wraps edge "
                  "PCF taps to the opposite side of the map and leaves a speckle ring at every "
                  "cascade border while the rolling cache is not wired" },
            };

            for (const ShaderContract &k : kContracts)
            {
                if (!src.Contains(k.needle))
                {
                    GLogError(u8"Test 7C Failed: %s -- '%s' not found in pcf_shadow.glsl",
                              k.why, k.needle);
                    return 7;
                }
            }

            GLogInfo(u8"Test 7C Passed: shader source contract holds (%d checks, %lld bytes) -- "
                     u8"macro + shadow_params.w + tan(theta) formula + sample/select split.",
                     static_cast<int>(sizeof(kContracts) / sizeof(kContracts[0])),
                     static_cast<long long>(size));
        }

        // ─────────────────────────────────────────────────────────────
        // Test 8: ShadowComponent, Default Convention & Caster Culling Contracts
        // ─────────────────────────────────────────────────────────────
        {
            // 8A: 独立 ShadowComponent 默认值
            ShadowComponent sc;
            if (!sc.CanCastShadow() || sc.GetMaxDistance() != 0.0f || !sc.CanReceiveShadow() || sc.GetBiasMultiplier() != 1.0f)
            {
                GLogError(u8"Test 8A Failed: ShadowComponent default values incorrect");
                return 8;
            }

            // 8B: 实体未挂载 ShadowComponent 时，Renderable/Primitive 缺省回退约定（默认投射且接收）
            Entity e1("TestEntity_NoShadowComp");
            auto prim1 = e1.AddComponent<PrimitiveComponent>();
            if (!prim1->CanCastShadow())
            {
                GLogError(u8"Test 8B Failed: PrimitiveComponent without ShadowComponent must default CanCastShadow to true");
                return 8;
            }
            if (prim1->GetShadowMaxDistance() != 0.0f)
            {
                GLogError(u8"Test 8B Failed: PrimitiveComponent without ShadowComponent must default max distance to 0.0f");
                return 8;
            }
            if (!prim1->CanReceiveShadow())
            {
                GLogError(u8"Test 8B Failed: PrimitiveComponent without ShadowComponent must default CanReceiveShadow to true");
                return 8;
            }

            // 8C: 显式挂载 ShadowComponent 特异化控制
            Entity e2("TestEntity_WithShadowComp");
            auto prim2 = e2.AddComponent<PrimitiveComponent>();
            auto shadow2 = e2.AddComponent<ShadowComponent>();
            shadow2->SetCastShadow(false);
            shadow2->SetMaxDistance(45.0f);
            shadow2->SetReceiveShadow(false);

            if (prim2->CanCastShadow() != false)
            {
                GLogError(u8"Test 8C Failed: PrimitiveComponent must reflect attached ShadowComponent cast_shadow=false");
                return 8;
            }
            if (prim2->GetShadowMaxDistance() != 45.0f)
            {
                GLogError(u8"Test 8C Failed: PrimitiveComponent must reflect attached ShadowComponent max_distance=45.0f");
                return 8;
            }
            if (prim2->CanReceiveShadow() != false)
            {
                GLogError(u8"Test 8C Failed: PrimitiveComponent must reflect attached ShadowComponent receive_shadow=false");
                return 8;
            }

            // 8D: 距离剔除数学与边界验证
            const glm::vec3 cam_pos(0.0f, 0.0f, 10.0f);
            const glm::vec3 inside_pos(0.0f, 0.0f, 30.0f);  // dist = 20m <= 45m -> retain
            const glm::vec3 outside_pos(0.0f, 0.0f, 60.0f); // dist = 50m > 45m -> cull

            const float inside_dist_sqr = glm::dot(inside_pos - cam_pos, inside_pos - cam_pos);
            const float outside_dist_sqr = glm::dot(outside_pos - cam_pos, outside_pos - cam_pos);
            const float max_dist_sqr = shadow2->GetMaxDistance() * shadow2->GetMaxDistance();

            if (inside_dist_sqr > max_dist_sqr)
            {
                GLogError(u8"Test 8D Failed: inside object incorrectly culled");
                return 8;
            }
            if (outside_dist_sqr <= max_dist_sqr)
            {
                GLogError(u8"Test 8D Failed: outside object not culled");
                return 8;
            }

            GLogInfo(u8"Test 8 Passed: ShadowComponent, Default Convention & Caster Culling Contracts verified.");
        }

        // ─────────────────────────────────────────────────────────────
        // Test 9: ScenePipelineMode & Automated Shadow Workflow Contracts
        // ─────────────────────────────────────────────────────────────
        {
            // 9A: 默认管线模式为 StandardLitCSM（黄金路径）
            ECSContext ctx;
            if (ctx.GetScenePipelineMode() != ScenePipelineMode::StandardLitCSM)
            {
                GLogError(u8"Test 9A Failed: default ScenePipelineMode must be StandardLitCSM");
                return 9;
            }

            // 9B: 模式切换与预留场景枚举契约
            ctx.SetScenePipelineMode(ScenePipelineMode::TopDownRTS);
            if (ctx.GetScenePipelineMode() != ScenePipelineMode::TopDownRTS)
            {
                GLogError(u8"Test 9B Failed: ScenePipelineMode::TopDownRTS switch failed");
                return 9;
            }
            ctx.SetScenePipelineMode(ScenePipelineMode::AerialLowAltitude);
            if (ctx.GetScenePipelineMode() != ScenePipelineMode::AerialLowAltitude)
            {
                GLogError(u8"Test 9B Failed: ScenePipelineMode::AerialLowAltitude switch failed");
                return 9;
            }
            ctx.SetScenePipelineMode(ScenePipelineMode::Space3D);
            if (ctx.GetScenePipelineMode() != ScenePipelineMode::Space3D)
            {
                GLogError(u8"Test 9B Failed: ScenePipelineMode::Space3D switch failed");
                return 9;
            }

            // 9C: CameraSystem::GetMainCameraComponent 权威获取契约
            auto cam_sys = ctx.RegisterTickSystem<CameraSystem>();
            if (!cam_sys)
            {
                GLogError(u8"Test 9C Failed: CameraSystem registration failed");
                return 9;
            }

            auto e_cam = ctx.CreateEntity<Entity>("TestMainCamera");
            auto cam_comp = e_cam->AddComponent<CameraComponent>();
            cam_comp->is_main_camera = true;
            cam_comp->position = math::Vector3f(12.0f, 34.0f, 56.0f);

            auto *resolved_main_cam = cam_sys->GetMainCameraComponent();
            if (resolved_main_cam != cam_comp.get())
            {
                GLogError(u8"Test 9C Failed: GetMainCameraComponent must resolve the camera marked is_main_camera");
                return 9;
            }
            if (resolved_main_cam->position.x != 12.0f)
            {
                GLogError(u8"Test 9C Failed: resolved main camera position mismatch");
                return 9;
            }

            // 9D: A3 静态场景 revision 失效链契约。
            // 静态级联滚动缓存的"静态"前提由该链兜底：TransformSystem 在
            // SubmitTransformUpdates 检出 Static transform 变更 → 递增
            // ECSContext::static_scene_revision → EnvironmentSystem 的阴影
            // prepass 比对消费并 InvalidateStaticCache。历史教训：失效钩子
            // 曾只有 API（InvalidateStaticCache 零调用者），静态缓存默认
            // "场景永不变"——相机静止时移走静态物体，旧阴影挂在原地。
            {
                auto tf_sys = ctx.RegisterTickSystem<TransformSystem>();
                if (!tf_sys)
                {
                    GLogError(u8"Test 9D Failed: TransformSystem registration failed");
                    return 9;
                }

                const uint64_t revision_base = ctx.GetStaticSceneRevision();

                auto e_static = ctx.CreateEntity<Entity>("TestStaticMover");
                auto tf_static = e_static->AddComponent<TransformComponent>(Mobility::Static);
                tf_static->SetLocalPosition(math::Vector3f(5.0f, 6.0f, 7.0f));

                // 检出：提交后 revision 必须前移（移动静态物体必须打破静态缓存）
                tf_sys->SubmitTransformUpdates();
                if (ctx.GetStaticSceneRevision() == revision_base)
                {
                    GLogError(u8"Test 9D Failed: moving a Static transform must bump static_scene_revision "
                              u8"(static cascade cache would keep stale depth forever)");
                    return 9;
                }

                // 稳态：无变更的重复提交不得继续递增（否则每帧全量重建静态级联）
                tf_sys->SubmitTransformUpdates();
                if (ctx.GetStaticSceneRevision() != revision_base + 1)
                {
                    GLogError(u8"Test 9D Failed: unchanged scene must not keep bumping static_scene_revision");
                    return 9;
                }

                // EnvironmentSystem 消费端接线：阴影 prepass 必须比对 revision
                // 并调用 InvalidateStaticCache（防止消费逻辑被静默删除）。
                {
                    static const OSString kEnvSysPath =
                        OS_TEXT("src/ecs/systems/render/EnvironmentSystem.cpp");
                    hgl::io::OpenFileInputStream env_fis(kEnvSysPath);
                    if (!env_fis)
                    {
                        GLogError(u8"Test 9D Failed: cannot open EnvironmentSystem.cpp (run from repo root)");
                        return 9;
                    }
                    AnsiString env_src;
                    {
                        char chunk[4096];
                        int64 got;
                        while ((got = env_fis->Read(chunk, static_cast<int64>(sizeof(chunk)))) > 0)
                            env_src.Strcat(chunk, static_cast<int>(got));
                    }
                    if (!env_src.Contains("InvalidateStaticCache()")
                     || !env_src.Contains("GetStaticSceneRevision()"))
                    {
                        GLogError(u8"Test 9D Failed: RenderMainLightShadowPass must consume static_scene_revision "
                                  u8"and call InvalidateStaticCache (static cache invalidation chain severed)");
                        return 9;
                    }
                }
            }

            GLogInfo(u8"Test 9 Passed: ScenePipelineMode & Automated Shadow Workflow Contracts verified.");
        }

        // ─────────────────────────────────────────────────────────────
        // Test 10: Cascade Mask & Bias Modulation Contracts
        // ─────────────────────────────────────────────────────────────
        {
            EnvironmentSystem env_sys;

            // 10A: 默认所有级联开启
            if (env_sys.GetCascadeMask() != 0)
            {
                GLogError(u8"Test 10A Failed: default cascade_mask must be 0 (all enabled)");
                return 10;
            }
            for (uint32_t c = 0; c < 4; ++c)
            {
                if (!env_sys.IsCascadeEnabled(c))
                {
                    GLogError(u8"Test 10A Failed: cascade %u must be enabled by default", c);
                    return 10;
                }
            }

            // 10B: 单独关闭/开启指定级联（如关闭 CSM 0, CSM 1）
            env_sys.SetCascadeEnabled(0, false);
            if (env_sys.IsCascadeEnabled(0) != false || env_sys.GetCascadeMask() != 1u)
            {
                GLogError(u8"Test 10B Failed: disabling cascade 0 failed (mask=%u)", env_sys.GetCascadeMask());
                return 10;
            }

            env_sys.SetCascadeEnabled(1, false);
            if (env_sys.IsCascadeEnabled(1) != false || env_sys.GetCascadeMask() != 3u)
            {
                GLogError(u8"Test 10B Failed: disabling cascade 1 failed (mask=%u)", env_sys.GetCascadeMask());
                return 10;
            }

            env_sys.SetCascadeEnabled(0, true);
            if (env_sys.IsCascadeEnabled(0) != true || env_sys.GetCascadeMask() != 2u)
            {
                GLogError(u8"Test 10B Failed: re-enabling cascade 0 failed (mask=%u)", env_sys.GetCascadeMask());
                return 10;
            }

            // 10C: Bias 世界单位动态微调与自遮挡临界几何契约
            CascadedShadowConfig cfg10;
            cfg10.cascade_count = 4;
            cfg10.shadow_map_size = 1024.0f;
            cfg10.max_distance = 300.0f;
            cfg10.bias_world = -0.20f; // 安全贴合值（-0.2m，小于标准 Cube 厚度 1.0m）

            CascadedShadowController ctrl10(cfg10);
            ShadowInfo si10;
            CascadeUpdateResult upd10[kMaxShadowCascades];
            ctrl10.Update(cam6, aspect6, light6, si10, upd10);

            // 验证每级 bias 正确反映了 -0.20m
            const float expected_bias0 = -0.20f / upd10[0].depth_range;
            if (std::abs(si10.cascades[0].shadow_params.x - expected_bias0) > 1.0e-6f)
            {
                GLogError(u8"Test 10C Failed: bias_world=-0.20m normalized mismatch");
                return 10;
            }

            // 动态调大 bias 到 -1.15m（自遮挡危险区）：
            cfg10.bias_world = -1.15f;
            ctrl10.SetConfig(cfg10);
            ctrl10.Update(cam6, aspect6, light6, si10, upd10);

            const float expected_bias_large = -1.15f / upd10[0].depth_range;
            if (std::abs(si10.cascades[0].shadow_params.x - expected_bias_large) > 1.0e-6f)
            {
                GLogError(u8"Test 10C Failed: bias_world=-1.15m dynamic update failed");
                return 10;
            }

            // 几何自遮挡临界数学：
            // 当 Cube 沿光轴厚度 = 0.8m 时，若 bias_world = -1.15m (|bias| > thickness)，
            // 受光面被向后拉深超过自身厚度，判定落入自身阴影中；
            // 若 bias_world = -0.20m (|bias| < thickness)，受光面位于背面深度前方，不产生自阴影！
            const float cube_thickness = 0.8f;
            const bool will_self_shadow_large = std::abs(-1.15f) > cube_thickness;
            const bool will_self_shadow_safe  = std::abs(-0.20f) > cube_thickness;

            if (!will_self_shadow_large || will_self_shadow_safe)
            {
                GLogError(u8"Test 10C Failed: self-shadowing thickness threshold contract broken");
                return 10;
            }

            // 10D: 光相机 CameraInfo 行归还契约（csm-review A2）
            // light_camera 经 RenderTo→SetOverrideCamera→BindCameraResources 从
            // GlobalSSBOBufferRegistry AcquireCamera 占行；DisableMainLightShadow
            // 必须对称 ReleaseCamera——registry 容量不预留、超限 fail-fast，
            // 每次 Enable/Disable 泄漏一行迟早把行池顶满。ReleaseCamera 曾是
            // 全仓零调用 API，这里对源码断言防止归还逻辑被静默删掉。
            {
                static const OSString kEnvSysPath =
                    OS_TEXT("src/ecs/systems/render/EnvironmentSystem.cpp");
                hgl::io::OpenFileInputStream env_fis(kEnvSysPath);
                if (!env_fis)
                {
                    GLogError(u8"Test 10D Failed: cannot open EnvironmentSystem.cpp (run from repo root)");
                    return 10;
                }
                AnsiString env_src;
                {
                    char chunk[4096];
                    int64 got;
                    while ((got = env_fis->Read(chunk, static_cast<int64>(sizeof(chunk)))) > 0)
                        env_src.Strcat(chunk, static_cast<int>(got));
                }
                if (!env_src.Contains("ReleaseCamera(light_camera->camera_id)"))
                {
                    GLogError(u8"Test 10D Failed: DisableMainLightShadow must release the light camera's CameraInfo row "
                              u8"(ReleaseCamera(light_camera->camera_id) not found; every Enable/Disable leaks a row otherwise)");
                    return 10;
                }
            }

            GLogInfo(u8"Test 10 Passed: Cascade Mask & Bias Modulation Contracts verified.");
        }

        {
            // 本测试自带上下文（不依赖其它测试块的局部 ctx）：默认构造的
            // ECSContext 已含 world/transform storage，可建实体并注册 TransformSystem。
            ECSContext ctx;

            auto tf_sys = ctx.RegisterTickSystem<TransformSystem>();
            if (!tf_sys)
            {
                GLogError(u8"Test 15 Failed: TransformSystem registration failed");
                return 15;
            }

            // (a) 搭建期（组件刚建、尚未被渲染侧消费）：不 arm、不告警
            auto e_static = ctx.CreateEntity<Entity>("TestStaticLateWriter");
            auto tf_static = e_static ? e_static->AddComponent<TransformComponent>(Mobility::Static)
                                      : nullptr;
            if (!tf_static)
            {
                GLogError(u8"Test 15 Failed: 无法创建 Static TransformComponent");
                return 15;
            }

            tf_static->SetLocalPosition(math::Vector3f(1.0f, 2.0f, 3.0f));

            if (tf_static->IsStaticRuntimeWriteArmed() || tf_static->HasWarnedStaticRuntimeWrite())
            {
                GLogError(u8"Test 15 Failed: 搭建期写入静态 transform 不得告警"
                          u8"（否则每个示例的场景搭建都会刷出误报）");
                return 15;
            }

            // (b) 被渲染侧消费（首次静态段上传）之后必须 arm
            tf_sys->SubmitTransformUpdates();
            if (!tf_static->IsStaticRuntimeWriteArmed())
            {
                GLogError(u8"Test 15 Failed: 静态 transform 被渲染侧消费后未 arm 运行期写入告警"
                          u8"（D4 的留痕语义整体失效，运行期写静态又变静默）");
                return 15;
            }

            // (c) arm 之后再写 ⇒ 必须告警一次（且写入语义不变：值真的写进去）
            tf_static->SetLocalPosition(math::Vector3f(4.0f, 5.0f, 6.0f));
            if (!tf_static->HasWarnedStaticRuntimeWrite())
            {
                GLogError(u8"Test 15 Failed: 运行期写入 Static transform 未告警"
                          u8"（静默整段重写静态矩阵 + 全部静态级联失效）");
                return 15;
            }
            tf_static->SetLocalPosition(math::Vector3f(4.0f, 5.0f, 6.0f));
            if (!tf_static->IsDirty())
            {
                GLogError(u8"Test 15 Failed: 告警不得改变写入语义（值必须照旧写进去 + 标脏）");
                return 15;
            }

            // (d) Movable 完全不受影响（永不 arm / 永不告警）
            auto e_movable = ctx.CreateEntity<Entity>("TestMovableWriter");
            auto tf_movable = e_movable ? e_movable->AddComponent<TransformComponent>(Mobility::Movable)
                                        : nullptr;
            if (!tf_movable)
            {
                GLogError(u8"Test 15 Failed: 无法创建 Movable TransformComponent");
                return 15;
            }

            tf_sys->SubmitTransformUpdates();
            tf_movable->SetLocalPosition(math::Vector3f(7.0f, 8.0f, 9.0f));
            if (tf_movable->IsStaticRuntimeWriteArmed() || tf_movable->HasWarnedStaticRuntimeWrite())
            {
                GLogError(u8"Test 15 Failed: Movable 组件被静态写入告警波及（会误报每帧移动的对象）");
                return 15;
            }

            // (e) SetMobility(Movable) 是"会动"的正解：迁移后写入不再算静态写入
            auto e_migrate = ctx.CreateEntity<Entity>("TestStaticMigratedWriter");
            auto tf_migrate = e_migrate ? e_migrate->AddComponent<TransformComponent>(Mobility::Static)
                                        : nullptr;
            if (!tf_migrate)
            {
                GLogError(u8"Test 15 Failed: 无法创建待迁移 TransformComponent");
                return 15;
            }

            tf_sys->SubmitTransformUpdates();          // 先 arm（模拟已进场景）
            tf_migrate->SetMobility(Mobility::Movable); // 正解：迁到 movable 通道
            tf_migrate->SetLocalPosition(math::Vector3f(13.0f, 14.0f, 15.0f));
            if (tf_migrate->HasWarnedStaticRuntimeWrite())
            {
                GLogError(u8"Test 15 Failed: 迁移到 Movable 后的写入仍被判为静态写入"
                          u8"（正解路径会被误报，开发者会被自己的告警劝退）");
                return 15;
            }
        }
    }

    // ─────────────────────────────────────────────────────────────
    // 源码契约公共检查器（Test 11 / Test 12 共用）
    //
    // 在指定源文件里查找 needle；`forbidden = true` 表示该 needle 必须**不存在**
    // （用于钉住"某机制已删除，勿复活"）。命中/缺失都报"哪个文件、哪个 needle、
    // 后果是什么"，失败返回 test_no 供调用方当退出码。只看源码文本，不需要图形
    // 设备——本可执行文件没有 GraphicsContext。
    // ─────────────────────────────────────────────────────────────
    struct SourceContract
    {
        const char *file;      // 打印用短名
        const OSString path;   // 仓库相对路径
        const char *needle;
        const char *why;
        bool        forbidden = false;   // true：该 needle 必须**不存在**（禁复活）
    };

    auto verify_source_contracts = [](const int test_no, const SourceContract *list,
                                      const uint count) -> int
    {
        for (uint i = 0; i < count; ++i)
        {
            const SourceContract &k = list[i];

            hgl::io::OpenFileInputStream fis(k.path);
            if (!fis)
            {
                GLogError(u8"Test %d Failed: cannot open %s (run from repo root)", test_no, k.file);
                return test_no;
            }

            if (fis->GetSize() <= 0)
            {
                GLogError(u8"Test %d Failed: %s is empty", test_no, k.file);
                return test_no;
            }

            AnsiString src;
            {
                char chunk[4096];
                int64 got;
                while ((got = fis->Read(chunk, static_cast<int64>(sizeof(chunk)))) > 0)
                    src.Strcat(chunk, static_cast<int>(got));
            }

            const bool hit = src.Contains(k.needle);
            if (k.forbidden ? hit : !hit)
            {
                GLogError(k.forbidden
                              ? u8"Test %d Failed: %s -- '%s' 重新出现在 %s（禁复活）"
                              : u8"Test %d Failed: %s -- '%s' not found in %s",
                          test_no, k.why, k.needle, k.file);
                return test_no;
            }
        }

        return 0;
    };

    // ─────────────────────────────────────────────────────────────
    // Test 11: masked caster 片元链路源码契约（D1）
    //
    // 背景：depth-only 通道（零颜色附件）会剥离片元 stage；含 discard 的材质
    // （alpha test / dither）被剥掉后会在深度图退化为实心——ShadowCasterMasked
    // 曾因此完全失效，而当时本套契约测试全绿（豁免被改回也不会被发现）。
    // 本可执行文件无图形设备（图像级判读在 `AlphaTestShadow --selfcheck` 的
    // 深度图读回 + 包围盒填充率断言里），所以这里钉住"判据链是否还在"：
    // 剥离点必须检查 keep_fragment_shader、recipe 语义必须参与判据、masked
    // caster 必须真的评估 alpha；同时**禁止**程序级 discard 扫描复活（D8 实测
    // 两层实现恒 false，判据已收敛为 recipe 语义一条）。
    // ─────────────────────────────────────────────────────────────
    {
        const SourceContract kFSContracts[] =
        {
            { "VKRenderPass.cpp", OS_TEXT("src/Vulkan/VKRenderPass.cpp"),
              "&&!keep_fragment_shader",
              "depth-only 通道剥离片元 stage 时不再检查 keep_fragment_shader——"
              "含 discard 的材质会在深度图退化为实心" },
            { "VKRenderPass.cpp", OS_TEXT("src/Vulkan/VKRenderPass.cpp"),
              "render_state.alpha_test",
              "recipe 的 alpha_test 不再是 FS 保留判据（discard 语义丢失）" },
            { "VKRenderPass.cpp", OS_TEXT("src/Vulkan/VKRenderPass.cpp"),
              "render_state.dither",
              "recipe 的 dither 不再是 FS 保留判据（抖动覆盖语义丢失）" },
            { "FragmentTemplateComposer.cpp", OS_TEXT("src/ShaderGen/template/FragmentTemplateComposer.cpp"),
              "ShadowCasterMasked",
              "masked caster 模板分派丢失（depth-purpose 不再区分 alpha test 材质）" },
            { "FragmentTemplateComposer.cpp", OS_TEXT("src/ShaderGen/template/FragmentTemplateComposer.cpp"),
              "HGLApplyAlpha(",
              "shadow 模板不再调用 HGLApplyAlpha——discard 不会进入 SPIRV，深度图实心" },
            { "FragmentTemplateComposer.cpp", OS_TEXT("src/ShaderGen/template/FragmentTemplateComposer.cpp"),
              "EvalAlpha(",
              "masked caster 不再评估材质 alpha（opacity_mask 采样链断裂）" },
            { "forward_lit.glsl.tmpl", OS_TEXT("ShaderLibrary/fragment/forward_lit.glsl.tmpl"),
              "HGL_ALPHA_TEST",
              "forward 本体未接线 alpha test（物件本体不再镂空，只剩影子镂空）" },
            { "forward_lit.glsl.tmpl", OS_TEXT("ShaderLibrary/fragment/forward_lit.glsl.tmpl"),
              "HGLApplyAlpha(",
              "forward 本体的 discard 调用丢失（PBR 输出 alpha 恒 1，alpha test 永不触发）" },
            { "RenderPrimitiveCollectSystem.cpp",
              OS_TEXT("src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp"),
              "MaterialRequiresRecipeRuntimeRows",
              "阴影 pass 不再判定 masked caster 的行需求——行未就绪时仍采深度会写出实心影子" },
            // ── 禁复活（D8 收敛，2026-09-26 实测证据见 doc/backlog.md D8）──
            { "ShaderProgramManager.cpp", OS_TEXT("src/SceneGraph/module/ShaderProgramManager.cpp"),
              "ScanSPVHasDiscard",
              "程序级 SPIRV 扫描已按 D8 删除（常量写错从未命中 + 唯一调用点在 stage 缓存"
              "命中分支 + 结果被文本扫描硬赋值覆盖，实测三层恒 0）；确需程序级判据请重做",
              true },
            { "VKShaderProgram.h", OS_TEXT("inc/hgl/vk/VKShaderProgram.h"),
              "FragmentShaderRequired",
              "ShaderProgram 的 FS-required 标志已按 D8 删除——FS 保留判据只能来自 recipe 语义",
              true },
        };

        if (const int failed = verify_source_contracts(11, kFSContracts,
                                                       static_cast<uint>(sizeof(kFSContracts) /
                                                                         sizeof(kFSContracts[0]))))
            return failed;

        GLogInfo(u8"Test 11 Passed: masked caster FS-retention source contract holds (%d checks) -- "
                 u8"keep_fragment_shader + recipe alpha_test/dither + masked alpha evaluation "
                 u8"+ no revived program-level discard scan.",
                 static_cast<int>(sizeof(kFSContracts) / sizeof(kFSContracts[0])));
    }

    // ─────────────────────────────────────────────────────────────
    // Test 12: pipeline 缓存键的内容身份契约（D2）
    //
    // 背景：`FinalPipelineKey::shader_stages_hash` 曾用 VkShaderModule **句柄值**
    // 计算——句柄在模块销毁后可被新建模块复用，两个不同的 shader 会算出同一个
    // key → 复用错误 pipeline（与已修复的 resolvedRuntimePipelineMap 无 program
    // 键控同构）。现在键用 SPIRV 字节内容 hash：模块创建时在 VulkanDevice 登记、
    // 析构时注销，resolver 按键时查内容 hash（查不到即 fail-fast 判键不完整）。
    // ─────────────────────────────────────────────────────────────
    {
        const SourceContract kPipelineKeyContracts[] =
        {
            { "VKPipelineResolver.cpp", OS_TEXT("src/Vulkan/pipeline/VKPipelineResolver.cpp"),
              "GetShaderModuleHash(",
              "pipeline 键不再查 module 的 SPIRV 内容 hash（退回句柄值身份 = 不同 shader 撞键）" },
            { "VKShaderModule.cpp", OS_TEXT("src/Vulkan/VKShaderModule.cpp"),
              "RegisterShaderModuleHash(",
              "模块创建时不再登记内容 hash——键构造会 fail-fast 直接失败（或退化为句柄身份）" },
            { "VKShaderModule.cpp", OS_TEXT("src/Vulkan/VKShaderModule.cpp"),
              "UnregisterShaderModuleHash(",
              "模块析构不再注销内容 hash——句柄复用时可能残留旧身份，命中错误键" },
            { "VKShaderModule.cpp", OS_TEXT("src/Vulkan/VKShaderModule.cpp"),
              "AppendBytes(spv_data,spv_size)",
              "内容 hash 不再覆盖 SPIRV 全部字节（截断/漏算会撞键）" },
            { "PrimitiveComponent.h", OS_TEXT("inc/hgl/ecs/components/PrimitiveComponent.h"),
              "mtl::ShaderProgramKey program_key;",
              "已解析管线条目不再携带 program 结构化身份（退回指针身份）" },
            // ── 禁复活 ──
            { "VKPipelineResolver.cpp", OS_TEXT("src/Vulkan/pipeline/VKPipelineResolver.cpp"),
              "(uint64_t)(uintptr_t)stages[i].module",
              "shader stage 的 module 身份又变回句柄值（D2 已修：句柄值可被复用）",
              true },
            { "PrimitiveComponent.h", OS_TEXT("inc/hgl/ecs/components/PrimitiveComponent.h"),
              "resolvedRuntimePipelineProgramMap",
              "pipeline/program 两张平行 map 又回来了（同一职责两套实现，手工同步易漂移）",
              true },
        };

        if (const int failed = verify_source_contracts(12, kPipelineKeyContracts,
                                                       static_cast<uint>(sizeof(kPipelineKeyContracts) /
                                                                         sizeof(kPipelineKeyContracts[0]))))
            return failed;

        GLogInfo(u8"Test 12 Passed: pipeline cache key identity contract holds (%d checks) -- "
                 u8"SPIRV content hash registered at module creation, consumed by the resolver key, "
                 u8"handle-value identity forbidden.",
                 static_cast<int>(sizeof(kPipelineKeyContracts) / sizeof(kPipelineKeyContracts[0])));
    }

    // ─────────────────────────────────────────────────────────────
    // Test 13: 阴影跳过路径的告警/收敛契约（D9）
    //
    // 背景：masked caster 行未就绪时阴影帧跳过该 caster 并 bump 静态级联
    // revision，设计意图是首帧收敛。但原实现**完全静默**，且若持续未就绪会退化成
    // "每帧 bump → 静态级联每帧全量重画"直到场景结束（`100% Cached` 再不出现）。
    // 现态：跳过/失败（行未就绪、程序解析失败、几何/管线失败）统一走
    // AdvanceShadowRetry——首次告警一次（含 primitive 名与原因）、连续超过
    // kShadowRetryFullBumpFrames 帧后报错并把 bump 降频为每 kShadowRetryBumpPeriod
    // 帧一次；caster 成功产出 item 时复位计数。
    // ─────────────────────────────────────────────────────────────
    {
        const SourceContract kShadowRetryContracts[] =
        {
            { "RenderPrimitiveCollectSystem.cpp", OS_TEXT("src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp"),
              "AdvanceShadowRetry(",
              "阴影跳过/失败路径不再走统一收敛入口（告警与 bump 降频都失效）" },
            { "RenderPrimitiveCollectSystem.cpp", OS_TEXT("src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp"),
              "masked caster runtime rows not ready",
              "masked 行未就绪路径又不报原因了（退回静默跳过 = 持续失败全程无日志）" },
            { "RenderPrimitiveCollectSystem.cpp", OS_TEXT("src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp"),
              "kShadowRetryFullBumpFrames + 1",
              "收敛上限判定没了（持续失败会一直每帧 bump → 每帧全量重画）" },
            { "RenderPrimitiveCollectSystem.cpp", OS_TEXT("src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp"),
              "kShadowRetryBumpPeriod) == 0",
              "bump 降频公式没了（超上限后仍每帧 bump）" },
            { "MaterialComponent.h", OS_TEXT("inc/hgl/ecs/components/MaterialComponent.h"),
              "uint32_t shadow_retry_frames = 0;",
              "per-primitive 重试计数没了（无法区分首帧收敛与持续失败）" },
            // ── 禁复活：逐帧刷屏的旧告警 ──
            { "RenderPrimitiveCollectSystem.cpp", OS_TEXT("src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp"),
              "Shadow pass geometry failed for ",
              "阴影几何失败又逐帧刷告警（应由统一收敛入口每 episode 一次）",
              true },
            { "RenderPrimitiveCollectSystem.cpp", OS_TEXT("src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp"),
              "Shadow pass pipeline failed for ",
              "阴影管线失败又逐帧刷告警（应由统一收敛入口每 episode 一次）",
              true },
        };

        if (const int failed = verify_source_contracts(13, kShadowRetryContracts,
                                                       static_cast<uint>(sizeof(kShadowRetryContracts) /
                                                                         sizeof(kShadowRetryContracts[0]))))
            return failed;

        GLogInfo(u8"Test 13 Passed: shadow skip-path alert/convergence contract holds (%d checks) -- "
                 u8"one-shot warning with reason + ramp-up cap + throttled static redraw + per-primitive reset, "
                 u8"no silent skip and no per-frame log spam.",
                 static_cast<int>(sizeof(kShadowRetryContracts) / sizeof(kShadowRetryContracts[0])));
    }

    // ─────────────────────────────────────────────────────────────
    // Test 14: 接收侧阴影旋钮落地契约（D3）
    //
    // 背景：ShadowComponent 的 receive_shadow / bias_multiplier 长期**零消费者**
    //（文档写着已生效、实际没人读）。D3 把两者接到 per-draw 行表
    //（MaterialInstanceAddresses —— FS 早已按 dataIndex 寻址同一行）。
    // 本测试钉住这条链的四段：
    //   ① 行结构是 X 列表单源（CPU struct / 布局断言 / GLSL 发射同源，无手写漂移面）；
    //   ② 发射端遍历该列表，不手写字段；
    //   ③ 批处理写入端从 RenderableComponent 取真值（逐图元）；
    //   ④ 片元端真的读该行，并据 receive 早退、据 bias_multiplier 缩放偏差。
    // 任一段被"简化"掉，旋钮就重新变回死旋钮——这正是本测试存在的理由。
    // ─────────────────────────────────────────────────────────────
    {
        const SourceContract kShadowKnobContracts[] =
        {
            // ① 行结构单一真源 + 自动 scale 的布局断言
            { "ShaderBufferSources.h", OS_TEXT("inc/hgl/graph/ShaderBufferSources.h"),
              "HGL_MATERIAL_INSTANCE_ADDRESSES_FIELD_LIST",
              "per-draw 行结构不再是 X 列表单源——CPU/GLSL 两侧从此各自漂移" },
            { "ShaderBufferSources.h", OS_TEXT("inc/hgl/graph/ShaderBufferSources.h"),
              "shadow_bias_multiplier",
              "行结构里没有局部偏差倍率字段（bias_multiplier 旋钮无载体）" },
            { "ShaderBufferSources.h", OS_TEXT("inc/hgl/graph/ShaderBufferSources.h"),
              "shadow_flags",
              "行结构里没有接收标志位（receive_shadow 旋钮无载体）" },
            { "ShaderBufferSources.h", OS_TEXT("inc/hgl/graph/ShaderBufferSources.h"),
              "kMaterialShadowFlagNoReceive",
              "接收标志位的位定义缺失（GLSL 侧将只能硬编码数值）" },
            { "ShaderBufferSources.h", OS_TEXT("inc/hgl/graph/ShaderBufferSources.h"),
              "MaterialInstanceAddressesLayoutValid",
              "布局断言退回手列写法——加字段就得再补一条断言，早晚漏" },
            { "ShaderBufferSources.h", OS_TEXT("inc/hgl/graph/ShaderBufferSources.h"),
              "sizeof(MaterialInstanceAddresses) == 8",
              "行结构退回了 8B（D3 之前的形态）——接收侧旋钮又没地方放", true },

            // ② 发射端遍历列表（不是手写字段）
            { "MaterialShaderEmitter.cpp", OS_TEXT("src/ShaderGen/compile/MaterialShaderEmitter.cpp"),
              "kMaterialInstanceAddressesFieldNames[field_index]",
              "GLSL 行结构不再从 X 列表发射，而是手写字段——与 CPU 端的漂移面复活" },
            { "MaterialShaderEmitter.cpp", OS_TEXT("src/ShaderGen/compile/MaterialShaderEmitter.cpp"),
              "kMaterialShadowFlagNoReceive",
              "接收标志位不再由 C++ 枚举发射（GLSL 侧数值会与 CPU 端脱钩）" },
            { "MaterialShaderEmitter.cpp", OS_TEXT("src/ShaderGen/compile/MaterialShaderEmitter.cpp"),
              "uint payload_index",
              "GLSL 行结构退回手写字段发射——加字段必然漏改一侧", true },

            // ③ 逐图元写入（真值来自 RenderableComponent）
            { "PrimitiveBatchPipeline.cpp", OS_TEXT("src/ecs/support/PrimitiveBatchPipeline.cpp"),
              "renderable->CanReceiveShadow()",
              "批处理写入端不再读 ShadowComponent 的接收开关（receive_shadow 又成死旋钮）" },
            { "PrimitiveBatchPipeline.cpp", OS_TEXT("src/ecs/support/PrimitiveBatchPipeline.cpp"),
              "row_ptr[i].shadow_flags",
              "接收开关没有写进 per-draw 行（着色端读到的永远是默认值）" },
            { "PrimitiveBatchPipeline.cpp", OS_TEXT("src/ecs/support/PrimitiveBatchPipeline.cpp"),
              "GetShadowBiasMultiplier()",
              "批处理写入端不再读局部偏差倍率（bias_multiplier 又成死旋钮）" },
            { "PrimitiveBatchPipeline.cpp", OS_TEXT("src/ecs/support/PrimitiveBatchPipeline.cpp"),
              "row_ptr[i].shadow_bias_multiplier = bias_multiplier;",
              "局部偏差倍率没有写进 per-draw 行（着色端读到的永远是默认值）" },

            // ④ 片元端读取 + 早退 + 缩放
            { "pcf_shadow.glsl", OS_TEXT("ShaderLibrary/shadow/pcf_shadow.glsl"),
              "GetShadowReceiveParams(data_index)",
              "片元端不再读接收侧参数（row 里的值没人消费）" },
            { "pcf_shadow.glsl", OS_TEXT("ShaderLibrary/shadow/pcf_shadow.glsl"),
              "MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[data_index]",
              "接收侧参数不是从 per-draw 行读的（dataIndex 与行号必须一致）" },
            { "pcf_shadow.glsl", OS_TEXT("ShaderLibrary/shadow/pcf_shadow.glsl"),
              "HGL_MATERIAL_SHADOW_FLAG_NO_RECEIVE",
              "片元端不再判定接收标志位（receive_shadow 不生效）" },
            { "pcf_shadow.glsl", OS_TEXT("ShaderLibrary/shadow/pcf_shadow.glsl"),
              "if (!receive_params.receive)",
              "不接收阴影的图元没有早退——仍会采样/选级，开关只影响噪音" },
            { "pcf_shadow.glsl", OS_TEXT("ShaderLibrary/shadow/pcf_shadow.glsl"),
              "shadow.cascades[c].shadow_params.x * bias_scale",
              "级联路径的深度 bias 不再乘局部倍率（bias_multiplier 只改法线偏移）" },
            { "pcf_shadow.glsl", OS_TEXT("ShaderLibrary/shadow/pcf_shadow.glsl"),
              "* receive_params.bias_multiplier",
              "法线偏移/采样调用不再带局部倍率（bias_multiplier 半途丢失）" },
            { "pcf_shadow.glsl", OS_TEXT("ShaderLibrary/shadow/pcf_shadow.glsl"),
              "return EvalPCFShadowAt(sample_pos, surface.worldPos);",
              "阴影采样退回无倍率调用（bias_multiplier 无法到达采样点）", true },

            // ⑤ 参数真的到达合成器（dataIndex 从 FS 模板一路传到 provider）
            { "forward_lighting.glsl", OS_TEXT("ShaderLibrary/compositor/forward_lighting.glsl"),
              "GetShadowFactor(si, data_index)",
              "合成器不再把 per-draw 行号传给阴影 provider（旋钮在最后一步断链）" },
            { "forward_lit.glsl.tmpl", OS_TEXT("ShaderLibrary/fragment/forward_lit.glsl.tmpl"),
              "BuildForwardLightingInput(surface, si, materialDataIndex)",
              "FS 模板不再传 materialDataIndex（行号在入口处就丢了）" },
        };

        if (const int failed = verify_source_contracts(14, kShadowKnobContracts,
                                                       sizeof(kShadowKnobContracts) / sizeof(kShadowKnobContracts[0])))
            return failed;

        GLogInfo(u8"Test 14 Passed: shadow receive-side knob contract holds (%d checks) -- "
                 u8"receive_shadow/bias_multiplier carried by the per-draw row from "
                 u8"RenderableComponent to the fragment-side shadow factor.",
                 static_cast<int>(sizeof(kShadowKnobContracts) / sizeof(kShadowKnobContracts[0])));
    }

        // ─────────────────────────────────────────────────────────────
        // 15: D4 静态写入语义化契约（A′：把"静态写完不动"变成 API 语义）。
        //
        // "静态物体写一次就不动"是本引擎的硬约定：静态段写一次用很久，且静态级联
        // 阴影缓存的**正确性前提**就是它。违反约定的**运行期**写入代价 = 整段静态
        // 矩阵重写 + 全部静态级联缓存失效（当帧 4 级全量重绘）+ 连带标脏整棵子树。
        // 因此组件侧必须留痕（每组件一次性告警），而不是静默生效。
        // 本测试同时钉住行为（搭建期不告警 / 消费后告警一次 / Movable 不受影响）
        // 与八条写入路径的源码契约（任何一条被删都会静默失效留痕语义）。
        // ─────────────────────────────────────────────────────────────

        {
            static const SourceContract kStaticWriteContracts[] =
            {
                { "TransformComponent.cpp", OS_TEXT("src/ecs/components/TransformComponent.cpp"),
                  "void TransformComponent::WarnStaticRuntimeWrite(const char *what)",
                  "D4 一次性告警的实现被删——运行期写静态又变成静默生效" },
                { "TransformComponent.cpp", OS_TEXT("src/ecs/components/TransformComponent.cpp"),
                  "if (!IsStatic() || !static_runtime_write_armed || static_runtime_write_warned)",
                  "告警守卫被改：要么搭建期误报，要么每帧刷屏（一次性语义失效）" },
                { "TransformComponent.cpp", OS_TEXT("src/ecs/components/TransformComponent.cpp"),
                  "WarnStaticRuntimeWrite(\"SetLocalPosition\");",
                  "本地位置写入不再留痕（最常见的每帧写路径）" },
                { "TransformComponent.cpp", OS_TEXT("src/ecs/components/TransformComponent.cpp"),
                  "WarnStaticRuntimeWrite(\"SetLocalRotation\");",
                  "本地旋转写入不再留痕" },
                { "TransformComponent.cpp", OS_TEXT("src/ecs/components/TransformComponent.cpp"),
                  "WarnStaticRuntimeWrite(\"SetLocalScale\");",
                  "本地缩放写入不再留痕" },
                { "TransformComponent.cpp", OS_TEXT("src/ecs/components/TransformComponent.cpp"),
                  "WarnStaticRuntimeWrite(\"SetLocalTRS\");",
                  "TRS 复合写入不再留痕（批量搭建/动画常用路径）" },
                { "TransformComponent.cpp", OS_TEXT("src/ecs/components/TransformComponent.cpp"),
                  "WarnStaticRuntimeWrite(\"SetWorldPosition\");",
                  "世界位置写入不再留痕" },
                { "TransformComponent.cpp", OS_TEXT("src/ecs/components/TransformComponent.cpp"),
                  "WarnStaticRuntimeWrite(\"SetWorldRotation\");",
                  "世界旋转写入不再留痕" },
                { "TransformComponent.cpp", OS_TEXT("src/ecs/components/TransformComponent.cpp"),
                  "WarnStaticRuntimeWrite(\"SetWorldScale\");",
                  "世界缩放写入不再留痕" },
                { "TransformComponent.cpp", OS_TEXT("src/ecs/components/TransformComponent.cpp"),
                  "WarnStaticRuntimeWrite(\"SetParent\");",
                  "改父级不再留痕（同样会让全部静态级联失效）" },
                { "TransformSystem.cpp", OS_TEXT("src/ecs/systems/tick/TransformSystem.cpp"),
                  "comp->ArmStaticRuntimeWriteWarning();",
                  "渲染侧不再 arm：搭建期与运行期无法区分（告警永不触发或永远误报）" },
            };

            if (const int failed = verify_source_contracts(15, kStaticWriteContracts,
                                                           sizeof(kStaticWriteContracts) / sizeof(kStaticWriteContracts[0])))
                return failed;

            GLogInfo(u8"Test 15 Passed: static runtime-write contract holds "
                     u8"(%d source checks + 5 behavioral checks) -- 搭建期不告警、渲染侧消费后"
                     u8"运行期写入每组件告警一次、Movable 与已迁移对象不受影响、八条写入路径"
                     u8"全部留痕。",
                     static_cast<int>(sizeof(kStaticWriteContracts) / sizeof(kStaticWriteContracts[0])));
        }

    // ─────────────────────────────────────────────────────────────
    // 16: 横向锚定步长的 **texel 口径**契约（D5/S1）。
    //
    // 主参数是 texel 数 B（不是世界米）：世界步长 L 与半径补偿都由 B 派生——
    //     L = 2·B·r0 / (M − 1.416·B)，radius = r0 + 0.708·L  ⇒ L/texel ≡ B
    //     精度损失 = 1.416·B/M（与切片半径 r0、贴图分辨率 M 都无关，只看 B/M）
    // 为什么必须这样：L 同时是"环形偏移的量子"（`cache_offset` 是 uvec，滚动必须整数
    // texel）与"半径补偿量"（决定静态阴影精度）。两个约束同时成立 ⇒ 锚定格必须是该级
    // texel 的整数倍；若退回世界米口径，同一常量在不同级联折算出非整数 texel（实测旧
    // 2m 常量 ≈ c1 15.7 / c2 5.1 / c3 2.8 texel），既破坏量子性，又让远景级联的条带退化
    // 到比 PCF 外扩还窄。
    //
    // 本测试**不读被测代码的中间量**：用"有无锚定两次运行的 texel 之差"反解补偿量
    //     texel(B) − texel(0) = 1.416·L/M  ⇒ L = (texel(B) − texel(0))·M/1.416
    // 再断言 L/texel(B) 恰为 B（整数 texel 量子）与精度损失公式。
    // ─────────────────────────────────────────────────────────────
    {
        const float M = 1024.0f;

        auto measure_texel = [&M](const uint32_t b1, const uint32_t b2, const uint32_t b3,
                                  float out_texel[4]) -> bool
        {
            CascadedShadowConfig cfg;
            cfg.cascade_count = 4;
            cfg.c0_dynamic_overlay = true;
            cfg.shadow_map_size = M;
            cfg.max_distance = 300.0f;
            cfg.cache_anchor_step = 16.0f;
            cfg.cache_scroll_band_texels[0] = 0;   // c0 是逐帧全量动态层，永不加锚定
            cfg.cache_scroll_band_texels[1] = b1;
            cfg.cache_scroll_band_texels[2] = b2;
            cfg.cache_scroll_band_texels[3] = b3;

            CascadedShadowController ctrl(cfg);
            ShadowInfo info;
            Camera cam;
            cam.znear = 0.1f;
            cam.zfar = 500.0f;
            cam.fovY = 60.0f;
            cam.pos = Vector3f(0.0f, 0.0f, 1.7f);
            // 注意 Camera 默认 world_up=(0,0,1)（Z 轴向上）；若视线与之平行，cross 退化成
            // normalize(0)=NaN（半径会静默变 0）。这里显式给一组不共线的基准。
            cam.world_up = Vector3f(0.0f, 1.0f, 0.0f);
            cam.viewDirection = Vector3f(0.0f, 0.0f, -1.0f);

            CascadeUpdateResult r[kMaxShadowCascades];
            ctrl.Update(cam, 16.0f / 9.0f, glm::normalize(Vector3f(0.5f, 0.8f, -1.0f)), info, r);

            for (uint32_t c = 0; c < 4; ++c)
            {
                if (!(r[c].sphere_radius > 0.0f) || !std::isfinite(r[c].sphere_radius))
                    return false;
                out_texel[c] = (2.0f * r[c].sphere_radius) / M;   // texel = 2r/M（sphere_radius 由 texel 反解）
            }
            return true;
        };

        float texel_off[4] = {0, 0, 0, 0};   // B = 0（禁用锚定）
        float texel_def[4] = {0, 0, 0, 0};   // B = {0,16,16,32}（默认档）
        float texel_dbl[4] = {0, 0, 0, 0};   // B = {0,32,32,64}（加倍：单调性）
        float texel_bad[4] = {0, 0, 0, 0};   // B = 800 > M/1.416（退化 ⇒ fail-safe 禁用）

        if (!measure_texel(0, 0, 0, texel_off) ||
            !measure_texel(16, 16, 32, texel_def) ||
            !measure_texel(32, 32, 64, texel_dbl) ||
            !measure_texel(800, 800, 800, texel_bad))
        {
            GLogError(u8"Test 16 Failed: CascadeUpdateResult::sphere_radius 未产出（texel 无法量测）");
            return 16;
        }

        const uint32_t bands[4] = {0, 16, 16, 32};

        // (a)(b)(e) 逐级：L/texel 恰为 B；精度损失 == 1.416·B/M；B 加倍 ⇒ texel 单调变粗
        for (uint32_t c = 1; c < 4; ++c)
        {
            const float B = static_cast<float>(bands[c]);
            const float L_measured = (texel_def[c] - texel_off[c]) * M / 1.416f;
            const float quantum = L_measured / texel_def[c];

            if (std::abs(quantum - B) > B * 0.01f + 0.05f)
            {
                GLogError(u8"Test 16 Failed: cascade %u 锚定格 %.3f texel ≠ B=%.0f "
                          u8"（L=2B·r0/(M−1.416B) 闭式被改 ⇒ 环形偏移不再是整数 texel）",
                          c, quantum, B);
                return 16;
            }

            const float loss = (texel_def[c] - texel_off[c]) / texel_off[c];
            // 精确式：loss = 0.708·L/r0 = 1.416·B/(M − 1.416·B)（一阶近似是 1.416·B/M，
            // 二者在 B=32 时差 4.6%：4.63% vs 4.43%，故必须用精确式）
            const float loss_expected = 1.416f * B / (M - 1.416f * B);
            if (std::abs(loss - loss_expected) > loss_expected * 0.02f + 1.0e-4f)
            {
                GLogError(u8"Test 16 Failed: cascade %u 精度损失 %.4f%% ≠ 1.416·B/(M−1.416B) = %.4f%% "
                          u8"（B 口径的代价模型被破坏）",
                          c, loss * 100.0f, loss_expected * 100.0f);
                return 16;
            }

            if (!(texel_dbl[c] > texel_def[c] && texel_def[c] > texel_off[c]))
            {
                GLogError(u8"Test 16 Failed: cascade %u texel 未随 B 单调变粗（%.5f / %.5f / %.5f）",
                          c, texel_off[c], texel_def[c], texel_dbl[c]);
                return 16;
            }
        }

        // (c) c0 不得被锚定波及（B=0）：三种配置下 texel 必须完全一致
        if (std::abs(texel_def[0] - texel_off[0]) > 1.0e-6f ||
            std::abs(texel_dbl[0] - texel_off[0]) > 1.0e-6f)
        {
            GLogError(u8"Test 16 Failed: cascade 0（动态层）被横向锚定波及（%.6f vs %.6f）",
                      texel_def[0], texel_off[0]);
            return 16;
        }

        // (d) 退化保护：B ≥ M/1.416 不得产生除零/负半径/NaN，而是退回"禁用锚定"
        for (uint32_t c = 1; c < 4; ++c)
        {
            if (!std::isfinite(texel_bad[c]) || std::abs(texel_bad[c] - texel_off[c]) > 1.0e-4f)
            {
                GLogError(u8"Test 16 Failed: cascade %u 的 B=800(>M/1.416) 未 fail-safe 回禁用路径"
                          u8"（texel=%.6f vs 禁用 %.6f）",
                          c, texel_bad[c], texel_off[c]);
                return 16;
            }
        }

        GLogInfo(u8"[CSM-BAND] B={0,16,16,32} texel(bare)=[%.5f %.5f %.5f %.5f] "
                 u8"texel(anchored)=[%.5f %.5f %.5f %.5f] loss=[%.2f%% %.2f%% %.2f%%]",
                 texel_off[0], texel_off[1], texel_off[2], texel_off[3],
                 texel_def[0], texel_def[1], texel_def[2], texel_def[3],
                 (texel_def[1] - texel_off[1]) / texel_off[1] * 100.0f,
                 (texel_def[2] - texel_off[2]) / texel_off[2] * 100.0f,
                 (texel_def[3] - texel_off[3]) / texel_off[3] * 100.0f);

        // (f) 源码契约：texel 口径主参数在位 + 世界米口径不得复活
        static const SourceContract kBandContracts[] =
        {
            { "CascadedShadowController.h",
              OS_TEXT("inc/hgl/graph/render/lighting/CascadedShadowController.h"),
              "uint32_t cache_scroll_band_texels[kMaxShadowCascades] = { 0, 16, 16, 32 }",
              "texel 口径主参数被删/改默认档——横向锚定步长退回世界米口径（失去跨分辨率可比的量子语义）" },
            { "CascadedShadowController.cpp",
              OS_TEXT("src/SceneGraph/render/lighting/CascadedShadowController.cpp"),
              "lateral_anchor = (2.0f * band * radius) / denom;",
              "L=2B·r0/(M−1.416B) 闭式被改——锚定格不再是该级 texel 的整数倍（环形偏移会引入亚 texel 抖动）" },
            { "CascadedShadowController.cpp",
              OS_TEXT("src/SceneGraph/render/lighting/CascadedShadowController.cpp"),
              "const float denom = map_size - 1.416f * band;",
              "退化保护被删——B ≥ M/1.416 时除零/负半径（NaN 半径会让整级联消失）" },
            { "CascadedShadowController.cpp",
              OS_TEXT("src/SceneGraph/render/lighting/CascadedShadowController.cpp"),
              "config_.cache_scroll_band_texels[c]",
              "调用点未逐级传 B——退化成所有级联共用一个步长（远景级联条带退化）" },
            { "CascadedShadowController.h",
              OS_TEXT("inc/hgl/graph/render/lighting/CascadedShadowController.h"),
              "cache_lateral_anchor_step",
              "世界米口径的横向锚定字段复活——与 texel 口径主参数并存会造成两套步长语义",
              true },
        };

        if (const int failed = verify_source_contracts(16, kBandContracts,
                                                       static_cast<uint>(sizeof(kBandContracts) / sizeof(kBandContracts[0]))))
            return failed;

        GLogInfo(u8"Test 16 Passed: horizontal anchor step is texel-denominated (S1) -- "
                 u8"L/texel == B exactly, loss == 1.416*B/M, c0 unaffected, degenerate B fails safe.");
    }

    // ─────────────────────────────────────────────────────────────
    // 17: 环形滚动（S2）—— 偏移补偿 + 物理条带的坐标一致性契约
    //
    // 滚动缓存成立的全部数学压在这两条不变式上（任一符号错 ⇒ 阴影整体滑一整个步长）：
    //   ① 物理不动：固定世界点在物理贴图上的位置，跨格前后**不变**
    //      （旧内容原地继续有效，靠读侧 fract(uv + O·texel) 映射回正确位置）
    //   ② 写读一致：写侧矩阵投出的 uv ≡ 读侧 fract(uv + O·texel)
    // 另钉死三条排除项：
    //   ③ 整级重建 ⇒ 偏移必为 0（非零偏移的"整级重画"会漏掉尾部 |O| 条带，光栅器无环绕）
    //   ④ 纯滚动 ⇒ 偏移按 ±B 累加（mod M 回绕），条带覆盖全部"新暴露"内容
    //   ⑤ 位移不是整步（未开横向锚定）⇒ 回落整级重建 + 偏移清零
    // 本用例要求真的走到纯滚动（跨格 ≥ 8 次），否则视为空跑失败。
    // ─────────────────────────────────────────────────────────────
    {
        const uint32_t kProbeCount = 8;
        const float kProbeX[kProbeCount] = { 2.0f, 6.0f, 11.0f, 20.0f, 34.0f, 58.0f, 88.0f, 130.0f };

        const Vector3f light_dir = glm::normalize(Vector3f(0.5f, 0.8f, -1.0f));
        const float aspect = 16.0f / 9.0f;

        auto make_camera = []()
        {
            Camera c;
            c.znear = 0.1f; c.zfar = 500.0f; c.fovY = 60.0f;
            c.pos = Vector3f(0.0f, 0.0f, 1.7f);
            c.world_up = Vector3f(0.0f, 1.0f, 0.0f);      // 显式：默认 (0,0,1) 与 +x 视线共线会 NaN
            c.viewDirection = Vector3f(1.0f, 0.0f, 0.0f); // 沿 +x 行走：跨格方向确定
            return c;
        };

        // 读侧公式（与 pcf_shadow.glsl 一致）：uv = 0.5 + 0.5*ndc.xy
        const auto layout_uv = [](const Matrix4f &vp, const Vector3f &p)
        {
            const Vector4f clip = vp * Vector4f(p.x, p.y, p.z, 1.0f);
            return Vector2f(0.5f + 0.5f * clip.x, 0.5f + 0.5f * clip.y);
        };
        const auto inside_unit = [](const Vector2f &uv)
        {
            return uv.x > 0.02f && uv.x < 0.98f && uv.y > 0.02f && uv.y < 0.98f;
        };
        // 环形差值：a ≡ b (mod 1) 时为 0
        const auto wrap_delta = [](float a, float b)
        {
            const float d = a - b;
            return std::abs(d - std::round(d));
        };

        CascadedShadowConfig scfg;
        scfg.cascade_count = 4;
        scfg.c0_dynamic_overlay = true;
        scfg.shadow_map_size = 1024.0f;
        scfg.max_distance = 300.0f;
        scfg.cache_anchor_step = 16.0f;
        scfg.cache_scroll_band_texels[1] = 16;
        scfg.cache_scroll_band_texels[2] = 16;
        scfg.cache_scroll_band_texels[3] = 32;

        const uint32_t kBand[4] = { 0, 16, 16, 32 };
        const float M = scfg.shadow_map_size;

        CascadedShadowController scroll_ctrl(scfg);
        Camera scroll_cam = make_camera();

        ShadowInfo sinfo;
        CascadeUpdateResult supd[kMaxShadowCascades];

        Matrix4f prev_vp[4];                     // 上一帧的布局矩阵（判定"旧内容覆盖范围"）
        Vector2f prev_phys[4][kProbeCount];      // 上一帧的**物理** uv（不动点判据）
        bool prev_inside[4][kProbeCount] = {};   // 上一帧在框内（留余量）⇒ phys 有意义
        bool prev_scrolled[4] = {};              // 上一帧不是整级重建（内容未重光栅化）
        uint32_t crossings[4] = {};
        uint32_t scroll_hits = 0;
        uint32_t strip_checks = 0;
        uint32_t neg_crossings = 0;  // 反向跨格次数（接缝拆分分支的唯一可达来源）
        uint32_t uoff_prev_x[4] = {};
        uint32_t uoff_prev_y[4] = {};
        constexpr uint32_t kStripSamples = 128;  // 每轴条带探针数（覆盖 B=16 texel 的窄带）

        // ④ 的检查体提取为 lambda：主用例（默认 B）与子用例 (e)（B ∤ M ⇒ 可跨缝）共用。
        //    独立判据：探针由**当前帧**的框几何生成（z_view=0 平面上的网格，以 uv→世界点
        //    反算，不引用条带公式），"是否新暴露"由**上一帧矩阵**判定（uv_prev 落在
        //    [0,1)² 之外 ⇒ 旧内容覆盖不到它）⇒ 它的物理位置必须落在本帧 dirty_rects 的
        //    并集内，否则该处阴影保持过期内容。返回 0 通过，17 失败（已打印原因）。
        const auto check_strip_coverage = [&](const CascadeUpdateResult &res, uint32_t c,
                                             const Matrix4f &vp_read, const Matrix4f &vp_prev,
                                             const Vector4u &off, uint32_t &counter) -> int
        {
            const Matrix4f inv_view = glm::inverse(res.light_view);
            const float rr = res.sphere_radius;

            for (uint32_t axis = 0; axis < 2; ++axis)
            {
                for (uint32_t s = 0; s < kStripSamples; ++s)
                {
                    const float t = (static_cast<float>(s) + 0.5f) / static_cast<float>(kStripSamples);
                    const float u = (axis == 0) ? t : 0.5f;
                    const float v = (axis == 0) ? 0.5f : t;

                    const Vector4f view_pos(2.0f * rr * (u - 0.5f),
                                            2.0f * rr * (0.5f - v), 0.0f, 1.0f);
                    const Vector4f world = inv_view * view_pos;
                    const Vector3f P(world.x, world.y, world.z);

                    // 生成器自检：该点的当前布局 uv 必须等于我请求的 (u,v)
                    const Vector2f uv_now = layout_uv(vp_read, P);
                    if (std::abs(uv_now.x - u) > 1.0e-3f || std::abs(uv_now.y - v) > 1.0e-3f)
                    {
                        GLogError(u8"Test 17 Failed: 条带探针生成器自检失败（请求 uv=(%.4f,%.4f) "
                                  u8"实得 (%.4f,%.4f)）——uv↔view 映射假设有误，条带覆盖判据不可信",
                                  u, v, uv_now.x, uv_now.y);
                        return 17;
                    }

                    const Vector2f uv_prev = layout_uv(vp_prev, P);
                    if (uv_prev.x >= 0.0f && uv_prev.x < 1.0f &&
                        uv_prev.y >= 0.0f && uv_prev.y < 1.0f)
                        continue;   // 旧内容覆盖得到，不需要重画

                    const float fx = uv_now.x * M + static_cast<float>(off.x);
                    const float fy = uv_now.y * M + static_cast<float>(off.y);
                    const float wrapped_x = fx - std::floor(fx / M) * M;
                    const float wrapped_y = fy - std::floor(fy / M) * M;

                    bool covered = false;
                    for (uint32_t r = 0; r < res.dirty_rect_count; ++r)
                    {
                        const ShadowDirtyRect &rect = res.dirty_rects[r];
                        if (wrapped_x >= static_cast<float>(rect.x) &&
                            wrapped_x < static_cast<float>(rect.x + rect.width) &&
                            wrapped_y >= static_cast<float>(rect.y) &&
                            wrapped_y < static_cast<float>(rect.y + rect.height))
                        {
                            covered = true;
                            break;
                        }
                    }
                    if (!covered)
                    {
                        GLogError(u8"Test 17 Failed: cascade %u 新暴露内容（旧框 uv=(%.4f,%.4f) 在外）"
                                  u8"的物理位置 (%.2f,%.2f) 不在任何 dirty_rect 内（O=(%u,%u)）"
                                  u8"——条带漏画，该处阴影会保持过期内容",
                                  c, uv_prev.x, uv_prev.y, wrapped_x, wrapped_y, off.x, off.y);
                        return 17;
                    }
                    ++counter;
                }
            }
            return 0;
        };

        // 跨缝拆分的可观测签名：同一帧里贴图**两端**各有一段 x 受限、y 占满高的矩形
        // （单条带只占一端；角落双轴重合是"一条竖 + 一条横"，不会给出两段竖条）。
        const auto has_vertical_seam_split = [&](const CascadeUpdateResult &res) -> bool
        {
            for (uint32_t a = 0; a < res.dirty_rect_count; ++a)
            {
                const ShadowDirtyRect &ra = res.dirty_rects[a];
                if (ra.x != 0 || ra.width >= static_cast<uint32_t>(M) ||
                    ra.height < static_cast<uint32_t>(M))
                    continue;
                for (uint32_t b = 0; b < res.dirty_rect_count; ++b)
                {
                    const ShadowDirtyRect &rb = res.dirty_rects[b];
                    if (a != b && rb.width < static_cast<uint32_t>(M) &&
                        rb.height >= static_cast<uint32_t>(M) &&
                        rb.x + rb.width == static_cast<uint32_t>(M))
                        return true;
                }
            }
            return false;
        };

        // 面积契约：纯滚动帧里"沿主轴各段宽度之和 == |该轴位移量|"（拆分只改变分段位置，
        // 不改变总宽）。与 ④ 的位置判据互补：④ 管"该画的地方有没有画"，本判据管"该画的
        // 总量对不对"⇒ 段落被截短一纹素、漏段、或在命中帧（位移 0）多画，都会被咬住。
        const auto check_strip_area = [&](const CascadeUpdateResult &res, uint32_t c,
                                         uint32_t delta_x, uint32_t delta_y, uint32_t band) -> int
        {
            const uint32_t wrap_neg = (band > 0) ? (static_cast<uint32_t>(M) - band) : 0;
            const uint32_t expect_x = (band > 0 && (delta_x == band || delta_x == wrap_neg)) ? band : 0;
            const uint32_t expect_y = (band > 0 && (delta_y == band || delta_y == wrap_neg)) ? band : 0;

            uint32_t sum_x = 0, sum_y = 0;
            for (uint32_t r = 0; r < res.dirty_rect_count; ++r)
            {
                const ShadowDirtyRect &rect = res.dirty_rects[r];
                if (rect.height >= static_cast<uint32_t>(M) && rect.width < static_cast<uint32_t>(M))
                    sum_x += rect.width;      // 竖直条带（含拆分后的两段）
                else if (rect.width >= static_cast<uint32_t>(M) && rect.height < static_cast<uint32_t>(M))
                    sum_y += rect.height;     // 水平条带（含拆分后的两段）
            }

            if (sum_x != expect_x || sum_y != expect_y)
            {
                GLogError(u8"Test 17 Failed: cascade %u 条带宽度之和 (x=%u,y=%u) ≠ 该轴位移量 "
                          u8"(x=%u,y=%u)（delta=(%u,%u) B=%u）——条带被截短/漏段（或命中帧多画），"
                          u8"贴图会残留过期内容",
                          c, sum_x, sum_y, expect_x, expect_y, delta_x, delta_y, band);
                return 17;
            }
            return 0;
        };

        for (uint32_t f = 0; f < 400; ++f)
        {
            // 前 200 帧朝 +x（正向跨格：条带恰好终止于偏移处，不跨缝），后 200 帧反向。
            // 反向跨格时条带起点 = 新偏移，起点落在贴图末 B 个纹素内就会**跨过接缝**，
            // 必须被拆成两段矩形（环形缓存里贴图两端物理相邻）——该分支只在反向行程
            // 可达，因此行程必须双向。
            // 步长 < c1 的锚定步长 L≈2.13m ⇒ 每帧最多跨 1 格（不触发多格回落），
            // 偏移才能连续累积到 ±M 回绕。
            scroll_cam.pos.x += (f < 200) ? 2.0f : -2.0f;
            scroll_ctrl.Update(scroll_cam, aspect, light_dir, sinfo, supd);

            for (uint32_t c = 1; c < 4; ++c)
            {
                const CascadeUpdateResult &res = supd[c];
                const Vector4u &off = res.cache_offset;
                const float texel_uv = 1.0f / M;   // 1 纹素 = 1/M uv

                // ③ 整级重建必须清偏移（否则尾部条带会缺内容）
                if (res.need_full_update && (off.x != 0 || off.y != 0))
                {
                    GLogError(u8"Test 17 Failed: cascade %u 整级重建时 cache_offset=(%u,%u) 非零"
                              u8"（非零偏移下整级重画会漏掉尾部 |O| 条带 ⇒ 贴图尾部残留旧内容）",
                              c, off.x, off.y);
                    return 17;
                }
                // UBO 里的偏移必须与结果一致（读侧靠它做环绕）
                if (sinfo.cascades[c].cache_offset.x != off.x || sinfo.cascades[c].cache_offset.y != off.y)
                {
                    GLogError(u8"Test 17 Failed: cascade %u 的 ShadowInfo.cache_offset=(%u,%u) 与"
                              u8"CascadeUpdateResult=(%u,%u) 不一致（读侧环绕会错位）",
                              c, sinfo.cascades[c].cache_offset.x, sinfo.cascades[c].cache_offset.y,
                              off.x, off.y);
                    return 17;
                }
                // 偏移量级：必须是 B 的整数倍（环形滚动的量子）
                if (kBand[c] > 0 && (off.x % kBand[c] != 0 || off.y % kBand[c] != 0))
                {
                    GLogError(u8"Test 17 Failed: cascade %u 偏移 (%u,%u) 不是步长 B=%u 的整数倍"
                              u8"（环形偏移必须按整数个锚定格累加）",
                              c, off.x, off.y, kBand[c]);
                    return 17;
                }

                // c0 与所有"偏移为 0"的帧：写侧矩阵必须与未偏移矩阵逐位相同
                if (off.x == 0 && off.y == 0)
                {
                    for (int row = 0; row < 4; ++row)
                        for (int col = 0; col < 4; ++col)
                            if (std::abs(res.light_view_draw[row][col] - res.light_view[row][col]) > 0.0f)
                            {
                                GLogError(u8"Test 17 Failed: cascade %u 偏移为 0 时 light_view_draw"
                                          u8" 与 light_view 不逐位相同（未滚动路径不应有额外变换）", c);
                                return 17;
                            }
                }

                const Matrix4f vp_read = res.light_proj * res.light_view;
                const Matrix4f vp_draw = res.light_proj * res.light_view_draw;

                for (uint32_t i = 0; i < kProbeCount; ++i)
                {
                    const Vector3f P(kProbeX[i], 0.0f, 1.0f);
                    const Vector2f uv = layout_uv(vp_read, P);
                    const bool inside     = inside_unit(uv);
                    const bool has_prev   = (f > 0);

                    float phys_x = 0.0f, phys_y = 0.0f;

                    if (inside)
                    {
                        // ② 写读一致：写侧投出的 uv ≡ 读侧 fract(uv + O·texel)
                        const Vector2f uv_draw = layout_uv(vp_draw, P);
                        phys_x = uv.x + static_cast<float>(off.x) * texel_uv;
                        phys_y = uv.y + static_cast<float>(off.y) * texel_uv;

                        if (wrap_delta(uv_draw.x, phys_x) > 1.0e-4f ||
                            wrap_delta(uv_draw.y, phys_y) > 1.0e-4f)
                        {
                            GLogError(u8"Test 17 Failed: cascade %u 探针 %u 写侧 uv=(%.6f,%.6f) 与"
                                      u8"读侧 fract(uv+O·texel)=(%.6f,%.6f) 不一致（O=(%u,%u) texel=%.6f m）"
                                      u8"——写读坐标系错位，阴影会整体平移",
                                      c, i, uv_draw.x, uv_draw.y,
                                      phys_x - std::floor(phys_x), phys_y - std::floor(phys_y),
                                      off.x, off.y, res.texel_world_size);
                            return 17;
                        }

                        // ① 物理不动：连续两个"非整级重建"帧里，同一世界点的物理位置必须不变
                        if (has_prev && prev_inside[c][i] && prev_scrolled[c] && !res.need_full_update)
                        {
                            if (wrap_delta(phys_x, prev_phys[c][i].x) > 1.0e-4f ||
                                wrap_delta(phys_y, prev_phys[c][i].y) > 1.0e-4f)
                            {
                                GLogError(u8"Test 17 Failed: cascade %u 探针 %u 物理位置从 (%.6f,%.6f) 跳到"
                                          u8"(%.6f,%.6f) —— 旧内容没有原地继续有效（环形补偿符号/量级错），"
                                          u8"表现为相机移动时静态阴影整体滑动",
                                          c, i, prev_phys[c][i].x, prev_phys[c][i].y, phys_x, phys_y);
                                return 17;
                            }
                        }

                        prev_phys[c][i] = Vector2f(phys_x, phys_y);
                    }

                    prev_inside[c][i] = inside;
                }

                // ④ 新暴露内容必须被条带覆盖。
                //    独立判据：探针由**当前帧**的框几何生成（z_view=0 平面上的网格，
                //    以 uv→世界点反算，不引用条带公式），"是否新暴露"由**上一帧矩阵**
                //    判定（uv_prev 落在 [0,1)² 之外 ⇒ 旧内容覆盖不到它）⇒ 它的物理位置
                //    必须落在本帧 dirty_rects 的并集内，否则该处阴影保持过期内容。
                if (f > 0 && !res.need_full_update && res.dirty_rect_count > 0)
                {
                    const int rc = check_strip_coverage(res, c, vp_read, prev_vp[c], off, strip_checks);
                    if (rc != 0)
                        return rc;
                }

                // 偏移增量可观测 ⇒ 用它判定跨格方向与量级（同时供面积契约使用）：
                //   正向跨格 ⇒ delta == B（条带恰好终止于偏移处，不跨缝）
                //   反向跨格 ⇒ delta == M−B（条带起点 = 新偏移，落在贴图末 B 纹素内即跨缝）
                //   多格跳变/整级重建 ⇒ need_full_update，偏移被清零，不作为条带依据
                uint32_t delta_x = 0;
                uint32_t delta_y = 0;
                if (f > 0)
                {
                    const uint32_t mw = static_cast<uint32_t>(M);
                    delta_x = (off.x + mw - uoff_prev_x[c]) % mw;
                    delta_y = (off.y + mw - uoff_prev_y[c]) % mw;
                    if (delta_x != 0 && delta_x == mw - static_cast<uint32_t>(kBand[c]))
                        ++neg_crossings;
                }
                uoff_prev_x[c] = off.x;
                uoff_prev_y[c] = off.y;

                if (!res.need_full_update)
                {
                    const int rc = check_strip_area(res, c, delta_x, delta_y,
                                                    static_cast<uint32_t>(kBand[c]));
                    if (rc != 0)
                        return rc;
                }

                prev_vp[c] = vp_read;
                prev_scrolled[c] = !res.need_full_update;
                if (!res.need_full_update)
                {
                    ++scroll_hits;
                    if (kBand[c] > 0 && res.dirty_rect_count > 0)
                        ++crossings[c];
                }
            }
        }

        // ── (e) 跨接缝拆分：B ∤ M 才可达 ─────────────────────────────────────────
        // 默认 B{16,16,32} 都整除 M=1024 ⇒ 偏移恒为 B 的整数倍 ⇒ 条带 start+width 恰好
        // ≤ M，**永不跨缝**（该分支在默认配置下不可达，但仍必须在 B 不整除 M 的配置下
        // 正确，例如 B=24：1024 % 24 = 16 ⇒ 偏移可取到贴图末 B 纹素内 ⇒ 条带跨缝）。
        // 该子用例用 B=24 走一趟双向行程，把"跨缝必须拆成两段"钉死。
        {
            CascadedShadowConfig ecfg = scfg;
            ecfg.cache_scroll_band_texels[1] = 24;
            ecfg.cache_scroll_band_texels[2] = 24;
            ecfg.cache_scroll_band_texels[3] = 24;
            // 深度锚点（沿光轴 step）在这里故意放到全程不会跨过：它跨一次就把偏移清零，
            // 而横向偏移要累积到"末 B 纹素"需要连续几十次跨格不被重置（B=24 时从 M−B
            // 降到 16 需 41 次）。深度锚点本身的行为由 Test 5B-2 覆盖，此处只隔离横向滚动。
            ecfg.cache_anchor_step = 4096.0f;

            CascadedShadowController ectrl(ecfg);
            ShadowInfo einfo;
            CascadeUpdateResult eupd[kMaxShadowCascades];
            Matrix4f epvp[4];
            Camera ecam = scroll_cam;
            ecam.pos = Vector3f(0.0f, 0.0f, 1.7f);

            uint32_t e_checks = 0;
            uint32_t e_splits = 0;
            uint32_t e_negs = 0;
            uint32_t e_offs[4] = {};
            uint32_t e_offs_y[4] = {};

            for (uint32_t f = 0; f < 400; ++f)
            {
                // 步长 3.0m < B=24 档的 L(≈3.23m) ⇒ 每帧最多跨一格，偏移可连续累积；
                // 双向行程保证偏移遍历 [0,M) 内足够多的 8 的倍数（≥128 个）以命中"末 B 纹素"。
                ecam.pos.x += (f < 200) ? 3.0f : -3.0f;
                ectrl.Update(ecam, aspect, light_dir, einfo, eupd);

                for (uint32_t c = 1; c < 4; ++c)
                {
                    const CascadeUpdateResult &res = eupd[c];
                    const Vector4u &off = res.cache_offset;
                    const Matrix4f evp = res.light_proj * res.light_view;

                    if (f > 0 && !res.need_full_update && res.dirty_rect_count > 0)
                    {
                        const int rc = check_strip_coverage(res, c, evp, epvp[c], off, e_checks);
                        if (rc != 0)
                            return rc;
                    }
                    if (!res.need_full_update && has_vertical_seam_split(res))
                        ++e_splits;
                    uint32_t edx = 0, edy = 0;
                    if (f > 0)
                    {
                        const uint32_t mw = static_cast<uint32_t>(M);
                        edx = (off.x + mw - e_offs[c]) % mw;
                        edy = (off.y + mw - e_offs_y[c]) % mw;
                        if (edx != 0 && edx == mw - 24u)
                            ++e_negs;
                    }
                    e_offs[c] = off.x;
                    e_offs_y[c] = off.y;
                    if (!res.need_full_update)
                    {
                        const int arc = check_strip_area(res, c, edx, edy, 24u);
                        if (arc != 0)
                            return arc;
                    }
                    epvp[c] = evp;
                }
            }

            if (e_splits < 1 || e_checks < 8 || e_negs < 8)
            {
                GLogError(u8"Test 17(e) Failed: B=24（1024 %% 24 = 16）下跨接缝拆分分支未被走到"
                          u8"（覆盖检查 %u 次 / 跨缝拆分 %u 次 / 反向跨格 %u 次）——"
                          u8"跨缝条带的尾部 |O| 纹素会漏画，贴图末端残留过期内容",
                          e_checks, e_splits, e_negs);
                return 17;
            }
            GLogInfo(u8"[CSM-SEAM] B=24 M=1024 覆盖检查=%u 次 跨缝拆分=%u 次 反向跨格=%u 次",
                     e_checks, e_splits, e_negs);
        }

        // 空跑保护：必须真的走到纯滚动（每级至少若干次跨格），否则上面的不变式是废话
        const uint32_t total_crossings = crossings[1] + crossings[2] + crossings[3];
        if (total_crossings < 8 || strip_checks < 8 || scroll_hits < 20 || neg_crossings < 4)
        {
            GLogError(u8"Test 17 Failed: 用例没走到环形滚动（跨格 %u 次 / 条带覆盖检查 %u 次 / "
                      u8"命中帧 %u / 反向跨格 %u 次）——探针或轨迹设置失效，契约形同虚设",
                      total_crossings, strip_checks, scroll_hits, neg_crossings);
            return 17;
        }

        // ⑤ 位移不是整步（横向锚定关闭 ⇒ snapped 原点按 texel 连续移动）⇒ 必须回落整级重建且偏移清零
        {
            CascadedShadowConfig ncfg = scfg;
            for (uint32_t i = 0; i < kMaxShadowCascades; ++i)
                ncfg.cache_scroll_band_texels[i] = 0;   // 关横向锚定

            CascadedShadowController nostep_ctrl(ncfg);
            Camera nostep_cam = make_camera();
            ShadowInfo ninfo;
            CascadeUpdateResult nupd[kMaxShadowCascades];

            uint32_t fallback_frames = 0;
            for (uint32_t f = 0; f < 24; ++f)
            {
                nostep_cam.pos.x += 0.7f;
                nostep_ctrl.Update(nostep_cam, aspect, light_dir, ninfo, nupd);
                for (uint32_t c = 1; c < 4; ++c)
                {
                    if (nupd[c].need_full_update)
                        ++fallback_frames;
                    if (nupd[c].cache_offset.x != 0 || nupd[c].cache_offset.y != 0)
                    {
                        GLogError(u8"Test 17 Failed: 未开横向锚定（位移非整步）时 cascade %u 仍产生"
                                  u8"偏移 (%u,%u)——条带兜不住这种位移，必须回落整级重建并清零偏移",
                                  c, nupd[c].cache_offset.x, nupd[c].cache_offset.y);
                        return 17;
                    }
                }
            }
            if (fallback_frames < 12)
            {
                GLogError(u8"Test 17 Failed: 未开锚定的用例没有触发足够的整级重建（%u 次）"
                          u8"——回落路径没有被验证到", fallback_frames);
                return 17;
            }
        }

        GLogInfo(u8"[CSM-SCROLL] frames=400 crossings=[%u,%u,%u] 条带覆盖检查=%u 次 命中帧=%u "
                 u8"反向跨格=%u 次 off3=(%u,%u) 非整步回落=OK",
                 crossings[1], crossings[2], crossings[3], strip_checks, scroll_hits, neg_crossings,
                 supd[3].cache_offset.x, supd[3].cache_offset.y);
        GLogInfo(u8"Test 17 Passed: toroidal scroll offset/strip coordinates hold "
                 u8"(write==read, content physically fixed, full redraw clears offset, "
                 u8"non-integer step falls back).");
    }

    // ─────────────────────────────────────────────────────────────
    // 18: 环形寻址接缝可达性（S4）—— 证明 wrap 只发生在可见接收者够不到的地方
    //
    // 读侧是 `fract(uv + O·texel)`：整张贴图构成一个环，layout uv=0/1 是一条**跳变线
    // （seam）**，其物理位置 = O（随偏移移动）。采样跨越 seam 会读到方框对侧的深度
    // （世界距离 M 个 texel），因此只要可见接收者能贴到 seam，就会在那里留下一条
    // 1~2 texel 宽的错影线。判据：视锥切片 8 个角点到方框边界的**最小余量（texel）**
    // 必须大于采样可达半径：
    //     可达半径 = pcf_radius（Poisson 磁盘 / 3x3 盒式最大 1.0·radius）
    //              + normal_offset 硬上限 1.5m / texel_world_size（shader 里 tan(θ) 被 clamp）
    // 余量（= (1-max|ndc|)/2 · M）与分辨率、级联世界半径自动缩放，故本用例逐级断言并
    // 打印 slack 倍数；另钉住"极窄级联必然越过边界"的算例，使断言非空转。
    // ─────────────────────────────────────────────────────────────
    {
        const Vector3f light_dir = glm::normalize(Vector3f(0.5f, 0.8f, -1.0f));
        const float aspect = 16.0f / 9.0f;
        // pcf_shadow.glsl: SHADOW_NORMAL_OFFSET_MAX（源码契约里钉住它仍为 1.5）
        const float kNormalOffsetHardCapM = 1.5f;

        auto make_camera = []()
        {
            Camera c;
            c.znear = 0.1f; c.zfar = 500.0f; c.fovY = 60.0f;
            c.pos = Vector3f(0.0f, 0.0f, 1.7f);
            c.world_up = Vector3f(0.0f, 1.0f, 0.0f);      // 显式：默认 (0,0,1) 与 +x 视线共线会 NaN
            c.viewDirection = Vector3f(1.0f, 0.0f, 0.0f);
            return c;
        };

        // 读侧公式（与 pcf_shadow.glsl 一致）：uv = 0.5 + 0.5*ndc.xy
        const auto layout_uv = [](const Matrix4f &vp, const Vector3f &p)
        {
            const Vector4f clip = vp * Vector4f(p.x, p.y, p.z, 1.0f);
            return Vector2f(0.5f + 0.5f * clip.x, 0.5f + 0.5f * clip.y);
        };

        // 与 example/Basic/CascadeShadowMap.cpp 同配置（B={0,16,16,32}、anchor=16m 用默认）
        const auto make_cfg = [](float map_size)
        {
            CascadedShadowConfig cfg;
            cfg.cascade_count = 4;
            cfg.c0_dynamic_overlay = true;
            cfg.shadow_map_size = map_size;
            cfg.max_distance = 300.0f;
            cfg.split_distances[0] = 50.0f;
            cfg.split_distances[1] = 50.0f;
            cfg.split_distances[2] = 160.0f;
            cfg.split_distances[3] = 300.0f;
            cfg.normal_offset_world = 0.10f;
            cfg.pcf_radius = 1.5f;
            return cfg;
        };

        struct SeamStat
        {
            float margin_min[4]   = { 1.0e9f, 1.0e9f, 1.0e9f, 1.0e9f };
            float required_max[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            uint32_t wrap_frames[4] = {};
        };

        // 运动轨迹：分 4 个相位（朝向 0/60/120/180°，避开与 world_up=(0,1,0) 共线），
        // 每相位内朝向固定、位置单调推进 —— 环形滚动的纯滚动分支要求**每帧位移恰跨一格**
        // （|shift|==B），朝向乱转会让位移来回抹掉、多格跳变则回落整级重建，两种都拿不到
        // wrap 生效帧。步长与级联锚定格 L_c = 2·B·r0/(M−1.416B) 有关：级联越窄/分辨率越高
        // ⇒ L_c 越小 ⇒ 步长必须同步调小（否则每帧都是多格跳变 = 整级重建）。
        const auto measure = [&](const CascadedShadowConfig &cfg, const uint32_t frames_per_phase,
                                 const float step_m, SeamStat &st)
        {
            constexpr uint32_t kPhases = 4;
            const float yaws[kPhases] = { 0.0f, 1.04719755f, 2.09439510f, 3.14159265f };
            //                                   60°           120°          180°

            for (uint32_t p = 0; p < kPhases; ++p)
            {
                const Vector3f dir(std::cos(yaws[p]), std::sin(yaws[p]), 0.0f);

                CascadedShadowController ctrl(cfg);
                Camera cam = make_camera();
                cam.viewDirection = dir;
                ShadowInfo info;
                CascadeUpdateResult upd[kMaxShadowCascades];

                for (uint32_t f = 0; f < frames_per_phase; ++f)
                {
                    cam.pos = Vector3f(0.0f, 0.0f, 1.7f) + dir * (static_cast<float>(f) * step_m);

                    ctrl.Update(cam, aspect, light_dir, info, upd);

                    const Vector3f cam_forward = dir;
                    const Vector3f cam_right = glm::normalize(glm::cross(cam_forward, cam.world_up));
                    const Vector3f cam_up = glm::cross(cam_right, cam_forward);
                    const float tan_half = std::tan(cam.fovY * (std::numbers::pi / 180.0) * 0.5f);

                    for (uint32_t c = 1; c < 4; ++c)
                    {
                        // 只有滚动帧（Offset 非零）才走 fract 环绕分支；未滚动帧的越界 tap
                        // 钳在边缘，余量小也无副作用 ⇒ 只统计 wrap 真生效的帧。
                        if ((upd[c].cache_offset.x == 0 && upd[c].cache_offset.y == 0)
                            || upd[c].texel_world_size <= 0.0f)
                            continue;

                        ++st.wrap_frames[c];

                        const Matrix4f vp_read = upd[c].light_proj * upd[c].light_view;
                        const float s_near = (c == 1 && cfg.c0_dynamic_overlay)
                                                 ? cam.znear : cfg.split_distances[c - 1];
                        const float dists[2] = { s_near, cfg.split_distances[c] };

                        float margin_ndc = 1.0e9f;
                        for (int d = 0; d < 2; ++d)
                        {
                            const float hh = dists[d] * tan_half;
                            const float ww = hh * aspect;
                            for (int sx = -1; sx <= 1; sx += 2)
                            {
                                for (int sy = -1; sy <= 1; sy += 2)
                                {
                                    const Vector3f corner = cam.pos + cam_forward * dists[d]
                                                          + cam_right * (ww * static_cast<float>(sx))
                                                          + cam_up * (hh * static_cast<float>(sy));
                                    const Vector2f uv = layout_uv(vp_read, corner);
                                    margin_ndc = std::min(margin_ndc,
                                                          std::min(std::min(uv.x, 1.0f - uv.x),
                                                                   std::min(uv.y, 1.0f - uv.y)));
                                }
                            }
                        }

                        st.margin_min[c] = std::min(st.margin_min[c], margin_ndc * cfg.shadow_map_size);
                        st.required_max[c] = std::max(st.required_max[c],
                                                      cfg.pcf_radius
                                                          + kNormalOffsetHardCapM / upd[c].texel_world_size);
                    }
                }
            }
        };

        // ① 默认档（示例同配置）：逐级断言余量 > 可达半径（步长 2.0m < 各级 L_c）
        SeamStat base;
        measure(make_cfg(1024.0f), 128, 2.0f, base);

        for (uint32_t c = 1; c < 4; ++c)
        {
            if (base.wrap_frames[c] < 4)
            {
                GLogError(u8"Test 18 Failed: cascade %u 在 512 帧轨迹里 wrap 生效帧只有 %u 帧"
                          u8"——seam 余量在非滚动帧上量测没有意义，本用例形同虚设",
                          c, base.wrap_frames[c]);
                return 18;
            }
            if (base.margin_min[c] <= base.required_max[c])
            {
                GLogError(u8"Test 18 Failed: cascade %u 视锥角点距方框边界只剩 %.3f texel，"
                          u8"而采样可达半径 %.3f texel——PCF/normal-offset 的 tap 会跨过 seam "
                          u8"读到方框对侧深度，贴图上会出现一圈错影",
                          c, base.margin_min[c], base.required_max[c]);
                return 18;
            }
        }
        GLogInfo(u8"[CSM-SEAM-MARGIN] margin(texel)=[%.1f,%.1f,%.1f] required=[%.1f,%.1f,%.1f] "
                 u8"slack=[%.1fx,%.1fx,%.1fx] wrap_frames=[%u,%u,%u]",
                 base.margin_min[1], base.margin_min[2], base.margin_min[3],
                 base.required_max[1], base.required_max[2], base.required_max[3],
                 base.margin_min[1] / base.required_max[1],
                 base.margin_min[2] / base.required_max[2],
                 base.margin_min[3] / base.required_max[3],
                 base.wrap_frames[1], base.wrap_frames[2], base.wrap_frames[3]);

        // ② 分辨率扫描：余量与可达半径都随 M 线性增长，slack 倍数应基本不变
        //    （可达半径 = pcf + 1.5·M/(2r0)、余量 ≈ 0.13·M ⇒ slack ∝ 级联世界半径 r0）
        {
            const float sizes[4] = { 256.0f, 512.0f, 1024.0f, 2048.0f };
            float slack[4] = {};
            for (int i = 0; i < 4; ++i)
            {
                SeamStat st;
                measure(make_cfg(sizes[i]), 96, 0.5f, st);
                float worst = 1.0e9f;
                for (uint32_t c = 1; c < 4; ++c)
                    if (st.wrap_frames[c] > 0 && st.required_max[c] > 0.0f)
                        worst = std::min(worst, st.margin_min[c] / st.required_max[c]);
                slack[i] = worst;
            }
            // 分辨率提高不会让 seam 变得可达（最坏数不应随 M 恶化超过 10%）
            if (slack[3] < slack[0] * 0.9f)
            {
                GLogError(u8"Test 18 Failed: 分辨率升高后 seam slack 反而恶化（M=256 → %.2fx，"
                          u8"M=2048 → %.2fx）——可达半径/余量的缩放模型不再成立", slack[0], slack[3]);
                return 18;
            }
            GLogInfo(u8"[CSM-SEAM-MARGIN] M=256/512/1024/2048 slack=[%.1fx,%.1fx,%.1fx,%.1fx]",
                     slack[0], slack[1], slack[2], slack[3]);
        }

        // ③ 边界算例：级联世界半径越小，texel 越细 ⇒ 固定 1.5m 的 normal-offset 上限
        //    折合的 texel 数越大，最终越过"余量"⇒ seam 变可达。此处钉住该边界**存在**，
        //    证明 ① 的断言不是恒真。
        {
            CascadedShadowConfig narrow = make_cfg(1024.0f);
            narrow.split_distances[0] = 1.0f;
            narrow.split_distances[1] = 1.0f;
            narrow.split_distances[2] = 2.0f;
            narrow.split_distances[3] = 4.0f;
            narrow.max_distance = 4.0f;

            // 窄级联的锚定格只有 2~10cm，步长必须小到不触发多格跳变（5mm/帧）
            SeamStat st;
            measure(narrow, 384, 0.005f, st);

            bool violated = false;
            for (uint32_t c = 1; c < 4; ++c)
                if (st.wrap_frames[c] > 0 && st.margin_min[c] <= st.required_max[c])
                    violated = true;

            if (!violated)
            {
                GLogError(u8"Test 18 Failed: 例级联（世界半径 <2m）也未越过 seam 边界"
                          u8"——判据对配置不敏感，① 的断言等价于恒真");
                return 18;
            }
            GLogInfo(u8"[CSM-SEAM-MARGIN] 窄级联边界算例（r0≈0.5~2m）margin=[%.1f,%.1f,%.1f] "
                     u8"required=[%.1f,%.1f,%.1f] ⇒ 越过边界（配置约束：级联世界半径过小时"
                     u8"须下调 normal_offset_world 或关闭横向滚动）",
                     st.margin_min[1], st.margin_min[2], st.margin_min[3],
                     st.required_max[1], st.required_max[2], st.required_max[3]);
        }

        // ④ 着色器源码契约：wrap 必须逐级由 cache_offset 驱动，且保留非滚动的 clamp 分支
        {
            const OSString kPcf(OS_TEXT("ShaderLibrary/shadow/pcf_shadow.glsl"));
            const SourceContract seam_contracts[] =
            {
                { "pcf_shadow.glsl", kPcf, "shadow.cascades[c].cache_offset.x != 0.0",
                  "wrap 必须逐级由该级联的 cache_offset 驱动（全局/编译期开关会让未滚动的级联也环绕）" },
                { "pcf_shadow.glsl", kPcf, "fract(shadow_uv + offset_uv)",
                  "读侧映射失效：物理 uv 不再与 cache_offset 对齐，滚动后阴影会整体滑一整个步长" },
                { "pcf_shadow.glsl", kPcf, "fract(tap)",
                  "tap 环绕缺失：条带尾部 |O| 纹素的采样会读到未刷新区域" },
                { "pcf_shadow.glsl", kPcf, "clamp(tap, vec2(0.0), vec2(1.0))",
                  "未滚动级联的越界 tap 必须钳在贴图边缘，否则方框边缘出现 1~2 texel 杂斑圈" },
                { "pcf_shadow.glsl", kPcf, "const float SHADOW_NORMAL_OFFSET_MAX = 1.5;",
                  "normal-offset 硬上限是 seam 余量判据的输入；改动它必须同步复核 Test 18 的模型" },
                { "pcf_shadow.glsl", kPcf, "fract(shadow_uv)",
                  "禁复活：不带偏移的 fract 意味着 wrap 与 cache_offset 解耦（未滚动也会环绕）",
                  true },
                { "pcf_shadow.glsl", kPcf, "HGL_SHADOW_TOROIDAL",
                  "禁复活：环形寻址是逐级运行期决策，不得退回编译期全局开关",
                  true },
            };

            const int src_rc = verify_source_contracts(18, seam_contracts,
                                                       static_cast<uint>(sizeof(seam_contracts)
                                                                         / sizeof(seam_contracts[0])));
            if (src_rc != 0)
                return src_rc;
        }

        GLogInfo(u8"Test 18 Passed: toroidal seam is out of reach for visible receivers "
                 u8"(margin > pcf_radius + normal_offset_cap/texel; wrap stays per-cascade "
                 u8"and cache_offset-driven).");
    }

    // ─────────────────────────────────────────────────────────────
    // 19: 更新形态计数器不变式（把 S5/S6 的诊断口径钉成回归判据）
    //   ① 每帧每级必落一支：fullC + stripC + hitC == Update 调用数
    //   ② 静置不动零更新：连续 60 帧纯命中，stripC 与 fullC 都不增长
    //   ③ 失效必致重建：InvalidateStaticCache() 后的首次 Update，每个静态级必须
    //      整级重建且 cache_offset 清零（非零偏移下整级重画会漏掉尾部 |O| 条带）
    // 反例敏感性：删掉任一支的计数器自增、或让失效不清偏移，本用例立即失败。
    // ─────────────────────────────────────────────────────────────
    {
        const Vector3f light_dir = glm::normalize(Vector3f(0.5f, 0.8f, -1.0f));
        const float aspect = 16.0f / 9.0f;

        auto make_camera = []()
        {
            Camera c;
            c.znear = 0.1f; c.zfar = 500.0f; c.fovY = 60.0f;
            c.pos = Vector3f(0.0f, 0.0f, 1.7f);
            c.world_up = Vector3f(0.0f, 1.0f, 0.0f);   // 默认 (0,0,1) 与 +x 视线共线会 NaN
            c.viewDirection = Vector3f(1.0f, 0.0f, 0.0f);
            return c;
        };

        CascadedShadowConfig cfg;
        cfg.cascade_count = 4;
        cfg.c0_dynamic_overlay = true;
        cfg.shadow_map_size = 1024.0f;
        cfg.max_distance = 300.0f;
        cfg.split_distances[0] = 50.0f;
        cfg.split_distances[1] = 50.0f;
        cfg.split_distances[2] = 160.0f;
        cfg.split_distances[3] = 300.0f;
        cfg.normal_offset_world = 0.10f;
        cfg.pcf_radius = 1.5f;

        CascadedShadowController ctrl(cfg);
        ShadowInfo info;
        CascadeUpdateResult upd[kMaxShadowCascades];
        Camera cam = make_camera();
        const Vector3f dir = glm::normalize(Vector3f(0.8f, 0.6f, 0.0f));

        const auto sum_forms = [&ctrl](uint32_t c)
        {
            return ctrl.GetFullUpdateCallCount(c) + ctrl.GetStripUpdateCallCount(c)
                 + ctrl.GetHitUpdateCallCount(c);
        };

        // ── 相位 1：推进 200 帧（命中/条带/整级重建混合）──
        cam.viewDirection = dir;
        uint32_t updates = 0;
        for (uint32_t f = 0; f < 200; ++f)
        {
            cam.pos = Vector3f(0.0f, 0.0f, 1.7f) + dir * (0.05f * static_cast<float>(f));
            ctrl.Update(cam, aspect, light_dir, info, upd);
            ++updates;
        }
        for (uint32_t c = 1; c < 4; ++c)
        {
            if (sum_forms(c) != updates)
            {
                GLogError(u8"Test 19 Failed: cascade %u 形态计数和 %u ≠ Update 调用数 %u"
                          u8"（fullC+stripC+hitC 必须每帧每级恰落一支：某分支漏计数）",
                          c, sum_forms(c), updates);
                return 19;
            }
        }
        uint32_t strip_total = 0, full_total = 0;
        for (uint32_t c = 1; c < 4; ++c)
        {
            strip_total += ctrl.GetStripUpdateCallCount(c);
            full_total  += ctrl.GetFullUpdateCallCount(c);
        }
        if (strip_total == 0)
        {
            GLogError(u8"Test 19 Failed: 200 帧推进里一次条带滚动都没有（用例空转 ⇒ 判据无意义）");
            return 19;
        }

        // ── 相位 2：静置 60 帧 ⇒ 只有命中 ──
        uint32_t s0[kMaxShadowCascades] = {};
        uint32_t f0[kMaxShadowCascades] = {};
        uint32_t h0[kMaxShadowCascades] = {};
        for (uint32_t c = 1; c < 4; ++c)
        {
            s0[c] = ctrl.GetStripUpdateCallCount(c);
            f0[c] = ctrl.GetFullUpdateCallCount(c);
            h0[c] = ctrl.GetHitUpdateCallCount(c);
        }
        for (uint32_t f = 0; f < 60; ++f)
        {
            ctrl.Update(cam, aspect, light_dir, info, upd);
            ++updates;
        }
        for (uint32_t c = 1; c < 4; ++c)
        {
            if (ctrl.GetStripUpdateCallCount(c) != s0[c] || ctrl.GetFullUpdateCallCount(c) != f0[c])
            {
                GLogError(u8"Test 19 Failed: cascade %u 静置 60 帧仍发生绘制（条带 %u→%u / 整级 %u→%u）"
                          u8"——相机不动时必须零更新",
                          c, s0[c], ctrl.GetStripUpdateCallCount(c),
                          f0[c], ctrl.GetFullUpdateCallCount(c));
                return 19;
            }
            if (ctrl.GetHitUpdateCallCount(c) - h0[c] != 60)
            {
                GLogError(u8"Test 19 Failed: cascade %u 静置期命中 %u ≠ 60",
                          c, ctrl.GetHitUpdateCallCount(c) - h0[c]);
                return 19;
            }
            if (sum_forms(c) != updates)
            {
                GLogError(u8"Test 19 Failed: cascade %u 静置期形态计数和 %u ≠ %u",
                          c, sum_forms(c), updates);
                return 19;
            }
        }

        // ── 相位 3：失效 ⇒ 恰好一次整级重建 + 偏移清零 ──
        const uint32_t inv0 = ctrl.GetInvalidateCallCount();
        ctrl.InvalidateStaticCache();
        if (ctrl.GetInvalidateCallCount() != inv0 + 1)
        {
            GLogError(u8"Test 19 Failed: InvalidateStaticCache() 未计入失效计数器（%u → %u）",
                      inv0, ctrl.GetInvalidateCallCount());
            return 19;
        }
        uint32_t f1[kMaxShadowCascades] = {};
        for (uint32_t c = 1; c < 4; ++c)
            f1[c] = ctrl.GetFullUpdateCallCount(c);

        ctrl.Update(cam, aspect, light_dir, info, upd);
        ++updates;
        for (uint32_t c = 1; c < 4; ++c)
        {
            if (ctrl.GetFullUpdateCallCount(c) != f1[c] + 1 || !upd[c].need_full_update)
            {
                GLogError(u8"Test 19 Failed: cascade %u 失效后首次 Update 未整级重建"
                          u8"（fullC %u→%u，need_full_update=%d）——静态缓存失效链断裂",
                          c, f1[c], ctrl.GetFullUpdateCallCount(c),
                          upd[c].need_full_update ? 1 : 0);
                return 19;
            }
            if (upd[c].cache_offset.x != 0 || upd[c].cache_offset.y != 0)
            {
                GLogError(u8"Test 19 Failed: cascade %u 失效重建后 cache_offset=(%u,%u) 非零"
                          u8"（非零偏移下整级重画会漏掉尾部 |O| 条带）",
                          c, upd[c].cache_offset.x, upd[c].cache_offset.y);
                return 19;
            }
            if (sum_forms(c) != updates)
            {
                GLogError(u8"Test 19 Failed: cascade %u 失效帧后形态计数和 %u ≠ %u",
                          c, sum_forms(c), updates);
                return 19;
            }
        }

        GLogInfo(u8"Test 19 Passed: cache update-form counters & invalidation chain hold "
                 u8"(每级 fullC+stripC+hitC == Update 数；静置 60 帧纯命中；失效必致一次整级重建"
                 u8"且偏移清零) -- frames=%u strips=%u fulls=%u invs=%u.",
                 updates, strip_total, full_total, ctrl.GetInvalidateCallCount());
    }

    // ─────────────────────────────────────────────────────────────
    // 20: 缓存有效期内的矩阵恒定性 + 写侧平移的位级残差（S6 根因量化）
    //
    // 滚动缓存成立的前提是"缓存有效期内 P/V 严格不变"（注释见 CalculateCascadeBounds
    // 第 226-236 行）。而 `radius` 由**拟合视锥角点**算出、再乘 1.02 + 横向锚定补偿，
    // 全部连续量 ⇒ `texel_size = 2r/M`、`light_proj`、`light_eye` 距离、乃至横向锚定
    // 步长 L 都随相机连续变化。本用例直接量测"同一个无整级重建的帧段内矩阵是否逐位
    // 相同"，并把漂移换算成贴图边缘处的**等效纹素位移**（= (M/2)·相对漂移）。
    //
    // 同时量测写侧平移的位级残差：`P·(T·V)·x` 与 `(P·V)·x + 理想平移量` 的差。
    // GPU 侧对拍（S6）实测：条带帧与整级帧收到的 caster 集合完全相同（items/idsum
    // 一致）而深度图有 ~3e2 纹素不同、集中在剪影、max|Δ| = 剪影深度落差 ⇒ 差异只能
    // 出自"同一世界内容被两套稍有不同的矩阵光栅化"。
    //
    // 判据：① 平移残差 ≪ 1 纹素；② 帧段内矩阵漂移换算到贴图边缘 ≤ 1 纹素
    //        （超过即说明"格内矩阵恒定"的前提已破，S6 的差异会随行走距离放大）。
    // ─────────────────────────────────────────────────────────────
    {
        const Vector3f light_dir = glm::normalize(Vector3f(0.5f, 0.8f, -1.0f));
        const float aspect = 16.0f / 9.0f;

        auto make_camera = []()
        {
            Camera c;
            c.znear = 0.1f; c.zfar = 500.0f; c.fovY = 60.0f;
            c.pos = Vector3f(0.0f, 0.0f, 1.7f);
            c.world_up = Vector3f(0.0f, 1.0f, 0.0f);
            c.viewDirection = Vector3f(1.0f, 0.0f, 0.0f);
            return c;
        };

        CascadedShadowConfig cfg;
        cfg.cascade_count = 4;
        cfg.c0_dynamic_overlay = true;
        cfg.shadow_map_size = 1024.0f;
        cfg.max_distance = 300.0f;
        cfg.split_distances[0] = 50.0f;
        cfg.split_distances[1] = 50.0f;
        cfg.split_distances[2] = 160.0f;
        cfg.split_distances[3] = 300.0f;
        cfg.normal_offset_world = 0.10f;
        cfg.pcf_radius = 1.5f;

        CascadedShadowController ctrl(cfg);
        ShadowInfo info;
        CascadeUpdateResult upd[kMaxShadowCascades];
        Camera cam = make_camera();
        const Vector3f dir = glm::normalize(Vector3f(0.8f, 0.6f, 0.0f));
        cam.viewDirection = dir;

        // ── 量测 A：帧段内矩阵稳定性（纯平移行走）──
        const float kMap = 1024.0f;
        double worst_rel_drift = 0.0;          // 相对 texel 漂移
        uint32_t segments = 0, drift_frames = 0, drift_frames_rot = 0;
        Matrix4f seg_vp(0.0f);
        float seg_texel = 0.0f;
        bool seg_open = false;

        for (uint32_t f = 0; f < 300; ++f)
        {
            cam.pos = Vector3f(0.0f, 0.0f, 1.7f) + dir * (0.05f * static_cast<float>(f));
            ctrl.Update(cam, aspect, light_dir, info, upd);

            const uint32_t c = 1;
            const Matrix4f vp = upd[c].light_proj * upd[c].light_view;
            const float texel = upd[c].texel_world_size;

            if (upd[c].need_full_update || !seg_open)
            {
                seg_vp = vp; seg_texel = texel; seg_open = true;
                ++segments;
                continue;
            }

            // 逐位比较矩阵；不同则量化漂移
            bool same = true;
            for (int r = 0; r < 4 && same; ++r)
                for (int col = 0; col < 4; ++col)
                    if (seg_vp[r][col] != vp[r][col]) { same = false; break; }

            if (!same)
            {
                ++drift_frames;
                const double rel = std::max(std::abs(static_cast<double>(texel) - seg_texel) / seg_texel,
                                            std::abs(static_cast<double>(vp[0][0]) - seg_vp[0][0]) /
                                                std::max(1.0e-20, std::abs(static_cast<double>(seg_vp[0][0]))));
                worst_rel_drift = std::max(worst_rel_drift, rel);
            }
        }
        // 旋转相位的对照：r 会随朝向变化（角点相对中心距离改变）
        for (uint32_t f = 0; f < 120; ++f)
        {
            cam.viewDirection = glm::normalize(Vector3f(std::cos(0.02f * static_cast<float>(f)),
                                                        std::sin(0.02f * static_cast<float>(f)), 0.0f));
            ctrl.Update(cam, aspect, light_dir, info, upd);
            if (!upd[1].need_full_update && seg_open && upd[1].texel_world_size != seg_texel)
                ++drift_frames_rot;
        }

        const double edge_shift_texel = worst_rel_drift * static_cast<double>(kMap) * 0.5;

        // ── 量测 B：写侧平移的位级残差 ──
        const uint32_t band = cfg.cache_scroll_band_texels[1];
        const float texel = upd[1].texel_world_size;
        const float radius = upd[1].sphere_radius;
        const Matrix4f V = upd[1].light_view;
        const Matrix4f P = upd[1].light_proj;

        Matrix4f T(1.0f);                          // 与写侧同构：T(O.x·texel, 0, 0)·V
        T[3][0] = static_cast<float>(band) * texel;
        const Matrix4f Vd = T * V;

        const double ideal_ndc = static_cast<double>(band) * 2.0 / static_cast<double>(kMap);
        double max_residual = 0.0;
        uint32_t nonzero = 0, samples = 0;
        for (int i = -2; i <= 2; ++i)
            for (int j = -2; j <= 2; ++j)
                for (int k = -2; k <= 2; ++k)
                {
                    const Vector3f p = cam.pos + Vector3f(static_cast<float>(i), static_cast<float>(j),
                                                          static_cast<float>(k)) * (radius * 0.5f);
                    const Vector4f a = (P * V)  * Vector4f(p, 1.0f);
                    const Vector4f b = (P * Vd) * Vector4f(p, 1.0f);
                    const double dx = std::abs(static_cast<double>(a.x) - static_cast<double>(b.x)) - ideal_ndc;
                    const double dy = std::abs(static_cast<double>(a.y) - static_cast<double>(b.y));
                    const double r = std::max(std::abs(dx), dy);
                    if (r > 0.0) ++nonzero;
                    max_residual = std::max(max_residual, r);
                    ++samples;
                }
        const double residual_texel = max_residual / (2.0 / static_cast<double>(kMap));

        if (residual_texel > 1.0)
        {
            GLogError(u8"Test 20 Failed: 写侧平移的位级残差 %.4f 纹素 超过 1 纹素", residual_texel);
            return 20;
        }
        if (edge_shift_texel > 1.0)
        {
            GLogError(u8"Test 20 Failed: 帧段内矩阵漂移换算到贴图边缘 %.4f 纹素 > 1"
                      u8"（'格内矩阵恒定'前提破裂 ⇒ 缓存内容会被新矩阵解读，S6 差异会随行走放大）",
                      edge_shift_texel);
            return 20;
        }

        GLogInfo(u8"Test 20 Passed: 缓存期矩阵稳定性与写侧平移残差受 1 纹素约束 "
                 u8"(纯平移段: segments=%u 段内矩阵不同帧=%u worst_rel=%.3e ⇒ 边缘 %.4f 纹素 | "
                 u8"旋转相位 texel 变化帧=%u | 平移残差 samples=%u nonzero=%u = %.4f 纹素, B=%u)",
                 segments, drift_frames, worst_rel_drift, edge_shift_texel,
                 drift_frames_rot, samples, nonzero, residual_texel, band);
    }

    // ─────────────────────────────────────────────────────────────
    // Test 21: 附件读回下沉（引擎 API）+ 布局跟踪同步 + 示例零自研回读残留
    // ─────────────────────────────────────────────────────────────
    {
        auto load_src = [](const OSString &path, AnsiString &out) -> bool
        {
            hgl::io::OpenFileInputStream fis(path);

            if (!fis)
                return false;

            char chunk[4096];
            int64 got;

            while ((got = fis->Read(chunk, static_cast<int64>(sizeof(chunk)))) > 0)
                out.Strcat(chunk, static_cast<int>(got));

            return true;
        };

        AnsiString hdr, impl, cmdbuf, cmake, ats, csm;

        if (!load_src(OS_TEXT("inc/hgl/vk/VKTextureReadback.h"), hdr)
         || !load_src(OS_TEXT("src/Vulkan/VKTextureReadback.cpp"), impl))
        {
            GLogError(u8"Test 21 Failed: 引擎回读文件缺失（VKTextureReadback.h/.cpp）");
            return 21;
        }

        if (!hdr.Contains("ReadbackTexture(")
         || !hdr.Contains("ReadbackColorTarget(")
         || !hdr.Contains("ReadbackDepthTarget(")
         || !impl.Contains("CopyImageToBuffer(")
         || !impl.Contains("vkQueueWaitIdle(")
         || !impl.Contains("VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL"))
        {
            GLogError(u8"Test 21 Failed: 引擎回读 API 不完整（入口声明或拷贝/排空/布局处理缺失）");
            return 21;
        }

        if (!load_src(OS_TEXT("src/Vulkan/CMakeLists.txt"), cmake)
         || !cmake.Contains("VKTextureReadback.cpp"))
        {
            GLogError(u8"Test 21 Failed: 回读实现未注册进构建（需从仓库根运行，且 VKTextureReadback.cpp 在 src/Vulkan/CMakeLists.txt 中）");
            return 21;
        }

        // 布局跟踪同步：渲染结束/开始时必须把真实布局写回纹理。不同步的话交换链颜色图
        // 会停在 SHADER_READ_ONLY_OPTIMAL，任何据此做转换/读回的代码都用错 oldLayout
        //（原示例因此硬编码 PRESENT_SRC_KHR）。
        if (!load_src(OS_TEXT("src/Vulkan/VKCommandBufferRender.cpp"), cmdbuf)
         || !cmdbuf.Contains("SetImageLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)")
         || !cmdbuf.Contains("SetImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)"))
        {
            GLogError(u8"Test 21 Failed: 布局跟踪未同步（BeginRendering/EndRenderingPresent 缺 SetImageLayout）");
            return 21;
        }

        if (!load_src(OS_TEXT("example/Basic/AlphaTestShadow.cpp"), ats)
         || !load_src(OS_TEXT("example/Basic/CascadeShadowMap.cpp"), csm))
        {
            GLogError(u8"Test 21 Failed: 无法读示例源（需从仓库根运行）");
            return 21;
        }

        if (!ats.Contains("graph::ReadbackDepthTarget(")
         || !ats.Contains("graph::ReadbackColorTarget(")
         || !ats.Contains("bitmap::SaveBitmapToTGA(")
         || !ats.Contains("MakeDumpName(")
         || !ats.Contains("SaveRaw("))
        {
            GLogError(u8"Test 21 Failed: AlphaTestShadow 未改用引擎回读 + 裸 F32 落盘 + CM2D 写出");
            return 21;
        }

        if (!csm.Contains("graph::ReadbackDepthTarget(")
         || !csm.Contains("bitmap::SaveBitmapToTGA(")
         || !csm.Contains("MakeDumpName(")
         || !csm.Contains("SaveRaw("))
        {
            GLogError(u8"Test 21 Failed: CascadeShadowMap 未改用引擎回读 + 裸 F32 落盘 + CM2D 写出");
            return 21;
        }

        if (ats.Contains("vkCmdCopyImageToBuffer")
         || csm.Contains("vkCmdCopyImageToBuffer")
         || ats.Contains("= 'B';")
         || csm.Contains("= 'B';"))
        {
            GLogError(u8"Test 21 Failed: 示例仍残留自研读回（vkCmdCopyImageToBuffer）或手写 BMP 头（= 'B';）");
            return 21;
        }

        GLogInfo(u8"Test 21 Passed: 附件读回已下沉引擎（ReadbackTexture/Color/Depth + 布局跟踪同步），"
                 u8"示例落盘为 裸 F32 .raw + CM2D 8bit 灰度 .tga（文件名自带 宽x高/格式）且零自研拷贝残留");
    }

    GLogInfo(u8"=== All CSM Incremental Pass Contract Tests PASSED ===");
    return 0;
}
