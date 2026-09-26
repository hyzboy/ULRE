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
     * 固定容量脏矩形池（环形滚动最多产生 4 个矩形：列/行条带各可跨贴图接缝一分为二；
     * 两轴同时跨格时角落有小面积重叠——重复绘制的是同值内容，无副作用），
     * 保持 POD 内存特性，避免逐帧堆分配。
     */
    struct CascadeUpdateResult
    {
        uint32_t cascade_index = 0;
        bool need_full_update = false;    // 是否需要全量重绘（级联0、首次生成、场景/深度锚点失效、或位移不是整步滚动）
        bool is_static_cache = false;     // 是否为中远景静态滚动缓存

        Matrix4f light_view = Matrix4f(1.0f); // 未偏移的光空间视图矩阵（读侧 shadow_vp = light_proj*本矩阵）
        Matrix4f light_proj = Matrix4f(1.0f); // 当前级联对应的正交投影矩阵
        /// 写侧专用：把投射内容光栅化到**物理贴图**坐标系的光照 view。
        /// = light_view 左乘光空间平移 (cache_offset.xy * texel_world)，与读侧 shader 的
        /// `fract(shadow_uv + cache_offset * inv_shadow_map_size)` 落在同一坐标系。
        /// cache_offset == 0 时与 light_view 逐位相同（未滚动路径零变化）。
        /// ⚠ 非零偏移下"整级重画"并不覆盖整张贴图：光栅器没有环绕，内容落在
        /// [O, 1+O) 而被裁掉尾部 |O| 条带 ⇒ 尾部条带必须由 dirty_rects 单独补画。
        Matrix4f light_view_draw = Matrix4f(1.0f);
        /// 本帧该级联的环形偏移（texel，x/y 有效，z/w 保留=0）。与写进
        /// ShadowInfo.cascades[c].cache_offset 的值相同，供上层与契约测试查询。
        Vector4u cache_offset = Vector4u(0);
        /// 该级联每个纹素对应的世界尺寸（写侧平移换算量 / 步长 L = B * 本值）。
        float texel_world_size = 0.0f;
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
     * 最近一帧某级联的更新形态（诊断/统计；不含矩阵，保持 POD）。
     *
     * 判读：`strip_count == 0 && !full_update` ⇒ 本帧该级联零绘制（纯命中）；
     * `strip_count > 0` ⇒ 环形滚动条带，面积占比 = strip_texels / map_texels；
     * `full_update` ⇒ 整级重建（此时 offset 必为 0）。上层 stats / 增量对拍用。
     */
    struct CascadeUpdateStats
    {
        bool     full_update  = false;       // 本帧整级重建
        bool     cache_hit    = false;       // 本帧纯命中（零绘制）
        uint32_t strip_count  = 0;           // 本帧条带矩形数
        uint32_t strip_texels = 0;           // 本帧条带面积（texel）
        uint32_t map_texels   = 0;           // 整图面积（texel，作分母）
        Vector4u offset       = Vector4u(0); // 该级联环形偏移（texel）
    };

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
        /// 滚动缓存的横向锚定步长（**单位：纹素 texel**，逐级）。
        ///
        /// 为什么用 texel 而不是世界米：横向锚定格 L 同时是"环形偏移的量子"（必须是
        /// 整数 texel，`cache_offset` 是 uvec）与"半径补偿量"（决定静态阴影精度），
        /// 两者都只有按 shadowmap 侧表达才可跨分辨率/跨级联比较。世界步长由它派生：
        ///
        ///     L_c = 2·B·r0_c / (M − 1.416·B)        // r0 = 未补偿的包围球半径
        ///     radius += 0.708·L_c                   // ⇒ 最终 texel = L_c / B，恰为 B 个纹素
        ///     精度损失 = 0.708·L_c / r0_c = 1.416·B / (M − 1.416·B)（与 r0、分辨率都无关）
        ///
        /// 于是"锚定格恰为 B 个纹素"与"格内布局矩阵恒定"同时成立，且环形偏移天然整数。
        /// 代价表（M=1024，精确式）：B=8 → 1.1% / 16 → 2.3% / 32 → 4.6% / 64 → 9.7% /
        /// M/8 → 21.5%；B ≥ M/1.416 ⇒ 退化（代码 fail-safe 回"禁用锚定"）。
        /// 把 M 开到 4096 也仍是同样的百分比（B/M 决定一切）——"贴图更大就能用更大的
        /// 分数步长"不成立，大贴图的收益是同一个 B 对应更小的**世界**步长（滞后更小）。
        /// 逐级给值：近景级联要小（世界步长小 = 陈旧带窄 + 精度损失小），远景可大。
        /// 0 = 该级禁用横向锚定（每跨 texel 即整级重建，仅调试/测试用）。
        uint32_t cache_scroll_band_texels[kMaxShadowCascades] = { 0, 16, 16, 32 };
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
        CascadeUpdateStats update_stats_[kMaxShadowCascades];
        Vector4u cascade_textures_[kMaxShadowCascades];
        float along_anchor_[kMaxShadowCascades] = { 0.0f };
        uint32_t scene_revision_ = 0;

        // 诊断计数器（单调累计，免疫"每帧多次 Update 覆盖 update_stats_"盲点）：
        // 用于区分"失效未生效"与"已生效但被同帧后续 Update 覆盖"两种根因
        uint32_t update_call_count_ = 0;
        uint32_t invalidate_call_count_ = 0;
        uint32_t full_update_call_count_[kMaxShadowCascades] = {};
        uint32_t strip_update_call_count_[kMaxShadowCascades] = {};
        uint32_t hit_update_call_count_[kMaxShadowCascades] = {};

    public:
        CascadedShadowController();
        explicit CascadedShadowController(const CascadedShadowConfig &cfg);

        void SetConfig(const CascadedShadowConfig &cfg) { config_ = cfg; }
        const CascadedShadowConfig &GetConfig() const { return config_; }

        /** 设置指定级联绑定的 bindless 纹理句柄与 array layer */
        void SetCascadeTexture(uint32_t cascade_idx, uint32_t bindless_handle, uint32_t layer = 0);

        /** 通知场景静态物体发生变更，强制中远景级联失效并全量重写 */
        void InvalidateStaticCache();

        /** 诊断：Update() 累计调用次数（同帧多路径调用会 > 帧数） */
        uint32_t GetUpdateCallCount() const { return update_call_count_; }
        /** 诊断：InvalidateStaticCache() 累计调用次数（该实例上的真实生效次数） */
        uint32_t GetInvalidateCallCount() const { return invalidate_call_count_; }
        /** 诊断：某级"整级重建"累计次数（含被同帧后续 Update 覆盖的那些） */
        uint32_t GetFullUpdateCallCount(uint32_t cascade_idx) const
        { return cascade_idx < kMaxShadowCascades ? full_update_call_count_[cascade_idx] : 0u; }
        /** 诊断：某级"条带滚动"累计次数 */
        uint32_t GetStripUpdateCallCount(uint32_t cascade_idx) const
        { return cascade_idx < kMaxShadowCascades ? strip_update_call_count_[cascade_idx] : 0u; }
        /** 诊断：某级"完全命中"累计次数 */
        uint32_t GetHitUpdateCallCount(uint32_t cascade_idx) const
        { return cascade_idx < kMaxShadowCascades ? hit_update_call_count_[cascade_idx] : 0u; }

        /** 获取特定级联的 CPU 缓存状态 */
        const ShadowCascadeCacheState &GetCacheState(uint32_t cascade_idx) const;

        /** 最近一帧该级联的更新形态（诊断/统计，见 CascadeUpdateStats）。 */
        const CascadeUpdateStats &GetUpdateStats(uint32_t cascade_idx) const;

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

        /// band_texels：该级联的横向锚定步长（texel 数）。0 = 禁用锚定。
        /// 世界步长 L 与半径补偿由本函数内派生（见 .cpp 的推导注释）——调用方只给 texel 数。
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
                                    float &out_zfar,
                                    uint32_t band_texels = 0) const;
    };
}
