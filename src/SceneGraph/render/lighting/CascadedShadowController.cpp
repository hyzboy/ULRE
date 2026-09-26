#include <hgl/graph/render/lighting/CascadedShadowController.h>
#include <hgl/math/Projection.h>
#include <hgl/log/Log.h>
#include <numbers>
#include <cmath>

namespace
{
    // 阴影正交投影的近平面。CalculateCascadeBounds 用它建投影并输出每级 zfar，
    // Update 用（zfar - znear）做逐级联 bias 解析——zfar 唯一推导处在拟合函数内。
    constexpr float kShadowOrthoNearZ = 0.1f;
}

namespace hgl::graph
{
    namespace
    {
        /// 环形偏移累加：off + delta 归一到 [0, map_size)（delta 允许为负）
        inline uint32_t WrapTexelOffset(uint32_t off, int32_t delta, uint32_t map_size)
        {
            if (map_size == 0)
                return 0;
            const int64_t m = static_cast<int64_t>(map_size);
            const int64_t v = (static_cast<int64_t>(off) + static_cast<int64_t>(delta)) % m;
            return static_cast<uint32_t>((v + m) % m);
        }

        /// 把"物理坐标下的条带"追加进脏矩形池：主轴起点 start、宽 width（texel）。
        /// 条带可以跨贴图接缝（start + width > map_size）——环形缓存里"贴图右边缘"与
        /// "左边缘"物理相邻，跨缝条带必须拆成两段矩形，否则尾部 width-first 个纹素
        /// 不会被重画（该处阴影保持过期内容，表现为贴图边缘一条 1~2 纹素宽的亮/暗带）。
        /// vertical = true  ⇒ 竖直条带（x 受限、y 占满全高）
        /// vertical = false ⇒ 水平条带（y 受限、x 占满全宽）
        inline void AppendWrappedStrip(CascadeUpdateResult &res, uint32_t start, uint32_t width,
                                       uint32_t map_size, bool vertical)
        {
            if (map_size == 0 || width == 0 || width >= map_size)
                return;

            const uint32_t begin = start % map_size;
            const uint32_t rest  = map_size - begin;   // 从 begin 到贴图末端的纹素数
            const uint32_t first = (width < rest) ? width : rest;

            if (vertical)
            {
                res.AddDirtyRect(ShadowDirtyRect{ begin, 0, first, map_size });
                if (width > first)
                    res.AddDirtyRect(ShadowDirtyRect{ 0, 0, width - first, map_size });
            }
            else
            {
                res.AddDirtyRect(ShadowDirtyRect{ 0, begin, map_size, first });
                if (width > first)
                    res.AddDirtyRect(ShadowDirtyRect{ 0, 0, map_size, width - first });
            }
        }
    }

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
        ++invalidate_call_count_;   // 诊断：累计失效调用次数
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

    const CascadeUpdateStats &CascadedShadowController::GetUpdateStats(uint32_t cascade_idx) const
    {
        if (cascade_idx >= kMaxShadowCascades)
            return update_stats_[0];
        return update_stats_[cascade_idx];
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
                                                         float &out_zfar,
                                                         uint32_t band_texels) const
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
        // 做法：把包围球中心在 light_right / -light_up_actual 两轴上吸附到锚定格 L 的
        // 整数倍。格内中心完全静止 ⇒ 布局矩阵恒定，缓存整段有效；跨格时中心跳变 L。
        //
        // **步长以 shadowmap 侧表达（texel 数 B，不是世界米）**，理由：L 同时是
        //   ① 环形偏移的量子——`cache_offset` 是 uvec，滚动必须按整数 texel 走；
        //   ② 半径补偿量——格内真实中心最远偏离 L/2（每轴），对角 0.708·L，必须把半径
        //      扩大同样的量，否则视锥切片角点掉出正交视窗（画面边缘物体没有阴影）。
        // 而"半径变大 ⇒ texel 变粗"的比例是 0.708·L/r0，把它写成 B 的形式：
        //     texel = 2(r0 + 0.708·L)/M,  L = B·texel
        //     ⇒ L = 2·B·r0 / (M − 1.416·B)       （闭式，一次算准，无需迭代）
        //     ⇒ 精度损失 = 0.708·L/r0 = 1.416·B/(M − 1.416·B)（与切片半径 r0、贴图分辨率 M 都无关）
        // 于是"锚定格恰为 B 个纹素"（环形偏移天然整数）与"格内矩阵恒定"同时成立，
        // 且代价只由 B/M 决定：B=16 ⇒ 2.3%，B=128(=M/8) ⇒ 21.5%（开到 4096 也一样）。
        // 逐级给值：近景 B 小（世界步长小 ⇒ 滞后窄、精度损失小），远景可大。
        // 必须用 round 而不是 floor 做格点吸附：floor 的偏离量是整整一个步长，半径就要
        // 按 1.414·L 扩，几乎翻倍。
        const float map_size = (config_.shadow_map_size > 0.0f) ? config_.shadow_map_size : 1024.0f;
        float lateral_anchor = 0.0f;
        if (band_texels > 0)
        {
            const float band = static_cast<float>(band_texels);
            const float denom = map_size - 1.416f * band;   // B ≥ M/1.416 时退化
            if (denom > 0.0f)
                lateral_anchor = (2.0f * band * radius) / denom;
        }

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
        out_zfar = zfar; // A9：zfar 的唯一推导处——Update 只消费本输出，不再反推

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
        ++update_call_count_;   // 诊断：累计 Update 调用次数
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
            float zfar_c = 0.0f;

            CalculateCascadeBounds(main_cam, aspect, split_near, split_far, light_dir,
                                   light_view, light_proj, snapped_center, texel_size, along_anchor,
                                   zfar_c,
                                   config_.cache_scroll_band_texels[c]);

            CascadeUpdateResult &update_res = out_updates[c];
            update_res.cascade_index = c;
            update_res.light_view = light_view;
            update_res.light_proj = light_proj;
            // texel_size = 2*radius/map_size ⇒ 反解包围球半径，供上层把归一化 bias 折算成世界偏移
            update_res.sphere_radius = texel_size * map_size * 0.5f;

            // A9：深度范围直接消费 CalculateCascadeBounds 输出的 zfar——
            // 公式唯一推导处在拟合函数内，Update 不再反推（防两处手抄漂移）。
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
                    ++full_update_call_count_[c];   // 诊断：整级重建次数
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
                    // 该级横向锚定步长（texel）：纯滚动要求"跨格位移恰为 ±B"（见下）
                    const int32_t band = static_cast<int32_t>(config_.cache_scroll_band_texels[c]);

                    if (shift_x == 0 && shift_y == 0)
                    {
                        // 未跨越整像素：完全命中缓存，0 绘制开销（相机不移动不更新）
                        ++hit_update_call_count_[c];
                        update_res.need_full_update = false;
                        update_res.ClearDirtyRects();
                    }
                    else if (band > 0 &&
                             (shift_x == 0 || std::abs(shift_x) == band) &&
                             (shift_y == 0 || std::abs(shift_y) == band))
                    {
                        // 环形滚动：横向锚定把缓存原点吸附到 L = B·texel 的整数倍格点上，
                        // 因此"跨格"必然让内容位移恰好 ±B texel —— 这正是可条带化的纯滚动。
                        // 旧内容在物理贴图里**原地继续有效**：偏移按跨格量累加补偿，读侧
                        // fract(shadow_uv + O·texel) 把旧内容映射回正确位置；本帧只需重画
                        // "新暴露"的 |shift| 宽条带。
                        // 位移不是整步（例：朝向变化 ⇒ 半径/texel 变 ⇒ 格点非等距跳动）或该级
                        // 没开横向锚定（B=0）⇒ 条带兜不住，走下面的整级重建分支。
                        // 偏移必须与写侧矩阵（light_view_draw）同帧落地：非零偏移下"整级重画"
                        // 反而会漏掉尾部 |O| 条带（光栅器没有环绕，内容落在 [O,1+O) 而被裁），
                        // 所以本分支走局部 scissor 路径。
                        Vector4u &offset = cache_states_[c].scroll_offset;
                        // 读侧 uv = 0.5 ± 0.5·ndc，两条轴的 ndc 都**反向**于 snapped 原点
                        // （cx/cy 增 ⇒ view 坐标减、且 V 轴向下 ⇒ 两轴同号）
                        // ⇒ 内容在 uv 上的位移 s = -shift，偏移补偿 O += shift。
                        offset.x = WrapTexelOffset(offset.x, shift_x, W);
                        offset.y = WrapTexelOffset(offset.y, shift_y, H);

                        // 新暴露条带在**物理**坐标下的起点：s>0(s<0) 缺的是低(高)端
                        //   start = (M - max(-s, 0) + O) mod M,  s = -shift
                        const uint32_t start_x = (shift_x > 0) ? WrapTexelOffset(offset.x, -shift_x, W)
                                                               : offset.x;
                        const uint32_t start_y = (shift_y > 0) ? WrapTexelOffset(offset.y, -shift_y, H)
                                                               : offset.y;

                        update_res.need_full_update = false;
                        update_res.ClearDirtyRects();
                        ++strip_update_call_count_[c];   // 诊断：条带滚动次数
                        if (shift_x != 0)
                            AppendWrappedStrip(update_res, start_x,
                                               static_cast<uint32_t>(std::abs(shift_x)), W, true);
                        if (shift_y != 0)
                            AppendWrappedStrip(update_res, start_y,
                                               static_cast<uint32_t>(std::abs(shift_y)), H, false);

                        cache_states_[c].snapped_origin = Vector2f(snapped_cx, snapped_cy);
                        cache_states_[c].texel_world_size = Vector2f(texel_size, texel_size);
                        cache_states_[c].valid_rect = Vector4u(0, 0, W, H);
                        along_anchor_[c] = along_anchor;
                        ++cache_states_[c].generation;
                    }
                    else
                    {
                        // 位移不是整步：整级重建 + 偏移清零（内容重画回未旋转的原点系）
                        ++full_update_call_count_[c];   // 诊断：整级重建次数
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
            // 写侧矩阵：非零环形偏移时把投射内容光栅化到**物理贴图**坐标系，使内容落在
            // 读侧 fract(uv + O·texel) 会去取的位置（偏移为 0 时与 light_view 逐位相同）。
            // x 取正、y 取负：把 y 取负是因为正交投影的 y 是反向的（OrthoMatrixReversedZ
            // 用 2/(bottom-top)、V 轴向下），而偏移量按 uv 定义。
            update_res.texel_world_size = texel_size;
            update_res.cache_offset = cache_states_[c].scroll_offset;
            if (update_res.cache_offset.x != 0 || update_res.cache_offset.y != 0)
            {
                update_res.light_view_draw =
                    TranslateMatrix(static_cast<float>(update_res.cache_offset.x) * texel_size,
                                    -static_cast<float>(update_res.cache_offset.y) * texel_size,
                                    0.0f) * light_view;
            }
            else
            {
                update_res.light_view_draw = light_view;
            }
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
            casc.cache_offset = update_res.cache_offset;
            casc.cache_valid_rect = Vector4u(0, 0, W, H);

            // 最近一帧更新形态（诊断/统计；示例 stats 与"整级 vs 条带"对拍消费）
            CascadeUpdateStats &st = update_stats_[c];
            st = CascadeUpdateStats{};
            st.full_update  = update_res.need_full_update;
            st.strip_count  = update_res.need_full_update ? 0u : update_res.dirty_rect_count;
            st.cache_hit    = !update_res.need_full_update && update_res.dirty_rect_count == 0;
            st.map_texels   = W * H;
            st.offset       = update_res.cache_offset;
            for (uint32_t r = 0; r < st.strip_count && r < 4; ++r)
                st.strip_texels += update_res.dirty_rects[r].width * update_res.dirty_rects[r].height;
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
