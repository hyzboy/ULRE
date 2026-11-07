/**
 * TestGPUModelExpansionParity —— 模板实体「逐部件展开」的 CPU 参考实现 vs 着色器口径（plan T5.2）。
 *
 * 展开侧（`ShaderLibrary/module/descriptor/ecs/entity_gpu_cull.glsl` 的
 * `expand_template_entity`）把一个模板实例拆成"每部件一个实例"，落到**共享的模板桶组**。
 * 本测试在 CPU 侧写一份等价实现，再用**手算期望表**对拍 —— 手算表是判据，
 * 镜像函数只是被执行体；两者不一致即着色器逻辑写错。
 *
 * 四组判据：
 *   A 矩阵合成顺序：世界阵 = 实例 L2W × 部件 local_matrix（左乘实例、右乘部件），
 *     即先部件本地、再实例位姿。顺序搞反，带旋转/缩放的实例会整片错位。
 *   B LOD 选级：屏幕占比 = r·cot(fovY/2)/dist；槽序细→粗；从最粗槽往回取第一个
 *     `ratio <= 阈值` 的可用槽；边界处 `ratio == 阈值` **命中**（着色器是 `>` 才跳过）。
 *   C 阴影代理 / 部件筛选 / 桶号：`桶号 = 组基址 + bucket_slot`；筛选顺序为
 *     部件可见位 → geometry_id → (阴影 pass) 可投影 → 槽号哨兵。
 *   D 源码契约：把上式钉在着色器文本上，防止后人"顺手"改成别的顺序/别的比较方向。
 *
 * `src/ecs/tests` 下没有图形设备（见 TestGPUModelTemplate 文件头），所以采用
 * "镜像 + 手算表 + 源码契约"三层，而不是真机对拍。
 */

#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/graph/ubo/ModelExpansionArena.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/math/Matrix.h>
#include <hgl/math/Vector.h>
#include <hgl/type/ValueArray.h>

namespace
{
    using namespace hgl;
    using namespace hgl::graph;
    using namespace hgl::graph::ssbo;
    using namespace hgl::math;

    int g_failed = 0;

    void Check(const bool ok, const char *what)
    {
        if (!ok)
        {
            ++g_failed;
            GLogError(u8"TestGPUModelExpansionParity 失败: %s", what);
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // A 组：矩阵合成顺序
    // ══════════════════════════════════════════════════════════════════════

    float ComponentDiff(const Vector4f &a, const Vector4f &b)
    {
        float m = 0.0f;

        for (int i = 0; i < 4; ++i)
        {
            const float d = fabsf(a[i] - b[i]);

            if (d > m)
                m = d;
        }

        return m;
    }

    bool TestMatrixOrder()
    {
        // 只保留**不可交换**的组合：纯平移 × 纯平移可交换，反序也分不出来，故不用
        struct
        {
            const char *name;
            Matrix4f    instance_l2w;
            Matrix4f    part_local;
        } kCases[] =
        {
            { "平移实例 + 旋转部件",
              TranslateMatrix(3.0f, -2.0f, 7.0f),
              AxisYRotate(0.9f) },

            { "旋转实例 + 平移部件",
              AxisZRotate(0.7f) * TranslateMatrix(3.0f, -2.0f, 7.0f),
              TranslateMatrix(1.0f, 2.0f, 0.0f) },

            { "非均匀缩放+旋转实例 + 缩放部件",
              TranslateMatrix(-1.5f, 4.0f, 2.0f) * AxisRotate(1.1f, Vector3f(0.0f, 0.0f, 1.0f)) *
                  ScaleMatrix(2.0f, 1.0f, 0.5f),
              ScaleMatrix(1.0f, 2.0f, 3.0f) },
        };

        static const Vector4f kProbes[] =
        {
            Vector4f(0.0f, 0.0f, 0.0f, 1.0f),
            Vector4f(1.0f, 2.0f, 3.0f, 1.0f),
            Vector4f(-1.0f, 0.5f, 2.0f, 1.0f),
        };

        bool ok = true;

        for (const auto &c : kCases)
        {
            // 着色器写法：先合成 M = 实例 L2W × 部件 local，再变换顶点
            const Matrix4f emitted = c.instance_l2w * c.part_local;

            float order_err       = 0.0f;    // 正确顺序误差（必须 ≈ 0）
            float wrong_order_err = 0.0f;    // 反序误差（必须明显 ≠ 0 ⇒ 本用例有分辨力）

            for (const auto &p : kProbes)
            {
                const Vector4f expect = c.instance_l2w * (c.part_local * p);   // 逐级变换 = 定义式
                const Vector4f got    = emitted * p;
                const Vector4f bad    = (c.part_local * c.instance_l2w) * p;

                const float e = ComponentDiff(expect, got);
                const float b = ComponentDiff(expect, bad);

                if (e > order_err)
                    order_err = e;

                if (b > wrong_order_err)
                    wrong_order_err = b;
            }

            if (order_err > 1e-5f)
            {
                ok = false;
                GLogError(u8"TestGPUModelExpansionParity 失败: %s —— 合成顺序误差 %.7f > 1e-5",
                          c.name, order_err);
            }

            if (wrong_order_err <= 1e-3f)
            {
                ok = false;
                GLogError(u8"TestGPUModelExpansionParity 失败: %s —— 反序差异仅 %.7f，"
                          u8"本用例分辨不出顺序错误（用例构造有问题）",
                          c.name, wrong_order_err);
            }
        }

        return ok;
    }

    // ══════════════════════════════════════════════════════════════════════
    // 展开镜像（逐行对应着色器 expand_template_entity）
    // ══════════════════════════════════════════════════════════════════════

    struct TemplateFixture
    {
        uint32_t                    id = kInvalidModelTemplateID;   ///< 测试夹具索引（GPU 侧即行号）
        ModelTemplateRow            row;
        ValueArray<ModelPartRow>    parts;                          ///< 本模板部件段
    };

    struct DrawRec
    {
        uint32_t bucket      = 0;
        uint32_t geometry_id = 0;
        uint32_t material_id = 0;

        bool Equals(const DrawRec &o) const
        {
            return bucket == o.bucket && geometry_id == o.geometry_id && material_id == o.material_id;
        }
    };

    const TemplateFixture *ByID(const TemplateFixture *tpls, const int tpl_count, const uint32_t id)
    {
        if (id == kInvalidModelTemplateID || !tpls)
            return nullptr;

        for (int i = 0; i < tpl_count; ++i)
            if (tpls[i].id == id)
                return &tpls[i];

        return nullptr;
    }

    /// 模板可用性：与着色器一致 —— 空壳模板（无部件/无部件段）不参与换级与代理
    bool TemplateUsable(const TemplateFixture *tpl)
    {
        return tpl && tpl->row.part_count != 0u && tpl->row.part_offset != 0u;
    }

    /// LOD 选级镜像（对应着色器 select_lod_template_id）
    uint32_t SelectLod(const TemplateFixture *tpls, const int tpl_count,
                       const uint32_t base_id,
                       const ModelTemplateRow &base_tpl,
                       const float ratio)
    {
        if ((base_tpl.flags & kModelTemplateFlagLodEnabled) == 0u)
            return base_id;

        // 槽序：0 最细 … N-1 最粗；从最粗槽往回扫，取第一个命中的可用槽
        for (uint32_t i = kModelLodProxyCount; i > 0u; --i)
        {
            const uint32_t slot = i - 1u;
            const uint32_t pid  = base_tpl.lod_proxy_template_ids[slot];

            if (pid == kInvalidModelTemplateID || base_tpl.lod_screen_ratios[slot] <= 0.0f)
                continue;

            if (ratio > base_tpl.lod_screen_ratios[slot])
                continue;       // 屏幕占比仍大于该级上限 ⇒ 该级太粗，看更细的

            if (!TemplateUsable(ByID(tpls, tpl_count, pid)))
                continue;

            return pid;
        }

        return base_id;
    }

    struct ExpandArgs
    {
        uint32_t blk             = 0;        ///< 0 = 直渲 pass，非 0 = 阴影等附属 pass
        uint32_t template_id     = 0;
        uint64_t part_mask       = 0;
        uint32_t blk_material_id = 0;        ///< 批次/pass 级材质（部件 material_id==0 时继承）
        uint32_t bucket_base     = 0;        ///< 组基址（CPU 分配的模板桶组起点）
        float    lod_ratio       = 0.0f;     ///< 屏幕占比（公式见 A 组/源码契约）
        bool     has_bound       = true;     ///< 模板是否填了包围体
    };

    /// 整个展开过程的镜像；out 为本实体在本 pass 实际落下的实例列表
    void ExpandEntity(const TemplateFixture *tpls, const int tpl_count,
                      const ExpandArgs &a,
                      ValueArray<DrawRec> &out)
    {
        out.Clear();

        // 本批没分配到桶组 ⇒ 一个实例都不画（不猜默认值）
        if (a.bucket_base == kModelExpansionInvalidBucketBase)
            return;

        const TemplateFixture *body = ByID(tpls, tpl_count, a.template_id);

        if (!TemplateUsable(body))
            return;

        const TemplateFixture *tpl = body;

        // LOD 换级：仅直渲 pass 且模板有包围体（无包围体算不出占比）
        if (a.blk == 0u && a.has_bound)
        {
            const uint32_t lod_id = SelectLod(tpls, tpl_count, a.template_id, body->row, a.lod_ratio);

            if (lod_id != a.template_id)
                tpl = ByID(tpls, tpl_count, lod_id);
        }

        // 阴影代理：非直渲 pass 且模板登记了代理 ⇒ 整棵子树换成代理部件
        if (a.blk != 0u
            && (tpl->row.flags & kModelTemplateFlagShadowProxy) != 0u
            && tpl->row.shadow_proxy_template_id != kInvalidModelTemplateID)
        {
            const TemplateFixture *proxy = ByID(tpls, tpl_count, tpl->row.shadow_proxy_template_id);

            if (TemplateUsable(proxy))
                tpl = proxy;
        }

        if (!tpl)
            return;

        for (uint32_t p = 0u; p < tpl->row.part_count; ++p)
        {
            if (((a.part_mask >> uint64_t(p)) & uint64_t(1)) == uint64_t(0))
                continue;       // 部件可见位关闭

            if (p >= uint32_t(tpl->parts.GetCount()))
                break;          // 夹具数据不足（防止越界；正常数据不会走到）

            const ModelPartRow &part = tpl->parts[int(p)];

            if (part.geometry_id == 0u)
                continue;       // 空部件（行 0 哨兵惯例）

            if (a.blk != 0u && (part.part_flags & kModelPartFlagCanCastShadow) == 0u)
                continue;       // 阴影 pass 只投可投影部件

            if (part.bucket_slot == kModelPartBucketSlotNone)
                continue;       // 无槽部件（CPU 侧已标无几何）

            DrawRec rec;
            rec.bucket      = a.bucket_base + part.bucket_slot;
            rec.geometry_id = part.geometry_id;
            rec.material_id = (part.material_id != 0u) ? part.material_id : a.blk_material_id;

            out.Add(rec);
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // 夹具
    // ══════════════════════════════════════════════════════════════════════

    void MakePart(ModelPartRow &part, const uint32_t geo, const uint32_t mat,
                  const uint32_t flags, const uint32_t slot)
    {
        part.local_matrix       = TranslateMatrix(float(geo) * 0.01f, 0.0f, 0.0f);
        part.aabb_center_radius = Vector4f(0.0f, 0.0f, 0.0f, -1.0f);    // w<0 = 始终可见哨兵
        part.aabb_extents       = Vector4f(1.0f, 1.0f, 1.0f, 0.0f);
        part.geometry_id        = geo;
        part.material_id        = mat;
        part.part_flags         = flags;
        part.bucket_slot        = (geo == 0u) ? kModelPartBucketSlotNone : slot;
    }

    void SetBound(ModelTemplateRow &row, const float radius)
    {
        row.local_bound_center_radius = Vector4f(0.0f, 0.0f, 0.0f, radius);
        row.local_bound_extents       = Vector4f(radius, radius, radius, 0.0f);
    }

    /**
     * 夹具：
     *   id 1 = 本体，4 个部件、2 个桶槽
     *         P0 geo=10 mat=0 （继承批次材质） slot=0 可投影+不透明
     *         P1 geo=20 mat=77（自带材质）     slot=1 仅不透明
     *         P2 geo=0  （空部件）             slot=哨兵
     *         P3 geo=20 mat=0                  slot=1 仅可投影（与 P1 共槽）
     *   id 2 = LOD1 代理，1 个部件 geo=30
     *   id 3 = 阴影代理，  1 个部件 geo=40（可投影）
     */

    /// 夹具模板数（本体 + LOD 代理 + 阴影代理）。用固定数组而非 ValueArray：
    /// TemplateFixture 内含 ValueArray 成员，不是 trivially copyable。
    constexpr int kFixtureCount = 3;

    int BuildFixture(TemplateFixture *tpls)
    {
        {
            TemplateFixture &body = tpls[0];
            body.id = 1u;
            body.row.part_offset       = 1u;
            body.row.part_count        = 4u;
            body.row.part_bucket_count = 2u;
            SetBound(body.row, 1.0f);

            ModelPartRow p0, p1, p2, p3;
            MakePart(p0, 10u, 0u,  kModelPartFlagCanCastShadow | kModelPartFlagIsOpaque, 0u);
            MakePart(p1, 20u, 77u, kModelPartFlagIsOpaque, 1u);
            MakePart(p2, 0u,  0u,  0u, 0u);
            MakePart(p3, 20u, 0u,  kModelPartFlagCanCastShadow, 1u);

            body.parts.Add(p0);
            body.parts.Add(p1);
            body.parts.Add(p2);
            body.parts.Add(p3);
        }

        {
            TemplateFixture &lod = tpls[1];
            lod.id = 2u;
            lod.row.part_offset       = 10u;
            lod.row.part_count        = 1u;
            lod.row.part_bucket_count = 2u;
            SetBound(lod.row, 0.5f);

            ModelPartRow r0;
            MakePart(r0, 30u, 0u, kModelPartFlagCanCastShadow, 0u);
            lod.parts.Add(r0);
        }

        {
            TemplateFixture &shadow = tpls[2];
            shadow.id = 3u;
            shadow.row.part_offset       = 20u;
            shadow.row.part_count        = 1u;
            shadow.row.part_bucket_count = 2u;
            SetBound(shadow.row, 0.2f);

            ModelPartRow s0;
            MakePart(s0, 40u, 0u, kModelPartFlagCanCastShadow, 0u);
            shadow.parts.Add(s0);
        }

        if (tpls[0].parts.GetCount() != 4 || tpls[1].parts.GetCount() != 1 || tpls[2].parts.GetCount() != 1)
            return 0;

        return kFixtureCount;
    }

    // ══════════════════════════════════════════════════════════════════════
    // B 组：LOD 选级手算表
    // ══════════════════════════════════════════════════════════════════════

    bool TestLodSelection()
    {
        TemplateFixture tpls[kFixtureCount];

        if (BuildFixture(tpls) != kFixtureCount)
        {
            GLogError(u8"TestGPUModelExpansionParity: 夹具构造失败");
            return false;
        }

        // 本体挂三个代理槽（阈值取 2 的负幂 ⇒ 手算值可精确表示）：
        //   槽 0 阈值 0.5   ⇒ id 2（最细代理）
        //   槽 1 阈值 0.25  ⇒ id 2（同模板复用：用于验证扫描顺序/回落）
        //   槽 2 阈值 0.125 ⇒ id 0（该级未登记）
        ModelTemplateRow body = tpls[0].row;
        body.flags                     = kModelTemplateFlagLodEnabled;
        body.lod_proxy_template_ids[0] = 2u;
        body.lod_proxy_template_ids[1] = 2u;
        body.lod_proxy_template_ids[2] = kInvalidModelTemplateID;
        body.lod_screen_ratios[0]      = 0.5f;
        body.lod_screen_ratios[1]      = 0.25f;
        body.lod_screen_ratios[2]      = 0.125f;

        struct
        {
            float       ratio;
            uint32_t    expect_id;
            const char *why;
        } kCases[] =
        {
            { 1.0f,   1u, "占比远大于最细阈值 ⇒ 本体" },
            { 0.51f,  1u, "略大于槽 0 阈值 ⇒ 本体" },
            { 0.5f,   2u, "占比 == 槽 0 阈值 ⇒ 命中（边界含等号）" },
            { 0.4f,   2u, "落在槽 0/槽 1 之间 ⇒ 细的槽 0 先命中" },
            { 0.25f,  2u, "占比 == 槽 1 阈值 ⇒ 命中槽 1" },
            { 0.125f, 2u, "占比 == 槽 2 阈值但该级未登记 ⇒ 回落到更细的可用槽" },
            { 0.01f,  2u, "占比极小 ⇒ 仍取最粗的**可用**槽（槽 2 不存在）" },
        };

        bool ok = true;

        for (const auto &c : kCases)
        {
            const uint32_t got = SelectLod(tpls, kFixtureCount, 1u, body, c.ratio);

            if (got == c.expect_id)
                continue;

            ok = false;
            GLogError(u8"TestGPUModelExpansionParity 失败: LOD ratio=%.4f 期望 id=%u 实得 %u（%s）",
                      c.ratio, c.expect_id, got, c.why);
        }

        // 未置 LOD 位 ⇒ 恒本体（改造前的行为，零回归面）
        ModelTemplateRow no_flag = body;
        no_flag.flags = 0u;

        if (SelectLod(tpls, kFixtureCount, 1u, no_flag, 0.001f) != 1u)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionParity 失败: 未置 LOD 位时必须恒选本体");
        }

        // 代理为空壳 ⇒ 跳过该级
        const uint32_t saved = tpls[1].row.part_count;
        tpls[1].row.part_count = 0u;

        if (SelectLod(tpls, kFixtureCount, 1u, body, 0.1f) != 1u)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionParity 失败: 空壳代理模板必须被跳过");
        }

        tpls[1].row.part_count = saved;

        return ok;
    }

    // ══════════════════════════════════════════════════════════════════════
    // C 组：阴影代理 / 部件筛选 / 桶号手算表
    // ══════════════════════════════════════════════════════════════════════

    bool ExpectDraws(const ValueArray<DrawRec> &got, const DrawRec *expect, const int expect_count,
                     const char *what)
    {
        bool ok = true;

        if (got.GetCount() != expect_count)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionParity 失败: %s —— 实例数 %d 期望 %d",
                      what, got.GetCount(), expect_count);
        }

        const int n = (got.GetCount() < expect_count) ? got.GetCount() : expect_count;

        for (int i = 0; i < n; ++i)
        {
            if (got[i].Equals(expect[i]))
                continue;

            ok = false;
            GLogError(u8"TestGPUModelExpansionParity 失败: %s —— 第 %d 个实例 "
                      u8"(桶 %u/geo %u/mat %u) 期望 (桶 %u/geo %u/mat %u)",
                      what, i,
                      got[i].bucket, got[i].geometry_id, got[i].material_id,
                      expect[i].bucket, expect[i].geometry_id, expect[i].material_id);
        }

        return ok;
    }

    bool TestExpansionTable()
    {
        TemplateFixture tpls[kFixtureCount];

        if (BuildFixture(tpls) != kFixtureCount)
        {
            GLogError(u8"TestGPUModelExpansionParity: 夹具构造失败");
            return false;
        }

        const uint32_t kBase  = 100u;   // CPU 分配的组基址
        const uint32_t kMatId = 55u;    // 批次/pass 级材质
        const float    kNear  = 1.0f;   // 占比很大 ⇒ 不换 LOD 级

        ValueArray<DrawRec> got;
        ExpandArgs a;

        a.template_id     = 1u;
        a.blk_material_id = kMatId;
        a.bucket_base     = kBase;
        a.lod_ratio       = kNear;
        a.has_bound       = true;

        bool ok = true;

        // 手算表（直渲 pass，部件全可见）：
        //   P0 ⇒ 桶 100 / geo 10 / 材质继承 55
        //   P1 ⇒ 桶 101 / geo 20 / 自带 77
        //   P2 ⇒ 跳过（geo 0）
        //   P3 ⇒ 桶 101 / geo 20 / 继承 55（与 P1 同槽 ⇒ 同桶两条实例）
        const DrawRec kE1[] =
        {
            { kBase + 0u, 10u, kMatId },
            { kBase + 1u, 20u, 77u },
            { kBase + 1u, 20u, kMatId },
        };

        // 1) 直渲 pass，部件全可见
        a.blk       = 0u;
        a.part_mask = 0xFu;
        ExpandEntity(tpls, kFixtureCount, a, got);
        ok = ExpectDraws(got, kE1, 3, "直渲 pass 全可见 ⇒ 跳空部件，槽 1 落两条实例") && ok;

        // 2) 只开空部件可见位 ⇒ 零实例
        a.part_mask = 0x4u;
        ExpandEntity(tpls, kFixtureCount, a, got);
        ok = ExpectDraws(got, nullptr, 0, "部件可见位全关 ⇒ 零实例") && ok;

        // 3) 只开第一个部件 ⇒ 一个实例
        a.part_mask = 0x1u;
        ExpandEntity(tpls, kFixtureCount, a, got);
        ok = ExpectDraws(got, kE1, 1, "只开部件 0 ⇒ 一个实例") && ok;

        // 4) 阴影 pass，未登记阴影代理 ⇒ 只投可投影部件（P1 无 CanCastShadow 位）
        const DrawRec kE4[] =
        {
            { kBase + 0u, 10u, kMatId },
            { kBase + 1u, 20u, kMatId },
        };

        a.blk       = 1u;
        a.part_mask = 0xFu;
        ExpandEntity(tpls, kFixtureCount, a, got);
        ok = ExpectDraws(got, kE4, 2, "阴影 pass 无代理 ⇒ 只有可投影部件") && ok;

        // 5) 阴影 pass，登记阴影代理（整栋楼 ⇒ 一个 Cube）⇒ 只画代理部件，组基址不变
        const DrawRec kE5[] = { { kBase + 0u, 40u, kMatId } };

        tpls[0].row.flags |= kModelTemplateFlagShadowProxy;
        tpls[0].row.shadow_proxy_template_id = 3u;

        ExpandEntity(tpls, kFixtureCount, a, got);
        ok = ExpectDraws(got, kE5, 1, "阴影 pass 有代理 ⇒ 只画代理部件（沿用同一组基址）") && ok;

        // 6) 直渲 pass 不受阴影代理影响
        a.blk = 0u;
        ExpandEntity(tpls, kFixtureCount, a, got);
        ok = ExpectDraws(got, kE1, 3, "直渲 pass 不受阴影代理影响") && ok;

        // 7) 开启 LOD（槽 0 ⇒ id 2），占比 0.1 < 阈值 0.5 ⇒ 换成代理模板的部件
        const DrawRec kE7[] = { { kBase + 0u, 30u, kMatId } };

        tpls[0].row.flags |= kModelTemplateFlagLodEnabled;
        tpls[0].row.lod_proxy_template_ids[0] = 2u;
        tpls[0].row.lod_screen_ratios[0]      = 0.5f;

        a.lod_ratio = 0.1f;
        ExpandEntity(tpls, kFixtureCount, a, got);
        ok = ExpectDraws(got, kE7, 1, "占比低于阈值 ⇒ 换成 LOD 代理的部件") && ok;

        // 8) 模板无包围体 ⇒ 算不出占比 ⇒ 不换级（退回本体）
        a.has_bound = false;
        ExpandEntity(tpls, kFixtureCount, a, got);
        ok = ExpectDraws(got, kE1, 3, "无包围体 ⇒ 不做 LOD 换级，画本体") && ok;
        a.has_bound = true;

        // 9) 阴影 pass 不参与 LOD 换级：仍走阴影代理
        a.blk = 1u;
        ExpandEntity(tpls, kFixtureCount, a, got);
        ok = ExpectDraws(got, kE5, 1, "阴影 pass 不参与 LOD 换级") && ok;
        a.blk = 0u;

        // 10) 桶组基址为哨兵 ⇒ 一个实例都不许落（CPU 未接管本批）
        const uint32_t saved_base = a.bucket_base;
        a.bucket_base = kModelExpansionInvalidBucketBase;
        ExpandEntity(tpls, kFixtureCount, a, got);
        ok = ExpectDraws(got, nullptr, 0, "组基址为哨兵 ⇒ 不画，不猜默认值") && ok;
        a.bucket_base = saved_base;
        a.lod_ratio   = kNear;

        // 11) 模板无部件 ⇒ 零实例
        const uint32_t saved_count = tpls[0].row.part_count;
        tpls[0].row.part_count = 0u;
        ExpandEntity(tpls, kFixtureCount, a, got);
        ok = ExpectDraws(got, nullptr, 0, "空模板 ⇒ 零实例") && ok;
        tpls[0].row.part_count = saved_count;

        return ok;
    }

    // ══════════════════════════════════════════════════════════════════════
    // D 组：着色器源码契约
    // ══════════════════════════════════════════════════════════════════════

    bool ReadSource(const OSString &path, AnsiString &out_text)
    {
        hgl::io::OpenFileInputStream fis(path);

        if (!fis || fis->GetSize() <= 0)
            return false;

        char chunk[4096];
        int64 got;

        while ((got = fis->Read(chunk, int64(sizeof(chunk)))) > 0)
            out_text.Strcat(chunk, int(got));

        return true;
    }

    bool ContainsToken(const AnsiString &src, const char *needle)
    {
        return strstr(src.c_str(), needle) != nullptr;
    }

    bool TestSourceContracts()
    {
        AnsiString cull_src;

        if (!ReadSource(OS_TEXT("ShaderLibrary/module/descriptor/ecs/entity_gpu_cull.glsl"), cull_src))
        {
            GLogError(u8"TestGPUModelExpansionParity: 无法读取着色器源（需在仓库根目录运行）");
            return false;
        }

        const struct
        {
            const char *needle;
            const char *why;
        } kChecks[] =
        {
            { "const mat4 part_l2w = root_l2w * part.local_matrix;",
              "世界阵必须 = 实例 L2W × 部件 local_matrix（左实例、右部件）" },

            { "const float cot_half_fov = max(abs(camera.projection[1][1]), 1e-4);",
              "cot(fovY/2) 必须取 |projection[1][1]|（RH_ZO 下 m11 为负）" },
            { "const float ratio = world_radius * cot_half_fov / dist;",
              "屏幕占比公式必须是 r·cot(fovY/2)/dist" },
            { "if (ratio > base_tpl.lod_screen_ratios[slot])",
              "LOD 判定必须是 ratio **严格大于**阈值才跳过 ⇒ 边界含等号" },
            { "for (uint i = uint(MODEL_LOD_PROXY_COUNT); i > 0u; --i)",
              "LOD 槽必须从最粗往回扫（细槽优先命中）" },

            { "if (blk == 0u && has_bound)",
              "LOD 换级只在直渲 pass 且有包围体时生效" },
            { "(tpl.flags & MODEL_TEMPLATE_FLAG_SHADOW_PROXY) != 0u",
              "阴影代理必须由 flags 位把关" },
            { "if (blk != 0u && (tpl.flags & MODEL_TEMPLATE_FLAG_SHADOW_PROXY) != 0u &&",
              "阴影代理只在非直渲 pass 生效" },

            { "if (((entity.model_part_mask >> uint64_t(p)) & uint64_t(1)) == uint64_t(0))",
              "部件可见位必须按位号 p 取" },
            { "if (part.geometry_id == 0u)",
              "无几何部件必须跳过" },
            { "if (blk != 0u && (part.part_flags & 1u) == 0u)",
              "阴影 pass 必须只投可投影部件（kModelPartFlagCanCastShadow）" },
            { "if (part.bucket_slot == 0xFFFFFFFFu)",
              "无槽部件（bucket_slot 哨兵）必须跳过，不得落到组基址" },
            { "const uint bucket = tag_bucket_base + part.bucket_slot;",
              "桶号必须是组基址 + 模板内槽号，不得跨组重排" },
            { "if (blk == 0u && part.aabb_center_radius.w >= 0.0)",
              "部件级细剔除只在直渲 pass 生效（阴影 pass 不做，避免漏阴影）" },
            { "(part.material_id != 0u) ? part.material_id : ids_blk.material_id",
              "部件材质优先，material_id==0 表示继承批次/pass 级材质" },
        };

        bool ok = true;

        for (const auto &c : kChecks)
        {
            if (ContainsToken(cull_src, c.needle))
                continue;

            ok = false;
            GLogError(u8"TestGPUModelExpansionParity 失败: %s", c.why);
        }

        return ok;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestGPUModelExpansionParity"));

    Check(TestMatrixOrder(), "A 组：矩阵合成顺序");
    Check(TestLodSelection(), "B 组：LOD 选级手算表");
    Check(TestExpansionTable(), "C 组：阴影代理/部件筛选/桶号手算表");
    Check(TestSourceContracts(), "D 组：着色器源码契约");

    if (g_failed > 0)
    {
        GLogError(u8"=== TestGPUModelExpansionParity: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestGPUModelExpansionParity: 全部测试通过 ===");
    return 0;
}
