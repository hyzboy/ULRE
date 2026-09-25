#include <hgl/graph/render/lighting/CascadedShadowController.h>
#include <hgl/math/Projection.h>
#include <hgl/log/Log.h>
#include <numbers>
#include <cmath>

namespace
{
    // 阴影正交投影的近平面。CalculateCascadeBounds 用它建投影，Update 用它反推
    // 每级的深度范围（zfar - znear）以做逐级联 bias 解析，两处必须一致。
    constexpr float kShadowOrthoNearZ = 0.1f;
}

namespace hgl::graph
{
    CascadedShadowController::CascadedShadowController()
    {
        for (uint32_t i = 0; i < kMaxShadowCascades; ++i)
        {
            cache_states_[i] = ShadowCascadeCacheState{};
            cascade_textures_[i] = Vector4u(0);
        }
    }

    CascadedShadowController::CascadedShadowController(const CascadedShadowConfig &cfg)
        : config_(cfg)
    {
        for (uint32_t i = 0; i < kMaxShadowCascades; ++i)
        {
            cache_states_[i] = ShadowCascadeCacheState{};
            cascade_textures_[i] = Vector4u(0);
        }
    }

    void CascadedShadowController::SetCascadeTexture(uint32_t cascade_idx, uint32_t bindless_handle, uint32_t layer)
    {
        if (cascade_idx < kMaxShadowCascades)
        {
            cascade_textures_[cascade_idx] = Vector4u(bindless_handle, layer, 0, 0);
        }
    }

    void CascadedShadowController::InvalidateStaticCache()
    {
        ++scene_revision_;
        for (uint32_t i = 1; i < kMaxShadowCascades; ++i)
        {
            cache_states_[i].valid = 0;
        }
    }

    const ShadowCascadeCacheState &CascadedShadowController::GetCacheState(uint32_t cascade_idx) const
    {
        if (cascade_idx >= kMaxShadowCascades)
            return cache_states_[0];
        return cache_states_[cascade_idx];
    }

    void CascadedShadowController::CalculateSplitDistances(float near_z, float far_z, float out_splits[kMaxShadowCascades]) const
    {
        const uint32_t count = (config_.cascade_count > 0 && config_.cascade_count <= kMaxShadowCascades)
                             ? config_.cascade_count : kMaxShadowCascades;

        if (config_.use_custom_splits)
        {
            for (uint32_t i = 0; i < count; ++i)
            {
                float d = config_.split_distances[i];
                if (d < near_z) d = near_z;
                if (d > far_z)  d = far_z;
                out_splits[i] = d;
            }
            return;
        }

        // Practical Split Scheme: lambda * log_split + (1 - lambda) * uniform_split
        const float lambda = config_.split_lambda;
        for (uint32_t i = 0; i < count; ++i)
        {
            const float p = static_cast<float>(i + 1) / static_cast<float>(count);
            const float log_split = near_z * std::pow(far_z / near_z, p);
            const float uniform_split = near_z + (far_z - near_z) * p;
            out_splits[i] = lambda * log_split + (1.0f - lambda) * uniform_split;
        }
    }

    void CascadedShadowController::CalculateCascadeBounds(const Camera &main_cam,
                                                         float aspect,
                                                         float split_near,
                                                         float split_far,
                                                         const Vector3f &light_dir,
                                                         Matrix4f &out_view,
                                                         Matrix4f &out_proj,
                                                         Vector4f &out_snapped_center,
                                                         float &out_texel_size,
                                                         float &out_along_anchor,
                                                         float lateral_step) const
    {
        Vector3f light_forward = glm::normalize(light_dir);
        Vector3f light_up(0.0f, 0.0f, 1.0f);
        if (std::abs(glm::dot(light_forward, light_up)) > 0.99f)
            light_up = Vector3f(0.0f, 1.0f, 0.0f);

        Vector3f cam_forward = glm::normalize(main_cam.viewDirection);
        Vector3f cam_right = glm::normalize(glm::cross(cam_forward, main_cam.world_up));
        Vector3f cam_up = glm::cross(cam_right, cam_forward);

        const float fov_rad = static_cast<float>(main_cam.fovY * (std::numbers::pi / 180.0));
        const float tan_half_fov = std::tan(fov_rad * 0.5f);

        const float h_n = split_near * tan_half_fov;
        const float w_n = h_n * aspect;
        const float h_f = split_far * tan_half_fov;
        const float w_f = h_f * aspect;

        const Vector3f corners[8] = {
            main_cam.pos + cam_forward * split_near - cam_right * w_n - cam_up * h_n,
            main_cam.pos + cam_forward * split_near + cam_right * w_n - cam_up * h_n,
            main_cam.pos + cam_forward * split_near - cam_right * w_n + cam_up * h_n,
            main_cam.pos + cam_forward * split_near + cam_right * w_n + cam_up * h_n,
            main_cam.pos + cam_forward * split_far  - cam_right * w_f - cam_up * h_f,
            main_cam.pos + cam_forward * split_far  + cam_right * w_f - cam_up * h_f,
            main_cam.pos + cam_forward * split_far  - cam_right * w_f + cam_up * h_f,
            main_cam.pos + cam_forward * split_far  + cam_right * w_f + cam_up * h_f,
        };

        const float mid_dist = (split_near + split_far) * 0.5f;
        const Vector3f sphere_center = main_cam.pos + cam_forward * mid_dist;

        float radius = 0.0f;
        for (int k = 0; k < 8; ++k)
        {
            radius = std::max(radius, glm::length(corners[k] - sphere_center));
        }
        radius *= 1.02f;

        const Vector3f light_right = glm::normalize(glm::cross(light_forward, light_up));
        const Vector3f light_up_actual = glm::cross(light_right, light_forward);

        // ── 横向粗锚定（滚动静态缓存的必要条件之二）────────────────────────────
        // 仅沿光轴锚定是不够的：横向只要相机移动 1 个 texel，包围球中心就整体平移
        // 1 个 texel，缓存里的旧内容全部错位。实测 600 帧横向行走会把静态级联打成
        // 每 1.4 帧一次整级全量重绘（CSM1 435/600），"滚动更新"退化为"逐帧全量"。
        //
        // 做法：把包围球中心在 light_right / -light_up_actual 两轴上吸附到
        // lateral_step 的整数倍。step 内中心完全静止 ⇒ 布局矩阵恒定，且 texel 吸附
        // 后位移恒为 0 ⇒ 缓存整段有效。跨格时中心跳变 step，位移必然非零而触发重建。
        //
        // 代价：吸附点只有落在 step 的整数格上，真实包围球中心在格内最远可偏离
        // step/2（每轴），即对角 0.707*step；因此必须把包围球半径扩大同样的量，
        // 否则视锥切片角点会掉出正交视窗（画面边缘物体没有阴影）。
        // 半径变大 ⇒ texel 变粗，这是本方案用一点静态阴影精度换掉绝大部分重绘开销。
        // 注意这里必须用 round 而不是 floor：floor 的偏离量是整整一个步长（[0,step)），
        // 半径就要按 1.414*step 扩，几乎翻倍。
        const float lateral_anchor = (lateral_step > 0.0f) ? lateral_step : 0.0f;
        if (lateral_anchor > 0.0f)
            radius += lateral_anchor * 0.708f;

        // ── 沿光轴的深度锚定（滚动静态缓存的必要条件）────────────────────────
        // 中远景级联走的是"静态物体滚动缓存"：整张 shadowmap 里的深度是多帧累积
        // 的结果，只有脏条带会被重绘。这要求光空间视图/投影矩阵的**深度分量**在
        // 缓存有效期内保持恒定——否则每帧的 shadow_vp 都会把缓存里的旧深度解释成
        // 另一个值：相机沿光轴每移动 1 个 texel 的横向距离，深度就整体偏移
        // 1/map_size 的完整深度范围，走几米就足以让被阴影物体自身（接收者）的深度
        // 掉出缓存深度窗口，表现为"远处地面不再接收阴影"（局部重建无法自愈）。
        //
        // 光轴方向的平移不改变 texel 对齐（不产生 xy 环形寻址位移），因此可以把
        // 包围球中心沿光轴吸附到 step 的整数倍上：step 内所有帧的深度矩阵完全一致，
        // 缓存恒定有效；跨越 step 时才让整级联失效重建一次。
        //
        // 注意 cx/cy 用的是 sphere_center 在 light_right / light_up_actual 上的投影，
        // 而锚定位移沿 light_forward，与这两个轴均正交，因此吸附不会改变 texel 对齐。
        const float anchor_step = (config_.cache_anchor_step > 0.0f) ? config_.cache_anchor_step : 0.0f;
        const float along = glm::dot(sphere_center, light_forward);
        float along_anchor = along;
        Vector3f anchored_center = sphere_center;

        if (anchor_step > 0.0f)
        {
            along_anchor = std::floor(along / anchor_step) * anchor_step;
            anchored_center = sphere_center - light_forward * (along - along_anchor);
        }

        // 将包围球投影到固定光空间坐标系：
        // X 对应 light_right (UV 的 U 轴)
        // Y 对应 -light_up_actual (Vulkan UV 的 V 轴向向下)
        const float cx0 = glm::dot(anchored_center, light_right);
        const float cy0 = glm::dot(anchored_center, -light_up_actual);

        float cx = cx0;
        float cy = cy0;

        if (lateral_anchor > 0.0f)
        {
            cx = std::round(cx0 / lateral_anchor) * lateral_anchor;
            cy = std::round(cy0 / lateral_anchor) * lateral_anchor;
        }

        const float map_size = (config_.shadow_map_size > 0.0f) ? config_.shadow_map_size : 1024.0f;
        const float texel_size = (2.0f * radius) / map_size;

        const float snapped_cx = std::floor(cx / texel_size) * texel_size;
        const float snapped_cy = std::floor(cy / texel_size) * texel_size;

        // 计算吸附后的世界空间包围球中心
        //
        // 关键：这里必须回到**粗锚定点**（cx/cy）而不是原始中心（cx0/cy0）。
        // 若用 (snapped_cx - cx)，则 coarse 吸附被抵消，包围球中心仍随视锥每帧连续
        // 移动 ⇒ 光空间视图/投影矩阵每帧都变 ⇒ 缓存里的旧深度被新矩阵解读，
        // 静态阴影在缓存有效期内会整体滑动。回到锚定点后，光照矩阵在一个锚定格
        // 内完全恒定，缓存命中时矩阵与生成该深度时的矩阵严格一致（这才是"静态
        // 滚动缓存"能成立的前提）；格内视锥相对窗口最多漂移 0.707*step，由上面的
        // 半径补偿保证仍被覆盖。
        const Vector3f snapped_sphere_center = anchored_center
                                             + (snapped_cx - cx0) * light_right
                                             + (snapped_cy - cy0) * (-light_up_actual);

        const Vector3f light_eye = snapped_sphere_center - light_forward * (radius + config_.caster_depth_margin);
        const Matrix4f light_view = LookAtMatrix(light_eye, snapped_sphere_center, light_up_actual);

        const float left   = -radius;
        const float right  = radius;
        const float bottom = -radius;
        const float top    = radius;

        const float znear = kShadowOrthoNearZ;
        // 远平面既要容纳包围球自身向后延伸的 radius + anchor_step，
        // 也要留足接收者（如地面在包围球下方较深位置）的深度余量（对称扩展 caster_depth_margin），
        // 否则相机升高、俯仰或晃动时，下方的地面深度就会超过 zfar 被裁掉（light_ndc.z < 0.0 判为无阴影）。
        const float zfar  = 2.0f * radius + 2.0f * config_.caster_depth_margin + anchor_step;

        const Matrix4f light_proj = OrthoMatrixReversedZ(left, right, bottom, top, znear, zfar);

        out_view = light_view;
        out_proj = light_proj;
        out_snapped_center = Vector4f(snapped_cx, snapped_cy, left, bottom);
        out_texel_size = texel_size;
        out_along_anchor = along_anchor;
    }

    void CascadedShadowController::Update(const Camera &main_cam,
                                         float aspect,
                                         const Vector3f &light_dir,
                                         ShadowInfo &out_shadow_info,
                                         CascadeUpdateResult out_updates[kMaxShadowCascades])
    {
        const uint32_t count = (config_.cascade_count > 0 && config_.cascade_count <= kMaxShadowCascades)
                             ? config_.cascade_count : kMaxShadowCascades;

        float splits[kMaxShadowCascades]{};
        const float near_z = (main_cam.znear > 0.0f) ? main_cam.znear : 0.1f;
        const float far_z  = (config_.max_distance > near_z) ? config_.max_distance : (near_z + 100.0f);
        CalculateSplitDistances(near_z, far_z, splits);

        for (uint32_t i = 0; i < kMaxShadowCascades; ++i)
        {
            out_updates[i] = CascadeUpdateResult{};
            out_updates[i].cascade_index = i;
        }

        const float map_size = (config_.shadow_map_size > 0.0f) ? config_.shadow_map_size : 1024.0f;
        const uint32_t W = static_cast<uint32_t>(map_size);
        const uint32_t H = static_cast<uint32_t>(map_size);

        for (uint32_t c = 0; c < count; ++c)
        {
            const float split_near = (c == 0 || (c == 1 && config_.c0_dynamic_overlay)) ? near_z : splits[c - 1];
            const float split_far  = splits[c];

            Matrix4f light_view(1.0f);
            Matrix4f light_proj(1.0f);
            Vector4f snapped_center(0.0f);
            float texel_size = 1.0f;
            float along_anchor = 0.0f;

            CalculateCascadeBounds(main_cam, aspect, split_near, split_far, light_dir,
                                   light_view, light_proj, snapped_center, texel_size, along_anchor,
                                   (c == 0) ? 0.0f : config_.cache_lateral_anchor_step);

            CascadeUpdateResult &update_res = out_updates[c];
            update_res.cascade_index = c;
            update_res.light_view = light_view;
            update_res.light_proj = light_proj;
            // texel_size = 2*radius/map_size ⇒ 反解包围球半径，供上层把归一化 bias 折算成世界偏移
            update_res.sphere_radius = texel_size * map_size * 0.5f;

            // 该级联的深度范围，与 CalculateCascadeBounds 里 zfar 的推导保持一致
            // （radius 已由 texel_size 精确反解，lateral 补偿只有 c > 0 才有，已包含在内）
            const float anchor_step_zfar = (config_.cache_anchor_step > 0.0f) ? config_.cache_anchor_step : 0.0f;
            const float zfar_c = 2.0f * update_res.sphere_radius
                               + 2.0f * config_.caster_depth_margin
                               + anchor_step_zfar;
            update_res.depth_range = zfar_c - kShadowOrthoNearZ;

            // 逐级联 bias 解析：世界单位优先（全场景世界偏移恒定），否则用归一化 bias 乘每级系数
            float resolved_bias = config_.bias * config_.per_cascade_bias_scale[c];
            if (config_.bias_world != 0.0f && update_res.depth_range > 0.0f)
                resolved_bias = config_.bias_world / update_res.depth_range;
            update_res.resolved_bias = resolved_bias;

            const float snapped_cx = snapped_center.x;
            const float snapped_cy = snapped_center.y;

            if (c == 0)
            {
                // 级联 0：全动态近距，每帧全量重绘
                update_res.need_full_update = true;
                update_res.is_static_cache = false;
                update_res.AddDirtyRect(ShadowDirtyRect{0, 0, W, H});

                cache_states_[0].snapped_origin = Vector2f(snapped_cx, snapped_cy);
                cache_states_[0].texel_world_size = Vector2f(texel_size, texel_size);
                cache_states_[0].scroll_offset = Vector4u(0, 0, 0, 0);
                cache_states_[0].valid_rect = Vector4u(0, 0, W, H);
                cache_states_[0].scene_revision = scene_revision_;
                cache_states_[0].valid = 1;
                along_anchor_[0] = along_anchor;
                ++cache_states_[0].generation;
            }
            else
            {
                // 级联 1..N：静态滚动缓存（含 CSM 1 近+中景、CSM 2 远景、CSM 3 超远景；相机不移动不更新）
                update_res.is_static_cache = true;

                if (cache_states_[c].valid == 0 || cache_states_[c].scene_revision != scene_revision_ ||
                    (config_.cache_anchor_step > 0.0f && along_anchor_[c] != along_anchor))
                {
                    // 首次生成 / 场景失效 / 深度锚点跨步：全量重绘
                    update_res.need_full_update = true;
                    update_res.AddDirtyRect(ShadowDirtyRect{0, 0, W, H});

                    cache_states_[c].snapped_origin = Vector2f(snapped_cx, snapped_cy);
                    cache_states_[c].texel_world_size = Vector2f(texel_size, texel_size);
                    cache_states_[c].scroll_offset = Vector4u(0, 0, 0, 0);
                    cache_states_[c].valid_rect = Vector4u(0, 0, W, H);
                    cache_states_[c].scene_revision = scene_revision_;
                    cache_states_[c].valid = 1;
                    along_anchor_[c] = along_anchor;
                    ++cache_states_[c].generation;
                }
                else
                {
                    const float dx = snapped_cx - cache_states_[c].snapped_origin.x;
                    const float dy = snapped_cy - cache_states_[c].snapped_origin.y;
                    const int32_t shift_x = static_cast<int32_t>(std::round(dx / texel_size));
                    const int32_t shift_y = static_cast<int32_t>(std::round(dy / texel_size));

                    update_res.texel_shift_x = shift_x;
                    update_res.texel_shift_y = shift_y;

                    if (shift_x == 0 && shift_y == 0)
                    {
                        // 未跨越整像素：完全命中缓存，0 绘制开销（相机不移动不更新）
                        update_res.need_full_update = false;
                        update_res.ClearDirtyRects();
                    }
                    else
                    {
                        // 相机移动跨越整像素：滚动更新刷新至新中心
                        update_res.need_full_update = true;
                        update_res.AddDirtyRect(ShadowDirtyRect{0, 0, W, H});

                        cache_states_[c].snapped_origin = Vector2f(snapped_cx, snapped_cy);
                        cache_states_[c].texel_world_size = Vector2f(texel_size, texel_size);
                        cache_states_[c].scroll_offset = Vector4u(0, 0, 0, 0);
                        cache_states_[c].valid_rect = Vector4u(0, 0, W, H);
                        along_anchor_[c] = along_anchor;
                        ++cache_states_[c].generation;
                    }
                }
            }

            // 写入该级联的标准 ShadowCascadeInfo
            auto &casc = out_shadow_info.cascades[c];
            casc.shadow_vp = light_proj * light_view;
            casc.shadow_params = Vector4f(update_res.resolved_bias, config_.pcf_radius,
                                          config_.darkness, config_.normal_offset_world);
            casc.shadow_map_size = Vector2f(map_size, map_size);
            casc.inv_shadow_map_size = Vector2f(1.0f / map_size, 1.0f / map_size);

            if (cascade_textures_[c].x > 0)
                casc.shadow_tex = cascade_textures_[c];
            else if (out_shadow_info.shadow_tex.x > 0)
                casc.shadow_tex = Vector4u(out_shadow_info.shadow_tex.x, c, 0, 0);

            casc.cascade_params = Vector4f(split_near, split_far, config_.blend_width, config_.blend_distance);
            casc.cache_origin = Vector4f(snapped_cx, snapped_cy, texel_size, texel_size);
            casc.cache_offset = cache_states_[c].scroll_offset;
            casc.cache_valid_rect = Vector4u(0, 0, W, H);
        }

        out_shadow_info.csm_params = Vector4u(count, config_.c0_dynamic_overlay ? 2u : 1u, 0, 0);

        // 单级回退字段同步（自动使用第0级）
        out_shadow_info.shadow_vp = out_shadow_info.cascades[0].shadow_vp;
        out_shadow_info.shadow_params = out_shadow_info.cascades[0].shadow_params;
        out_shadow_info.shadow_map_size = out_shadow_info.cascades[0].shadow_map_size;
        out_shadow_info.inv_shadow_map_size = out_shadow_info.cascades[0].inv_shadow_map_size;
        if (out_shadow_info.cascades[0].shadow_tex.x > 0)
            out_shadow_info.shadow_tex = out_shadow_info.cascades[0].shadow_tex;
    }
}
