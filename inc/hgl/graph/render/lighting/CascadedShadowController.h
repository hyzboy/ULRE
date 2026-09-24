#pragma once

#include <hgl/math/Vector.h>
#include <hgl/math/Matrix.h>
#include <hgl/graph/ubo/ShadowInfo.h>
#include <hgl/graph/camera/Camera.h>

namespace hgl::graph
{
    using namespace math;

    /**
     * 阴影贴图物理脏矩形（像素坐标）。
     * 用于 scissor / clear 和增量绘制区域设置。
     */
    struct ShadowDirtyRect
    {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    /**
     * 单个级联的更新决策与绘制参数。
     * 固定容量脏矩形池（滚动更新最多产生 4 个无重叠矩形），保持 POD 内存特性，避免逐帧堆分配。
     */
    struct CascadeUpdateResult
    {
        uint32_t cascade_index = 0;
        bool need_full_update = false;    // 是否需要全量重绘（如近景级联0、首次生成、或移动超限）
        bool is_static_cache = false;     // 是否为中远景静态滚动缓存
        int32_t texel_shift_x = 0;        // 本帧光空间位移（texel）
        int32_t texel_shift_y = 0;        // 本帧光空间位移（texel）

        Matrix4f light_view = Matrix4f(1.0f); // 当前级联对应的光空间视图矩阵
        Matrix4f light_proj = Matrix4f(1.0f); // 当前级联对应的正交投影矩阵

        uint32_t dirty_rect_count = 0;
        ShadowDirtyRect dirty_rects[4];

        void ClearDirtyRects() { dirty_rect_count = 0; }
        void AddDirtyRect(const ShadowDirtyRect &r)
        {
            if (dirty_rect_count < 4)
                dirty_rects[dirty_rect_count++] = r;
        }
    };

    static_assert(std::is_trivially_copyable_v<CascadeUpdateResult>, "CascadeUpdateResult must be trivially copyable");

    /**
     * 级联阴影配置参数。
     */
    struct CascadedShadowConfig
    {
        uint32_t cascade_count = 4;       // 级联数（1..kMaxShadowCascades）
        float split_lambda = 0.85f;       // Practical split 权重 (0=纯线性, 1=纯对数)
        float max_distance = 250.0f;      // 阴影最远裁剪距离
        float split_distances[kMaxShadowCascades] = { 15.0f, 45.0f, 100.0f, 250.0f }; // 自定义切分距离
        bool use_custom_splits = true;    // 是否优先使用自定义切分距离
        float shadow_map_size = 1024.0f;  // 贴图边长（默认 1024x1024）
        float caster_depth_margin = 100.0f; // 光源视锥沿 -Z 延伸余量（容纳视锥外背向光源的投影物）
        float bias = 0.002f;
        float pcf_radius = 1.5f;
        float darkness = 0.12f;
        float blend_width = 0.05f;        // 级联边缘混合带宽（UV 空间比例）
        // 滚动缓存的沿光轴深度锚定步长（米）。>0 时把包围球中心沿光轴吸附到 step 的
        // 整数倍，保证静态缓存的深度矩阵在 step 内恒定；跨步时整级联重建一次。
        // 设为 0 表示禁用锚定（仅用于测试/调试：此时缓存深度会随相机连续漂移）。
        float cache_anchor_step = 16.0f;
    };

    /**
     * 静态方向光 CSM 滚动缓存控制器。
     *
     * 负责：
     * 1. 级联划分（Split 距离计算）；
     * 2. 光空间视锥拟合、定向投影构建及 Texel Snapping 消除阴影抖动；
     * 3. 级联 0（近景）逐帧全量更新判定；
     * 4. 级联 1..N（中远景静态缓存）环形寻址（Toroidal Clipmap）位移与 Dirty Rect 条带计算；
     * 5. 同步输出标准 ShadowInfo UBO 数据与 ShadowCascadeCacheState。
     */
    class CascadedShadowController
    {
    private:
        CascadedShadowConfig config_;
        ShadowCascadeCacheState cache_states_[kMaxShadowCascades];
        Vector4u cascade_textures_[kMaxShadowCascades];
        float along_anchor_[kMaxShadowCascades] = { 0.0f };
        uint32_t scene_revision_ = 0;

    public:
        CascadedShadowController();
        explicit CascadedShadowController(const CascadedShadowConfig &cfg);

        void SetConfig(const CascadedShadowConfig &cfg) { config_ = cfg; }
        const CascadedShadowConfig &GetConfig() const { return config_; }

        /** 设置指定级联绑定的 bindless 纹理句柄与 array layer */
        void SetCascadeTexture(uint32_t cascade_idx, uint32_t bindless_handle, uint32_t layer = 0);

        /** 通知场景静态物体发生变更，强制中远景级联失效并全量重写 */
        void InvalidateStaticCache();

        /** 获取特定级联的 CPU 缓存状态 */
        const ShadowCascadeCacheState &GetCacheState(uint32_t cascade_idx) const;

        /**
         * 逐帧计算级联矩阵与滚动缓存。
         *
         * @param main_cam 主相机（用于视锥切分与视线定位）
         * @param aspect 主相机视口宽高比 (width / height)
         * @param light_dir 光线方向（从光源指向场景，需归一化）
         * @param out_shadow_info 输出填充的 ShadowInfo UBO 数据
         * @param out_updates 各级级联的更新状态（全量/增量条带数组，大小至少为 kMaxShadowCascades）
         */
        void Update(const Camera &main_cam,
                    float aspect,
                    const Vector3f &light_dir,
                    ShadowInfo &out_shadow_info,
                    CascadeUpdateResult out_updates[kMaxShadowCascades]);

    private:
        void CalculateSplitDistances(float near_z, float far_z, float out_splits[kMaxShadowCascades]) const;

        void CalculateCascadeBounds(const Camera &main_cam,
                                    float aspect,
                                    float split_near,
                                    float split_far,
                                    const Vector3f &light_dir,
                                    Matrix4f &out_view,
                                    Matrix4f &out_proj,
                                    Vector4f &out_snapped_center,
                                    float &out_texel_size,
                                    float &out_along_anchor) const;
    };
}
