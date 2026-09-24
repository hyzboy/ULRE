#include <hgl/graph/render/lighting/CascadedShadowController.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <cmath>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::math;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    hgl::logger::InitLogger(OS_TEXT("TestCascadedShadowController"));

    GLogInfo(u8"=== Testing CascadedShadowController (CSM Rolling Toroidal Cache) ===");

    CascadedShadowConfig config;
    config.cascade_count = 4;
    config.shadow_map_size = 1024.0f;
    config.use_custom_splits = true;
    config.split_distances[0] = 15.0f;
    config.split_distances[1] = 45.0f;
    config.split_distances[2] = 100.0f;
    config.split_distances[3] = 250.0f;
    // Test 1..5 只关心滚动缓存/脏矩形逻辑，禁用沿光轴锚定（step = 0）以免掩盖被测行为；
    // 锚定本身由 Test 6 单独覆盖。
    config.cache_anchor_step = 0.0f;

    CascadedShadowController controller(config);

    controller.SetCascadeTexture(0, 100, 0);
    controller.SetCascadeTexture(1, 100, 1);
    controller.SetCascadeTexture(2, 100, 2);
    controller.SetCascadeTexture(3, 100, 3);

    Camera cam;
    cam.pos = Vector3f(0.0f, 0.0f, 2.0f);
    cam.viewDirection = Vector3f(0.0f, 1.0f, 0.0f);
    cam.world_up = Vector3f(0.0f, 0.0f, 1.0f);
    cam.fovY = 60.0f;
    cam.znear = 0.1f;
    cam.zfar = 1000.0f;

    const float aspect = 16.0f / 9.0f;
    const Vector3f light_dir = glm::normalize(Vector3f(0.5f, 0.5f, -1.0f));

    ShadowInfo shadow_info;
    CascadeUpdateResult updates[kMaxShadowCascades];

    // ─────────────────────────────────────────────────────────────
    // Test 1: First update (initial generation)
    // ─────────────────────────────────────────────────────────────
    controller.Update(cam, aspect, light_dir, shadow_info, updates);

    if (shadow_info.csm_params.x != 4)
    {
        GLogError(u8"Test 1 Failed: csm_params.x != 4");
        return 1;
    }

    // Cascade 0: dynamic, full update
    if (!updates[0].need_full_update || updates[0].is_static_cache)
    {
        GLogError(u8"Test 1 Failed: cascade 0 should be full update dynamic");
        return 1;
    }

    // Cascade 1..3: static cache, first frame must be full update
    for (uint32_t c = 1; c < 4; ++c)
    {
        if (!updates[c].need_full_update || !updates[c].is_static_cache)
        {
            GLogError(u8"Test 1 Failed: cascade %u should be static full update on first frame", c);
            return 1;
        }
        if (updates[c].dirty_rect_count != 1 ||
            updates[c].dirty_rects[0].width != 1024 ||
            updates[c].dirty_rects[0].height != 1024)
        {
            GLogError(u8"Test 1 Failed: cascade %u dirty rect should be full (0, 0, 1024, 1024)", c);
            return 1;
        }
    }

    // Check projection matrix convention: Vulkan RH ZO, X not flipped (m00 > 0), Y flipped (m11 < 0)
    for (uint32_t c = 0; c < 4; ++c)
    {
        const Matrix4f &vp = shadow_info.cascades[c].shadow_vp;
        // Check that vp is valid
        if (std::isnan(vp[0][0]) || std::isnan(vp[1][1]) || std::isnan(vp[2][2]))
        {
            GLogError(u8"Test 1 Failed: cascade %u shadow_vp has NaN", c);
            return 1;
        }
    }
    GLogInfo(u8"[PASS] Test 1: Initial generation and full updates passed");

    // ─────────────────────────────────────────────────────────────
    // Test 2: Stationary camera (no movement)
    // ─────────────────────────────────────────────────────────────
    controller.Update(cam, aspect, light_dir, shadow_info, updates);

    // Cascade 0 must still update
    if (!updates[0].need_full_update)
    {
        GLogError(u8"Test 2 Failed: cascade 0 must update every frame");
        return 1;
    }

    // Cascades 1..3 must skip completely (0 dirty rects)
    for (uint32_t c = 1; c < 4; ++c)
    {
        if (updates[c].need_full_update)
        {
            GLogError(u8"Test 2 Failed: cascade %u should not need full update when stationary", c);
            return 1;
        }
        if (updates[c].dirty_rect_count != 0)
        {
            GLogError(u8"Test 2 Failed: cascade %u should have 0 dirty rects when stationary, got %u",
                     c, updates[c].dirty_rect_count);
            return 1;
        }
    }
    GLogInfo(u8"[PASS] Test 2: Stationary camera static cache hits (zero redraw) passed");

    // ─────────────────────────────────────────────────────────────
    // Test 3: Incremental camera movement (scrolling update)
    // ─────────────────────────────────────────────────────────────
    // Move camera forward by a small distance that translates in light space
    cam.pos += cam.viewDirection * 5.0f;
    controller.Update(cam, aspect, light_dir, shadow_info, updates);

    // Cascade 0: always full update
    if (!updates[0].need_full_update)
    {
        GLogError(u8"Test 3 Failed: cascade 0 must update");
        return 1;
    }

    // Cascades 1..3: should be incremental (need_full_update == false) with dirty strips
    bool any_incremental = false;
    for (uint32_t c = 1; c < 4; ++c)
    {
        if (updates[c].need_full_update)
        {
            GLogError(u8"Test 3 Failed: cascade %u should NOT need full update for small move", c);
            return 1;
        }
        if (updates[c].dirty_rect_count > 0)
        {
            any_incremental = true;
            // Verify all dirty rects are within bounds [0, 1024]
            for (uint32_t r = 0; r < updates[c].dirty_rect_count; ++r)
            {
                const auto &rect = updates[c].dirty_rects[r];
                if (rect.x + rect.width > 1024 || rect.y + rect.height > 1024)
                {
                    GLogError(u8"Test 3 Failed: dirty rect out of bounds: x=%u w=%u y=%u h=%u",
                             rect.x, rect.width, rect.y, rect.height);
                    return 1;
                }
            }
        }
    }
    if (!any_incremental)
    {
        GLogError(u8"Test 3 Failed: at least one cascade should have non-zero dirty strips on movement");
        return 1;
    }
    GLogInfo(u8"[PASS] Test 3: Incremental scrolling strip generation passed");

    // ─────────────────────────────────────────────────────────────
    // Test 4: Huge jump (exceeding shadow map size -> full update)
    // ─────────────────────────────────────────────────────────────
    cam.pos += cam.viewDirection * 5000.0f;
    controller.Update(cam, aspect, light_dir, shadow_info, updates);

    for (uint32_t c = 0; c < 4; ++c)
    {
        if (!updates[c].need_full_update)
        {
            GLogError(u8"Test 4 Failed: cascade %u should require full update after huge jump", c);
            return 1;
        }
    }
    GLogInfo(u8"[PASS] Test 4: Huge jump triggers full invalidation passed");

    // ─────────────────────────────────────────────────────────────
    // Test 5: Manual cache invalidation (static scene change)
    // ─────────────────────────────────────────────────────────────
    // First, stationary update to settle cache
    controller.Update(cam, aspect, light_dir, shadow_info, updates);
    if (updates[1].dirty_rect_count != 0)
    {
        GLogError(u8"Test 5 Setup Failed: settled frame should have 0 dirty rects");
        return 1;
    }

    // Invalidate
    controller.InvalidateStaticCache();
    controller.Update(cam, aspect, light_dir, shadow_info, updates);

    for (uint32_t c = 1; c < 4; ++c)
    {
        if (!updates[c].need_full_update)
        {
            GLogError(u8"Test 5 Failed: cascade %u must be full update after InvalidateStaticCache()", c);
            return 1;
        }
    }
    GLogInfo(u8"[PASS] Test 5: Manual InvalidateStaticCache() passed");

    // ─────────────────────────────────────────────────────────────
    // Test 6: 沿光轴移动必须触发深度锚定失效
    // ─────────────────────────────────────────────────────────────
    // 滚动静态缓存的深度是多帧累积的，只有脏条带会被重绘。若光空间视图矩阵的深度
    // 分量随相机连续变化，缓存里的旧深度就会与本帧 shadow_vp 失配，走几米后连
    // 被阴影物体自身的深度都会掉出缓存深度窗口（表现为"远处地面不接收阴影"）。
    // 因此控制器必须把深度锚点量化到固定步长，跨步时整级联重建。
    {
        const Vector3f light_forward = -glm::normalize(light_dir);
        const Vector3f base_pos = cam.pos;

        // 启用默认量级的锚定步长，让沿光轴漂移真正参与判定
        config.cache_anchor_step = 16.0f;
        controller.SetConfig(config);

        controller.Update(cam, aspect, light_dir, shadow_info, updates);

        // 纯沿光轴平移：不改变光空间 xy 对齐与包围球半径，只改变深度锚点
        cam.pos = base_pos + light_forward * (config.cache_anchor_step * 1.5f);
        controller.Update(cam, aspect, light_dir, shadow_info, updates);

        for (uint32_t c = 1; c < 4; ++c)
        {
            if (!updates[c].need_full_update)
            {
                GLogError(u8"Test 6 Failed: cascade %u must be fully rebuilt after light-axis move >= anchor step", c);
                return 1;
            }
            if (updates[c].dirty_rect_count != 1 ||
                updates[c].dirty_rects[0].width != 1024 ||
                updates[c].dirty_rects[0].height != 1024)
            {
                GLogError(u8"Test 6 Failed: cascade %u anchor invalidation should cover full map", c);
                return 1;
            }
        }

        // 重新锚定后静止一帧：缓存必须重新安定（0 脏矩形）
        controller.Update(cam, aspect, light_dir, shadow_info, updates);
        for (uint32_t c = 1; c < 4; ++c)
        {
            if (updates[c].need_full_update || updates[c].dirty_rect_count != 0)
            {
                GLogError(u8"Test 6 Failed: cascade %u should re-settle after anchor move", c);
                return 1;
            }
        }
        GLogInfo(u8"[PASS] Test 6: Light-axis depth anchoring invalidation passed");
    }

    GLogInfo(u8"=== All CascadedShadowController tests PASSED successfully! ===");
    return 0;
}
