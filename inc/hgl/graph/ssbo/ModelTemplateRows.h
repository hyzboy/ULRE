#pragma once

#include <hgl/CoreType.h>
#include <hgl/graph/BufferAnnotation.h>
#include <hgl/math/Matrix.h>
#include <hgl/math/Vector.h>
#include <cstddef>
#include <cstdint>

namespace hgl::graph::ssbo
{
    using hgl::math::Matrix4f;
    using hgl::math::Vector4f;
    /**
     * ModelTemplateRow / ModelPartRow —— GPU 常驻「模型模板池」的两级行结构（POD + GLSL 镜像）。
     *
     * 设计意图（对应 plan T1/T3）：
     * - 一个 StaticMesh 模型（含内部 Geometry 节点树）打包成模板池的**一行** +
     *   部件池的**连续若干行**；此后场景里每多一个实例，只在 EntityGPU 表加一行引用
     *   （template_id），GPU 展开阶段按 template_id 找到部件区间逐个绘制，
     *   CPU 侧不再为每个实例 new 组件。
     * - 两池都是 GlobalSSBOBufferRegistry 的行池（与 GeometryAABB / LightRow 同族），
     *   着色器经 `GlobalAddresses.addr_model_template_pool / addr_model_part_pool`
     *   以 BDA 方式只读访问。
     *
     * 约定（与 EntityGPU / GeometryAABB 一致）：
     * - **行 0 是哨兵**：模板 id 0 = 「无模型 / 未解析」（LOD 代理位、阴影代理位填 0），
     *   池创建时 `reserve_rows = 1` 划出该行，永不分配给真实模板。
     * - `part_count == 0` 或 `part_offset == 0` 的模板行视为无效（展开时跳过）。
     * - 部件 `geometry_id == 0` = 无几何可画（未流送就绪 / 代理占位），展开时跳过。
     *
     * ⚠ 布局要点：两行结构都**恰为 128 字节**（alignas(16)，行号→字节偏移 = id<<7）。
     *   GLSL 镜像由 BufferGen 从本头生成（`buffer_gen.toml` 已登记），
     *   本文件是布局的**唯一真源**；改字段必须重跑 codegen，禁止手改 .glsl。
     */

    /// 模板池行 0：非法/未解析哨兵。
    constexpr uint32_t kInvalidModelTemplateID = 0;

    /// LOD 代理槽位数（级 1..3；级 0 = 原始模板本体，不占槽）。
    constexpr uint32_t kModelLodProxyCount = 3;

    // ── LOD 级命名与导入导出标准（T4.1）───────────────────────────────────
    //
    // 级号 → 载体：
    //   级 0            = **本体**（`part_offset/part_count` 指向的那一段部件行）
    //   级 1..3         = `lod_proxy_template_ids[级号 - 1]` 指向的**独立模板**
    //
    // 代理是货真价实的模板（自有部件段、自有素材桶组）⇒ 展开侧"换级"只是换一个
    // template_id 再走同一条逐部件循环，没有任何第二条渲染路径。
    //
    // 阈值口径（`lod_screen_ratios[i]`）：该级代理的**屏幕占比上界**，0..1，
    //   1.0 ≈ 物体外接球恰好占满视口半高。判定 = 从**最粗槽往回**取第一个满足
    //   `ratio <= lod_screen_ratios[i]` 且 `lod_proxy_template_ids[i] != 0` 的级
    //   （槽序由细到粗 ⇒ 槽 0 最细、阈值最高）。
    //   要求 `lod_screen_ratios` **单调不增**（r1 >= r2 >= r3 > 0）；写 0 或 id 为 0
    //   ⇒ 该级不存在。全部不存在 ⇒ 恒选级 0（= 改造前行为，零回归面）。
    //
    // 导入导出标准（美术/DCC 侧约定，当前只在 CPU 侧落库，GPU 消费见 T4.1 空壳）：
    //   · 命名 = `<模型名>_LOD1` / `_LOD2` / `_LOD3` / `_SHADOWPROXY`（大小写不敏感）；
    //   · 每个代理与被代理本体**同局部坐标系、同单位、同朝向**（RH / Z-up），
    //     否则换级会跳位 —— 这是硬约束，不是建议；
    //   · 代理可以有自己的部件树（不必单部件），但**不得**再声明代理（禁止套娃：
    //     展开侧只做一跳，见 ModelTemplateManager::SetModelTemplateProxies 的校验）；
    //   · 阴影代理（`shadow_proxy_template_id`）独立于 LOD 级，可以是任一级的模板；
    //   · **一个代理模板只能归属一个本体**（不可被两个本体共用）。回填代理时本体与全部
    //     代理的部件 `bucket_slot` 会被统一重编号（按部件几何的首次出现序），代理共用
    //     ⇒ 后回填者改写前者的槽号 ⇒ 前者画错几何（且槽号不越界，不会报错）。
    constexpr uint32_t kModelLodLevelBody      = 0;                          ///< 级 0 = 本体
    constexpr uint32_t kModelLodLevelProxyBase = 1;                          ///< 级 1 = 第一个代理槽
    constexpr uint32_t kModelLodLevelCount     = 1 + kModelLodProxyCount;    ///< 级总数（含本体）

    // ── ModelTemplateRow.flags 位定义 ─────────────────────────────────────
    constexpr uint32_t kModelTemplateFlagUnused     = 0u;          // 占位：后续如 billboard / skinning 等
    constexpr uint32_t kModelTemplateFlagLodEnabled = 1u << 0;     ///< 允许展开侧消费 `lod_*` 表（T4.1）
    constexpr uint32_t kModelTemplateFlagShadowProxy= 1u << 1;     ///< `shadow_proxy_template_id` 已校验可用（T4.2）

    // ── ModelPartRow.part_flags 位定义 ──────────────────────────────────
    constexpr uint32_t kModelPartFlagCanCastShadow = 1u << 0; ///< 阴影 pass 可投影
    constexpr uint32_t kModelPartFlagIsOpaque      = 1u << 1; ///< 不透明（vs 透明分拣用）

    /// ModelPartRow.bucket_slot 的"无桶"哨兵（geometry_id == 0 的部件恒为此值）。
    constexpr uint32_t kModelPartBucketSlotNone = 0xFFFFFFFFu;

    /**
     * 模型模板行 —— 一个模型（含其 Geometry 节点树）在 VRAM 模板池里的一行。
     *
     * - `part_offset/part_count`：在 ModelPartRow 池中的**连续**区间
     *   （offset 从 1 起；区间内行序 = 打包序，见 ModelTemplateManager）。
     * - `local_bound_*`：模型**局部空间**（根节点 TRS 之前的原始包围盒）中心/半径与
     *   半长；实例化后由 GPU 用实例 L2W 变换做包围测试（plan T3.1 culling）。
     * - `lod_proxy_template_ids[i]` / `lod_screen_ratios[i]`：LOD 代理（T4.1）。代理本身
     *   也是模板 id；第 i 级（级号 i+1）在屏幕占比 ≤ `lod_screen_ratios[i]` 时启用。
     *   生效前提 = `flags & kModelTemplateFlagLodEnabled`；id=0 / 阈值=0 ⇒ 该级不存在。
     *   空表 ⇒ 恒用级 0（本体）。写入口 = `ModelTemplateManager::SetModelTemplateProxies`。
     * - `shadow_proxy_template_id`：阴影代理模板 id（如整栋楼 ≈ 一个 Cube）；生效前提 =
     *   `flags & kModelTemplateFlagShadowProxy`。0 或未置位 ⇒ 阴影 pass 直接渲染本体部件。
     *   展开侧只做一跳：代理自己的 `shadow_proxy_template_id` **不再被跟随**（防套娃）。
     * - `part_bucket_count`：本模板**部件素材桶数**（= 部件 `bucket_slot` 的最大值 + 1）。
     *   展开时 GPU 每个部件落一个实例到"模板素材桶组"的第 `bucket_slot` 个桶；
     *   组基址由 CPU 在装批时按 (批, 模板) 分配并写入展开竞技场的 per-实体表
     *   （见 `ModelExpansionArena::bucket_base_addr`）。本字段让 CPU 不必扫部件行
     *   就能知道要为本模板预留几个桶。
     *
     *   登记代理（`SetModelTemplateProxies`）时，本体与全部代理的 `bucket_slot` 会被
     *   **统一重编号**为按部件几何首次出现序，且**每个参与模板**的 `part_bucket_count`
     *   都被改写为该并集大小 —— 因为换级时部件仍落同一个桶组，槽号必须共用一套编号。
     *   无代理时该重编号与打包时编号等价（本体独占），是无代理路径的恒等变换。
     */
    struct alignas(16) HGL_SSBO_BDA ModelTemplateRow
    {
        uint32_t part_offset = 0;                        ///< 部件池起始行（0 = 无效）
        uint32_t part_count  = 0;                        ///< 部件行数（0 = 无效）

        uint32_t lod_proxy_template_ids[kModelLodProxyCount] = { 0u, 0u, 0u }; ///< LOD1..3 代理模板 id
        float    lod_screen_ratios[kModelLodProxyCount]      = { 0.0f, 0.0f, 0.0f }; ///< 启用阈值（屏幕占比）

        uint32_t shadow_proxy_template_id = 0;           ///< 阴影代理模板 id（0 = 用本体）
        uint32_t flags = 0;                              ///< kModelTemplateFlag*

        uint32_t part_bucket_count = 0;                  ///< 部件素材桶数（= max(bucket_slot)+1）
        uint32_t _pad0[1] = { 0u };                      ///< 对齐：下两个 vec4 落 16B 边界

        Vector4f local_bound_center_radius = Vector4f(0.0f, 0.0f, 0.0f, 0.0f); ///< xyz 中心，w 半径
        Vector4f local_bound_extents       = Vector4f(0.0f, 0.0f, 0.0f, 0.0f); ///< AABB 半长

        uint32_t _pad1[12] = { 0u };                     ///< 补足 128B 行
    };

    static_assert(sizeof(ModelTemplateRow) == 128, "ModelTemplateRow 必须恰为 128 字节");
    static_assert(alignof(ModelTemplateRow) == 16, "ModelTemplateRow 必须 alignas(16)");
    static_assert(offsetof(ModelTemplateRow, lod_proxy_template_ids) == 8,  "lod_proxy_template_ids @8");
    static_assert(offsetof(ModelTemplateRow, lod_screen_ratios) == 20,      "lod_screen_ratios @20");
    static_assert(offsetof(ModelTemplateRow, shadow_proxy_template_id) == 32, "shadow_proxy_template_id @32");
    static_assert(offsetof(ModelTemplateRow, part_bucket_count) == 40,      "part_bucket_count @40");
    static_assert(offsetof(ModelTemplateRow, local_bound_center_radius) == 48, "local_bound_center_radius @48");
    static_assert(offsetof(ModelTemplateRow, local_bound_extents) == 64,    "local_bound_extents @64");

    /**
     * 模型部件行 —— 模板内部一个可绘制叶子（StaticMesh 节点 × primitive）在 VRAM 里的一行。
     *
     * - `local_matrix`：部件相对**模型根**的复合局部位姿（打包时由节点 TRS 链推导；
     *   与 StaticMeshNode::localMatrix 同口径）。实例的世界阵 = 实例 L2W × local_matrix。
     * - `aabb_*`：部件在**自身局部空间**的包围盒（打包自 StaticMeshNode 的节点包围盒；
     *   半径为辅助快速球测，extents 供精确 AABB）。
     * - `geometry_id`：Geometry 池（MeshDrawParams 主表）行号，0 = 无几何跳过。
     *   口径与 EntityGPU 表的 4-ID（entity_render_ids.geometry）完全一致。
     * - `material_id`：材质数据行号（MaterialData 池）；0 = 未指定，
     *   展开时可回退到实例/批次材质（当前 collect 路径按批次材质，本字段先行占位）。
     * - `bucket_slot`：本部件在**模板素材桶组**内的槽号（同模板内按 `geometry_id`
     *   首次出现序去重编号；`geometry_id == 0` ⇒ `kModelPartBucketSlotNone`）。
     *   桶组由 CPU 在装批时按 (批, 模板) 分配，组基址写入展开竞技场的 per-实体表；
     *   展开时 `桶号 = 组基址 + bucket_slot`。本字段是"部件 ⇒ 素材桶"映射的
     *   **唯一真源**，与批无关、跨帧幂等。
     */
    struct alignas(16) HGL_SSBO_BDA ModelPartRow
    {
        Matrix4f local_matrix;                                                                             ///< 相对模型根的局部位姿
        Vector4f aabb_center_radius = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);                                     ///< xyz 中心，w 半径
        Vector4f aabb_extents       = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);                                     ///< AABB 半长

        uint32_t geometry_id = 0;                    ///< Geometry 池行号（0 = 无几何，展开跳过）
        uint32_t material_id = 0;                    ///< 材质数据行号（0 = 未指定）
        uint32_t part_flags  = 0;                    ///< kModelPartFlag*

        uint32_t bucket_slot = kModelPartBucketSlotNone; ///< 模板素材桶组内的槽号

        uint32_t _pad0[4] = { 0u };                  ///< 补足 128B 行
    };

    static_assert(sizeof(ModelPartRow) == 128, "ModelPartRow 必须恰为 128 字节");
    static_assert(alignof(ModelPartRow) == 16, "ModelPartRow 必须 alignas(16)");
    static_assert(offsetof(ModelPartRow, local_matrix) == 0,   "local_matrix @0");
    static_assert(offsetof(ModelPartRow, aabb_center_radius) == 64, "aabb_center_radius @64");
    static_assert(offsetof(ModelPartRow, aabb_extents) == 80,  "aabb_extents @80");
    static_assert(offsetof(ModelPartRow, geometry_id) == 96,   "geometry_id @96");
    static_assert(offsetof(ModelPartRow, material_id) == 100,  "material_id @100");
    static_assert(offsetof(ModelPartRow, part_flags) == 104,   "part_flags @104");
    static_assert(offsetof(ModelPartRow, bucket_slot) == 108,  "bucket_slot @108");
}
