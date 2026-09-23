#include <hgl/graph/render/lighting/CascadedShadowController.h>
#include <hgl/math/Projection.h>
#include <hgl/log/Log.h>
#include <numbers>
#include <cmath>

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
                                                         float &out_texel_size) const
    {
        Vector3f light_forward = -glm::normalize(light_dir);
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

        // 将包围球投影到固定光空间坐标系：
        // X 对应 light_right (UV 的 U 轴)
        // Y 对应 -light_up_actual (Vulkan UV 的 V 轴向向下)
        const float cx = glm::dot(sphere_center, light_right);
        const float cy = glm::dot(sphere_center, -light_up_actual);

        const float map_size = (config_.shadow_map_size > 0.0f) ? config_.shadow_map_size : 1024.0f;
        const float texel_size = (2.0f * radius) / map_size;

        const float snapped_cx = std::floor(cx / texel_size) * texel_size;
        const float snapped_cy = std::floor(cy / texel_size) * texel_size;

        // 计算吸附后的世界空间包围球中心
        const Vector3f snapped_sphere_center = sphere_center
                                             + (snapped_cx - cx) * light_right
                                             + (snapped_cy - cy) * (-light_up_actual);

        const Vector3f light_eye = snapped_sphere_center - light_forward * (radius + config_.caster_depth_margin);
        const Matrix4f light_view = LookAtMatrix(light_eye, snapped_sphere_center, light_up_actual);

        const float left   = -radius;
        const float right  = radius;
        const float bottom = -radius;
        const float top    = radius;

        const float znear = 0.1f;
        const float zfar  = 2.0f * radius + config_.caster_depth_margin;

        const Matrix4f light_proj = OrthoMatrixReversedZ(left, right, bottom, top, znear, zfar);

        out_view = light_view;
        out_proj = light_proj;
        out_snapped_center = Vector4f(snapped_cx, snapped_cy, left, bottom);
        out_texel_size = texel_size;
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
            const float split_near = (c == 0) ? near_z : splits[c - 1];
            const float split_far  = splits[c];

            Matrix4f light_view(1.0f);
            Matrix4f light_proj(1.0f);
            Vector4f snapped_center(0.0f);
            float texel_size = 1.0f;

            CalculateCascadeBounds(main_cam, aspect, split_near, split_far, light_dir,
                                   light_view, light_proj, snapped_center, texel_size);

            CascadeUpdateResult &update_res = out_updates[c];
            update_res.cascade_index = c;
            update_res.light_view = light_view;
            update_res.light_proj = light_proj;

            const float snapped_cx = snapped_center.x;
            const float snapped_cy = snapped_center.y;

            if (c == 0)
            {
                // 级联 0：动态近景，每帧全量重绘
                update_res.need_full_update = true;
                update_res.is_static_cache = false;
                update_res.AddDirtyRect(ShadowDirtyRect{0, 0, W, H});

                cache_states_[0].snapped_origin = Vector2f(snapped_cx, snapped_cy);
                cache_states_[0].texel_world_size = Vector2f(texel_size, texel_size);
                cache_states_[0].scroll_offset = Vector4u(0, 0, 0, 0);
                cache_states_[0].valid_rect = Vector4u(0, 0, W, H);
                cache_states_[0].scene_revision = scene_revision_;
                cache_states_[0].valid = 1;
                ++cache_states_[0].generation;
            }
            else
            {
                // 级联 1..N：中远景静态滚动缓存
                update_res.is_static_cache = true;

                if (cache_states_[c].valid == 0 || cache_states_[c].scene_revision != scene_revision_)
                {
                    // 首次生成或场景失效：全量重绘
                    update_res.need_full_update = true;
                    update_res.AddDirtyRect(ShadowDirtyRect{0, 0, W, H});

                    cache_states_[c].snapped_origin = Vector2f(snapped_cx, snapped_cy);
                    cache_states_[c].texel_world_size = Vector2f(texel_size, texel_size);
                    cache_states_[c].scroll_offset = Vector4u(0, 0, 0, 0);
                    cache_states_[c].valid_rect = Vector4u(0, 0, W, H);
                    cache_states_[c].scene_revision = scene_revision_;
                    cache_states_[c].valid = 1;
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
                        // 未跨越整像素：完全命中缓存，0 绘制开销
                        update_res.need_full_update = false;
                        update_res.ClearDirtyRects();
                    }
                    else if (std::abs(shift_x) >= static_cast<int32_t>(W) ||
                             std::abs(shift_y) >= static_cast<int32_t>(H))
                    {
                        // 位移超出整张贴图：全量重建
                        update_res.need_full_update = true;
                        update_res.AddDirtyRect(ShadowDirtyRect{0, 0, W, H});

                        cache_states_[c].snapped_origin = Vector2f(snapped_cx, snapped_cy);
                        cache_states_[c].texel_world_size = Vector2f(texel_size, texel_size);
                        cache_states_[c].scroll_offset = Vector4u(0, 0, 0, 0);
                        cache_states_[c].valid_rect = Vector4u(0, 0, W, H);
                        ++cache_states_[c].generation;
                    }
                    else
                    {
                        // 增量滚动更新：计算环形模偏与脏条带
                        update_res.need_full_update = false;

                        const uint32_t prev_ox = cache_states_[c].scroll_offset.x;
                        const uint32_t prev_oy = cache_states_[c].scroll_offset.y;

                        const uint32_t new_ox = static_cast<uint32_t>((static_cast<int64_t>(prev_ox) + shift_x) % W + W) % W;
                        const uint32_t new_oy = static_cast<uint32_t>((static_cast<int64_t>(prev_oy) + shift_y) % H + H) % H;

                        if (shift_x != 0)
                        {
                            const uint32_t x_start = (shift_x > 0) ? prev_ox : new_ox;
                            const uint32_t x_count = (shift_x > 0) ? static_cast<uint32_t>(shift_x) : static_cast<uint32_t>(-shift_x);

                            if (x_start + x_count <= W)
                            {
                                update_res.AddDirtyRect(ShadowDirtyRect{x_start, 0, x_count, H});
                            }
                            else
                            {
                                update_res.AddDirtyRect(ShadowDirtyRect{x_start, 0, W - x_start, H});
                                update_res.AddDirtyRect(ShadowDirtyRect{0, 0, x_count - (W - x_start), H});
                            }
                        }

                        if (shift_y != 0)
                        {
                            const uint32_t y_start = (shift_y > 0) ? prev_oy : new_oy;
                            const uint32_t y_count = (shift_y > 0) ? static_cast<uint32_t>(shift_y) : static_cast<uint32_t>(-shift_y);

                            if (y_start + y_count <= H)
                            {
                                update_res.AddDirtyRect(ShadowDirtyRect{0, y_start, W, y_count});
                            }
                            else
                            {
                                update_res.AddDirtyRect(ShadowDirtyRect{0, y_start, W, H - y_start});
                                update_res.AddDirtyRect(ShadowDirtyRect{0, 0, W, y_count - (H - y_start)});
                            }
                        }

                        cache_states_[c].snapped_origin = Vector2f(snapped_cx, snapped_cy);
                        cache_states_[c].texel_world_size = Vector2f(texel_size, texel_size);
                        cache_states_[c].scroll_offset = Vector4u(new_ox, new_oy, static_cast<uint32_t>(shift_x), static_cast<uint32_t>(shift_y));
                        ++cache_states_[c].generation;
                    }
                }
            }

            // 写入该级联的标准 ShadowCascadeInfo
            auto &casc = out_shadow_info.cascades[c];
            casc.shadow_vp = light_proj * light_view;
            casc.shadow_params = Vector4f(config_.bias, config_.pcf_radius, config_.darkness, 0.0f);
            casc.shadow_map_size = Vector2f(map_size, map_size);
            casc.inv_shadow_map_size = Vector2f(1.0f / map_size, 1.0f / map_size);

            if (cascade_textures_[c].x > 0)
                casc.shadow_tex = cascade_textures_[c];
            else if (out_shadow_info.shadow_tex.x > 0)
                casc.shadow_tex = Vector4u(out_shadow_info.shadow_tex.x, c, 0, 0);

            casc.cascade_params = Vector4f(split_near, split_far, config_.blend_width, (c > 0 ? 1.0f : 0.0f));
            casc.cache_origin = Vector4f(snapped_cx, snapped_cy, texel_size, texel_size);
            casc.cache_offset = cache_states_[c].scroll_offset;
            casc.cache_valid_rect = Vector4u(0, 0, W, H);
        }

        out_shadow_info.csm_params = Vector4u(count, 1, 0, 0);

        // 单级回退字段同步（自动使用第0级）
        out_shadow_info.shadow_vp = out_shadow_info.cascades[0].shadow_vp;
        out_shadow_info.shadow_params = out_shadow_info.cascades[0].shadow_params;
        out_shadow_info.shadow_map_size = out_shadow_info.cascades[0].shadow_map_size;
        out_shadow_info.inv_shadow_map_size = out_shadow_info.cascades[0].inv_shadow_map_size;
        if (out_shadow_info.cascades[0].shadow_tex.x > 0)
            out_shadow_info.shadow_tex = out_shadow_info.cascades[0].shadow_tex;
    }
}
