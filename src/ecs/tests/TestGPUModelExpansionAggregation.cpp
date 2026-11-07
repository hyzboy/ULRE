/**
 * TestGPUModelExpansionAggregation —— 模板实例化的**聚合账**与 ICB 命令语义（plan T5.3）。
 *
 * 判据来自「50 块棋盘」这个典型场景：50 个实体引用**同一个**模型模板，每个模板内部有
 * 多个部件几何。要证明的不是"能画出来"（那要真机），而是**账算对了**：
 *
 *   A 桶组聚合：50 个实例共享 **一组**桶（= 模板的 `part_bucket_count`），
 *     桶数**不随实例数增长**；每桶容量 = 该槽每实例部件数 × 实例数。
 *   B ICB 语义：一桶一条命令（`cmd_index = 块基址 + 桶号`），命令条数恒 = 桶数；
 *     `groupCountX = ceil(元素数 / 组大小)`、`groupCountZ = 1`、`groupCountY = 桶内实例数`
 *     ⇒ 50 个实例落在**同一条**命令的 Y 轴上（这才是"实例化"而非"每实例一条命令"）。
 *   C 容量/行/溢出：容量算少了 → `slot >= capacity` → 丢实例并计入溢出槽（硬判据）；
 *     桶间实例行起点必须 4 行对齐（`row_base % 4 == 0`，见 PrimitiveBatchPipeline 注释）。
 *   D 源码契约：把 A/B/C 的口径钉在着色器与 CPU 装配的文本上。
 *
 * `src/ecs/tests` 下没有图形设备（见 TestGPUModelTemplate 文件头），故为
 * "镜像算术 + 手算表 + 源码契约"三层。
 */

#include <hgl/ecs/support/pipeline/PipelineMaterialRenderer.h>   // MeshDispatchGroupSize（组大小单一真源）
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/mtl/MeshShaderLimits.h>
#include <hgl/type/ValueArray.h>

namespace
{
    using namespace hgl;
    using namespace hgl::ecs;
    using namespace hgl::graph::ssbo;

    int g_failed = 0;

    void Check(const bool ok, const char *what)
    {
        if (!ok)
        {
            ++g_failed;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: %s", what);
        }
    }

    /// 模板内的"每实例部件分布"：只有落进桶槽的部件才占容量（空部件/无槽部件不占）
    struct TemplateAggregate
    {
        uint32_t                part_bucket_count = 0;
        ValueArray<uint32_t>    slot_part_count;    ///< 每槽、**每实例**的部件数
        ValueArray<uint32_t>    slot_geometry;      ///< 每槽的代表几何（桶的 geometry_id）
    };

    /**
     * 夹具：3 部件 / 2 桶槽
     *   P0 geo=10 slot=0
     *   P1 geo=11 slot=0     ← 与 P0 共槽 ⇒ 一桶两部件
     *   P2 geo=12 slot=1
     *   P3 geo=0  slot=哨兵  ← 空部件，不占容量
     */
    TemplateAggregate MakeChessboardAggregate()
    {
        TemplateAggregate agg;
        agg.part_bucket_count = 2u;

        agg.slot_part_count.Add(2u);    // slot 0：P0 + P1
        agg.slot_geometry.Add(10u);

        agg.slot_part_count.Add(1u);    // slot 1：P2
        agg.slot_geometry.Add(12u);

        return agg;
    }

    /// 本批桶组的装配结果（对应 PrimitiveBatchPipeline 的 bucket_capacity / row_cursor 循环）
    struct BucketGroupPlan
    {
        uint32_t                bucket_first    = 0;    ///< 桶号空间起点（SetModelBucketBase 用）
        uint32_t                bucket_count    = 0;
        ValueArray<uint32_t>    capacity;               ///< 每桶容量
        ValueArray<uint32_t>    row_base;               ///< 每桶实例行起点（竞技场相对行号）
        uint32_t                need_rows       = 0;    ///< 前置校验口径：Σ(1 + capacity)
        uint32_t                consumed_rows   = 0;    ///< 实际推进口径：Σ align4(capacity)
    };

    uint32_t Align4(const uint32_t v)
    {
        return (v + 3u) & ~3u;
    }

    /**
     * 按 CPU 装配逻辑为「一个模板 + N 个实例」规划桶组。
     *
     * 两个空间必须分清（这正是真实装配里的两个独立变量）：
     *   桶号空间 —— 每桶一个 `bucket_first + s`，供 `SetModelBucketBase` 写回实体；
     *   实例行空间 —— 竞技场按 append 推进的 `row_cursor`，与桶号无关。
     */
    void PlanTemplateGroup(const TemplateAggregate &agg, const uint32_t instance_count,
                           const uint32_t bucket_first, BucketGroupPlan &plan)
    {
        plan = BucketGroupPlan();
        plan.bucket_first = bucket_first;
        plan.bucket_count = agg.part_bucket_count;

        uint32_t row_cursor = 0u;   // 竞技场行游标：本组独占的连续实例行

        for (uint32_t s = 0; s < agg.part_bucket_count; ++s)
        {
            // 逐实例累加：每个实例给该槽再加 `slot_part_count[s]` 个部件容量
            const uint32_t capacity = agg.slot_part_count[int(s)] * instance_count;

            plan.capacity.Add(capacity);
            plan.row_base.Add(row_cursor);

            plan.need_rows     += 1u + capacity;
            row_cursor          = Align4(row_cursor + capacity);
        }

        plan.consumed_rows = row_cursor;
    }

    /// 模拟 gather 落桶：返回成功落桶的实例数，溢出数走 out 参数
    uint32_t SimulateEmit(const TemplateAggregate &agg, const uint32_t instance_count,
                          const BucketGroupPlan &plan, uint32_t &out_overflow)
    {
        ValueArray<uint32_t> used;
        used.Reserve(int(plan.capacity.GetCount()));

        for (int s = 0; s < plan.capacity.GetCount(); ++s)
            used.Add(0u);

        uint32_t emitted  = 0;
        out_overflow      = 0;

        for (uint32_t inst = 0; inst < instance_count; ++inst)
        {
            for (uint32_t s = 0; s < agg.part_bucket_count; ++s)
            {
                for (uint32_t p = 0; p < agg.slot_part_count[int(s)]; ++p)
                {
                    const uint32_t slot = used[int(s)]++;

                    if (slot >= plan.capacity[int(s)])
                    {
                        ++out_overflow;     // 容量算少 / 布局错配 ⇒ 丢实例（硬判据）
                        continue;
                    }

                    ++emitted;
                }
            }
        }

        return emitted;
    }

    // ══════════════════════════════════════════════════════════════════════
    // A + C 组：50 实例聚合账
    // ══════════════════════════════════════════════════════════════════════

    bool TestFiftyInstanceAggregation()
    {
        const TemplateAggregate agg = MakeChessboardAggregate();

        if (agg.part_bucket_count != 2u || agg.slot_part_count.GetCount() != 2)
        {
            GLogError(u8"TestGPUModelExpansionAggregation: 夹具构造失败");
            return false;
        }

        const uint32_t kInstances = 50u;    // 50 块棋盘
        const uint32_t kBucketFirst = 7u;   // 本批内该模板组的桶号起点（任意非 0 值）

        BucketGroupPlan plan;
        PlanTemplateGroup(agg, kInstances, kBucketFirst, plan);

        bool ok = true;

        // A1 桶数 = 模板槽数，与实例数无关
        if (plan.bucket_count != agg.part_bucket_count)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 桶数 %u 必须等于模板槽数 %u",
                      plan.bucket_count, agg.part_bucket_count);
        }

        // A2 关键判据：50 个实例 **没有** 膨胀成 50×2 个桶
        if (plan.bucket_count >= kInstances * agg.part_bucket_count)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 桶数 %u 随实例数膨胀"
                      u8"（期望恒 %u，每实例独立成桶会是 %u）",
                      plan.bucket_count, agg.part_bucket_count, kInstances * agg.part_bucket_count);
        }

        // A3 每桶容量 = 每实例该槽部件数 × 实例数
        const uint32_t expect_cap0 = 2u * kInstances;   // slot 0：两部件 ⇒ 100
        const uint32_t expect_cap1 = 1u * kInstances;   // slot 1：一部件 ⇒ 50

        if (plan.capacity[0] != expect_cap0 || plan.capacity[1] != expect_cap1)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 桶容量 (%u,%u) 期望 (%u,%u)",
                      plan.capacity[0], plan.capacity[1], expect_cap0, expect_cap1);
        }

        // A4 一条命令承载整桶实例 ⇒ 桶内实例数必须远大于 1（否则实例化没生效）
        if (plan.capacity[0] <= 1u || plan.capacity[1] <= 1u)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 桶容量必须 > 1 才能验证 Y 轴实例化");
        }

        // A5 全部实例都落得下，无溢出
        uint32_t overflow = 0;
        const uint32_t emitted = SimulateEmit(agg, kInstances, plan, overflow);

        const uint32_t expect_emitted = kInstances * 3u;   // 50 × 3 个有槽部件

        if (emitted != expect_emitted || overflow != 0u)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 落桶 %u / 溢出 %u，期望 %u / 0",
                      emitted, overflow, expect_emitted);
        }

        // C1 行账：前置校验口径 Σ(1+capacity) 与实际推进口径 Σ align4(capacity) 必须一致到
        //    不越界 —— 实际 ≥ 校验，且本例两者相等（容量都是 4 的倍数，起点已对齐）
        if (plan.consumed_rows < plan.need_rows)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 实际行占用 %u < 校验口径 %u",
                      plan.consumed_rows, plan.need_rows);
        }

        const uint32_t expect_need = 2u + expect_cap0 + expect_cap1;   // 2 + 100 + 50

        if (plan.need_rows != expect_need || plan.consumed_rows != expect_need)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 行账 (%u/%u) 期望 %u",
                      plan.need_rows, plan.consumed_rows, expect_need);
        }

        // C2 桶间实例行起点 4 行对齐（push 地址 16 B 对齐依赖）
        for (uint32_t s = 0; s < plan.bucket_count; ++s)
        {
            if ((plan.row_base[int(s)] % 4u) == 0u)
                continue;

            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 桶 %u 行起点 %u 未 4 行对齐",
                      s, plan.row_base[int(s)]);
        }

        // C2b 桶号空间与实例行空间互不影响：换一个桶号起点，行账必须一模一样
        //     （否则把桶号误当行游标 ⇒ 行基址未对齐、行账多算）
        {
            BucketGroupPlan shifted;
            PlanTemplateGroup(agg, kInstances, kBucketFirst + 11u, shifted);

            if (shifted.need_rows != plan.need_rows
                || shifted.consumed_rows != plan.consumed_rows
                || shifted.row_base[0] != plan.row_base[0])
            {
                ok = false;
                GLogError(u8"TestGPUModelExpansionAggregation 失败: 桶号起点影响了实例行账"
                          u8"（%u/%u/%u vs %u/%u/%u）",
                          shifted.need_rows, shifted.consumed_rows, shifted.row_base[0],
                          plan.need_rows, plan.consumed_rows, plan.row_base[0]);
            }
        }

        // C3 反证：容量若按"每实例"算（忘了乘实例数）⇒ 除第一个实例外全部溢出
        BucketGroupPlan per_instance;
        PlanTemplateGroup(agg, 1u, kBucketFirst, per_instance);

        uint32_t bad_overflow = 0;
        const uint32_t bad_emitted = SimulateEmit(agg, kInstances, per_instance, bad_overflow);

        if (!(bad_emitted == 3u && bad_overflow == (kInstances - 1u) * 3u))
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 容量少乘实例数的反证不成立"
                      u8"（落桶 %u / 溢出 %u）", bad_emitted, bad_overflow);
        }

        return ok;
    }

    // ══════════════════════════════════════════════════════════════════════
    // B 组：ICB 命令语义（一桶一命令、X = ceil(元素/组)、Z = 1、Y = 实例数）
    // ══════════════════════════════════════════════════════════════════════

    /// 着色器/CPU 同式：ceil(element_count / group_size)
    uint32_t GroupCountX(const uint32_t element_count, const uint32_t group_size)
    {
        if (group_size == 0u)
            return 0u;

        return (element_count + group_size - 1u) / group_size;
    }

    bool TestIndirectCommandSemantics()
    {
        bool ok = true;

        // 组大小单一真源：线 = 64 段/组；其余 = 192 顶点/组
        if (MeshDispatchGroupSize(false) != graph::mtl::kMeshVertexPassthroughMaxVertices
            || MeshDispatchGroupSize(true) != graph::mtl::kMeshLineQuadMaxInvocations)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: MeshDispatchGroupSize 必须直取"
                      u8" MeshShaderLimits 的常量（组大小单一真源）");
        }

        if (MeshDispatchGroupSize(false) != 192u || MeshDispatchGroupSize(true) != 64u)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 组大小 (%u,%u) 期望 (192,64)",
                      MeshDispatchGroupSize(false), MeshDispatchGroupSize(true));
        }

        struct
        {
            uint32_t    element_count;
            bool        is_lines;
            uint32_t    expect_x;
            const char *why;
        } kCases[] =
        {
            { 192u,  false, 1u, "恰好一组 ⇒ 组数 1（不多派空组）" },
            { 193u,  false, 2u, "多一个元素 ⇒ 进位到 2 组" },
            { 960u,  false, 5u, "整倍数 ⇒ 5 组" },
            { 0u,    false, 0u, "无元素 ⇒ 0 组（命令自动无效）" },
            { 64u,   true,  1u, "线：64 段/组 ⇒ 1 组" },
            { 65u,   true,  2u, "线：65 段 ⇒ 2 组" },
            { 1u,    true,  1u, "线：1 段也占满 1 组" },
        };

        for (const auto &c : kCases)
        {
            const uint32_t got = GroupCountX(c.element_count, MeshDispatchGroupSize(c.is_lines));

            if (got == c.expect_x)
                continue;

            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 元素数 %u（线=%d）组数 %u 期望 %u（%s）",
                      c.element_count, int(c.is_lines), got, c.expect_x, c.why);
        }

        // 一桶一命令：50 实例落在同一个桶 ⇒ 命令条数 = 桶数，不随实例数增长
        const TemplateAggregate agg = MakeChessboardAggregate();
        const uint32_t kInstances = 50u;

        BucketGroupPlan plan;
        PlanTemplateGroup(agg, kInstances, 0u, plan);

        // 命令空间：cmd_index = 块基址 + 桶号（块基址 0 ⇒ 命令号 == 桶号）
        uint32_t command_count = plan.bucket_count;

        if (command_count != agg.part_bucket_count)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 命令条数 %u 必须等于桶数 %u",
                      command_count, agg.part_bucket_count);
        }

        if (command_count >= kInstances)
        {
            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: 命令条数 %u 随实例数增长"
                      u8"（期望 %u，每实例一条命令会是 %u）",
                      command_count, agg.part_bucket_count, kInstances);
        }

        return ok;
    }

    // ══════════════════════════════════════════════════════════════════════
    // D 组：源码契约
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
        AnsiString cull_src, batch_src;

        if (!ReadSource(OS_TEXT("ShaderLibrary/module/descriptor/ecs/entity_gpu_cull.glsl"), cull_src)
            || !ReadSource(OS_TEXT("src/ecs/support/pipeline/PrimitiveBatchPipeline.cpp"), batch_src))
        {
            GLogError(u8"TestGPUModelExpansionAggregation: 无法读取源文件（需在仓库根目录运行）");
            return false;
        }

        bool ok = true;

        // 着色器侧：命令语义
        const struct
        {
            const char *needle;
            const char *why;
        } kShaderChecks[] =
        {
            { "const uint cmd_index = block_base + bucket;",
              "命令号必须是块基址 + 桶号（一桶一命令）" },
            { "icb.commands[cmd_index].groupCountX = group_count_x;",
              "X 轴承载 mesh task 组数" },
            { "icb.commands[cmd_index].groupCountZ = 1u;",
              "Z 轴恒 1" },
            { "atomicAdd(IndirectCmdU32Ref(pc.indirect_commands_addr).words[cmd_index * 3u + 1u], 1u);",
              "Y 轴（实例数）只能原子累加，且必须按 cmd×3+1 定位" },
            { "const uint group_count_x = (element_count + binfo.group_size - 1u) / binfo.group_size;",
              "组数必须是 ceil(元素数 / 下发组大小)，组大小不得硬编码" },
            { "if (slot >= binfo.capacity || (expansion && slot >= l2w_capacity))",
              "槽号越界必须丢实例并计入溢出（容量算少的硬判据）" },
            { "const uint slot = atomicAdd(counters.counters[block_base + bucket], 1u);",
              "实例槽号必须按桶原子取号（同桶多线程并发落桶）" },
            { "cmds[cmd_index] =",
              "cmds 表与 ICB 共用命令空间（必须按 cmd_index 写）" },
        };

        for (const auto &c : kShaderChecks)
        {
            if (ContainsToken(cull_src, c.needle))
                continue;

            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: %s", c.why);
        }

        // CPU 侧：桶组装配
        const struct
        {
            const char *needle;
            const char *why;
        } kCpuChecks[] =
        {
            { "bucket_capacity[grp.group_bucket + s] += part_slot_count[s];",
              "容量必须逐实例累加到**模板组**的槽上（聚合而非每实例一组桶）" },
            { "const uint32_t group_size = MeshDispatchGroupSize(batch_is_lines);",
              "CPU 必须用 MeshDispatchGroupSize 单一真源下发组大小" },
            { "gpu_cull->SetBatchLayout(pass_index, next_index, row_cursor, capacity,",
              "布局行 = 一个桶，容量随行下发" },
            { "row_cursor = (row_cursor + capacity + 3u) & ~3u;",
              "桶间实例行起点必须推进到 4 行对齐" },
            { "gpu_cull->SetModelBucketBase(pass_index, owner->GetEntityID().index,",
              "模板实体必须按实体写回部件桶组基址" },
            { "need_rows += cap;",
              "竞技场前置校验必须按 Σ(1 + 容量) 计（实例行连续占用）" },
        };

        for (const auto &c : kCpuChecks)
        {
            if (ContainsToken(batch_src, c.needle))
                continue;

            ok = false;
            GLogError(u8"TestGPUModelExpansionAggregation 失败: %s", c.why);
        }

        return ok;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestGPUModelExpansionAggregation"));

    Check(TestFiftyInstanceAggregation(), "A/C 组：50 实例聚合账与容量/行/溢出");
    Check(TestIndirectCommandSemantics(), "B 组：ICB 命令语义");
    Check(TestSourceContracts(), "D 组：着色器/CPU 源码契约");

    if (g_failed > 0)
    {
        GLogError(u8"=== TestGPUModelExpansionAggregation: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestGPUModelExpansionAggregation: 全部测试通过 ===");
    return 0;
}
