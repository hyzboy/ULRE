#include <hgl/ecs/core/RenderPassRequest.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/core/ScenePipelineMode.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/components/PrimitiveComponent.h>
#include <hgl/ecs/components/ShadowComponent.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
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
        cfg.cache_lateral_anchor_step = 2.0f;

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
        //   5B-1 矩阵恒定性：缓存命中（need_full_update==false）意味着本帧不重绘贴图，
        //        因此本帧用的 light_proj*light_view 必须与"生成该贴图那一帧"完全一致；
        //        否则贴图里的深度被另一个矩阵解读，静态阴影会在锚定格内整体滑动。
        //        （历史缺陷：横向吸附量被二次吸附抵消 ⇒ 矩阵仍随相机连续移动）
        //   5B-2 覆盖率：窗口冻结在粗锚定点上，格内视锥最多漂移 step*0.707，半径补偿
        //        不足时切片角点落到正交视窗之外 ⇒ 边缘物体没有阴影。子用例故意让补偿
        //        量与半径同量级（真实配置下补偿只占半径百分之几），保证补偿一旦缺失
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
                ccfg.cache_lateral_anchor_step = 2.0f;

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

                        if (supd[c].need_full_update)
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
                ccfg.cache_lateral_anchor_step = 10.0f;

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
                        // 缓存命中帧沿用它一开始被重绘时的矩阵：贴图里的深度就是按那个
                        // 矩阵写进去的，覆盖率必须按同一矩阵判定。
                        if (supd[c].need_full_update || !frozen_valid[c])
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

                GLogInfo(u8"[CSM-COVERAGE] lateral_step=10m frames=%d fail=%u worst_ndc=[%f,%f,%f,%f]",
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
        ccfg.cache_lateral_anchor_step = 2.0f; // 横向锚定

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

                if (supd[c].need_full_update)
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
                { "EvalPCFShadowAt(sample_pos, surface.worldPos)",
                  "offset must not leak into cascade selection (selectPos must stay un-offset)" },
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

            GLogInfo(u8"Test 9 Passed: ScenePipelineMode & Automated Shadow Workflow Contracts verified.");
        }
    }

    GLogInfo(u8"=== All CSM Incremental Pass Contract Tests PASSED ===");
    return 0;
}
