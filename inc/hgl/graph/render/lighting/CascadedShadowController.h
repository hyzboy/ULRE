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
        float sphere_radius = 0.0f;           // 该级联包围球半径（供上层换算 bias 的世界单位）
        float depth_range = 0.0f;             // 该级联正交投影的深度范围（zfar - znear，米）
        float resolved_bias = 0.0f;           // 该级联实际写入 UBO 的归一化 bias

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
        bool c0_dynamic_overlay = false;  // CSM 0 仅作为近景动态阴影层，静态阴影由 CSM 1..N 从 near_z 起覆盖
        float shadow_map_size = 1024.0f;  // 贴图边长（默认 1024x1024）
        float caster_depth_margin = 100.0f; // 光源视锥沿 -Z 延伸余量（容纳视锥外背向光源的投影物）
        float bias = 0.002f;
        // ── 逐级联 bias ──────────────────────────────────────────────────────────
        // bias 是**归一化深度**偏移，它的世界效果 = bias × 该级联的深度范围。而各级联
        // 的深度范围差异极大（默认配置下近距级联约 376m、超远距级联约 976m），同一个
        // bias 在远景级联上会产生 2.6 倍的世界偏移：按近景调好的"贴合"取值套到远景
        // 就变成"半影光晕"，反之远景调好了近景又会漏光。
        //
        // 两种修正方式（按需选一）：
        //   1) bias_world != 0：把 bias 解释为**世界单位偏移（米）**，逐级联按各自
        //      深度范围自动换算（bias_c = bias_world / depth_range_c）。全场景的世界
        //      偏移恒定，只剩一个直觉旋钮，推荐用法。
        //   2) per_cascade_bias_scale[c]：直接给每级一个乘数，用于精细微调（默认全 1，
        //      即历史行为）。bias_world 非 0 时本项被忽略。
        // 解析结果见 CascadeUpdateResult::resolved_bias / depth_range。
        float bias_world = 0.0f;
        float per_cascade_bias_scale[kMaxShadowCascades] = { 1.0f, 1.0f, 1.0f, 1.0f };
        // ── Normal-offset shadow mapping ──────────────────────────────────────
        // 接收者沿**几何法线**外推后再去采样阴影（世界单位米，0 = 关闭）。外推量
        // 按 tan(θ) 随入射角放大（θ = 法线与光线夹角），因此只推斜射面、正面几乎
        // 不动 —— 这正是固定深度 bias 做不到的部分：bias 与角度无关，为掠射面调大
        // 就会让所有正面一起漏光（peter-panning）。
        //
        // 与 bias 的关系：两者互补而非替代。纯深度 bias 是为了压 acne 而不得不让
        // 阴影整体贴着遮挡体（甚至略微膨胀）；法线偏移把这份"不得不"卸掉大半，
        // 于是 bias_world 可以往回收。调参顺序：先把法线偏移调到掠射面无 acne，
        // 再把 |bias_world| 收小到接触点刚好不漏光。
        //
        // 该值写入 ShadowCascadeInfo::shadow_params.w（原本保留未用，std140 ABI
        // 不变），每级写同一个值，shader 读级联 0。shader 侧还有编译期宏
        // HGL_SHADOW_NORMAL_OFFSET 作为总开关（见 ShaderLibrary/shadow/pcf_shadow.glsl）。
        float normal_offset_world = 0.0f;
        float pcf_radius = 1.5f;
        float darkness = 0.12f;
        float blend_width = 0.05f;        // 静态链末级(max_distance 边缘)淡出带宽 + 动态层级联 0 的边界淡出带宽（占本级深度区间的比例）
        // 相邻级联交界带的宽度（世界单位米）。>0 时交界带内近级与远级取暗叠加(min)，
        // 近级始终整强度参与、不做淡出 —— 交界处表现为"叠加"而不是"近级被远级顶替"。
        // 该带必须很小（1~2m）：远级贴图更粗（PCF 半径折算到世界也更大），
        // 带宽一大就会看到"近景换成远景贴图"的替代感。
        // 设为 0 表示硬切换（交界处不采样相邻级联）。
        float blend_distance = 1.5f;
        // 滚动缓存的沿光轴深度锚定步长（米）。>0 时把包围球中心沿光轴吸附到 step 的
        // 整数倍，保证静态缓存的深度矩阵在 step 内恒定；跨步时整级联重建一次。
        // 设为 0 表示禁用锚定（仅用于测试/调试：此时缓存深度会随相机连续漂移）。
        float cache_anchor_step = 16.0f;
        // 滚动缓存的横向锚定步长（米）。>0 时把静态级联（CSM 1..N）的包围球中心在
        // 光源的 right/up 两轴上粗粒度吸附，使相机横向移动 step 内不产生 texel 位移，
        // 从而避免"每跨越 1 个 texel 就整级重绘"。
        // 代价：中心最多偏离真实中心 step*0.707，必须把包围球半径扩大同样的量以保住
        // 视锥覆盖率，等价于静态级联纹素精度下降（step=2 时近距级联约 -12%）。
        // 设为 0 表示禁用（横向每跨 texel 即整级重绘）。
        float cache_lateral_anchor_step = 2.0f;
    };

    /**
     * 静态方向光 CSM 滚动缓存控制器。
     *
     * 负责：
     * 1. 级联划分（Split 距离计算）；
     * 2. 光空间视锥拟合、定向投影构建及 Texel Snapping 消除阴影抖动；
     * 3. 级联 0（近景）逐帧全量更新判定；
     * 4. 级联 1..N（中远景静态缓存）的锚定与失效判定：横向/沿光轴粗锚定 + texel 位移检测；
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
                                    float &out_along_anchor,
                                    float lateral_step = 0.0f) const;
    };
}
