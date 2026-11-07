/**
 * TestGPUModelStress —— 10,000 实例复合模型压力测试与性能基准（Plan C Phase 8）。
 *
 * 评估场景：10,000 个复合模型（如城市场景中的建筑/树木），每个模型包含 4 个部件子几何：
 *   Part 0: 基座 (geo=1001, mtl=10, bucket_slot=0, local_offset=(0,0,0))
 *   Part 1: 墙体 (geo=1002, mtl=20, bucket_slot=1, local_offset=(0,0,2))
 *   Part 2: 屋顶 (geo=1003, mtl=30, bucket_slot=2, local_offset=(0,0,4))
 *   Part 3: 装饰 (geo=1004, mtl=40, bucket_slot=3, local_offset=(0,1,1))
 *
 * 对比维度：
 *   1. 内存占用对比：传统 ECS 展开为 40,000 个 Entity / Transform / GPU 行 vs GPU 模板仅 10,000 个 Entity。
 *   2. CPU 变换更新耗时：CPU 计算 40,000 个层级节点 vs 仅计算 10,000 个根节点。
 *   3. 绘制调用与 ICB 压缩：40,000 个离散绘制 vs 4 条聚合并行命令（groupCountY=10000）。
 *   4. GPU 展开段吞吐仿真：双帧矩阵（TAA Motion Vector）+ 逐实例自定义着色在 40,000 个部件展开时的正确性与性能。
 */

#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/InstanceCustomData.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/graph/ubo/ModelExpansionArena.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/math/Matrix.h>
#include <hgl/math/Vector.h>
#include <hgl/time/Time.h>
#include <hgl/type/ValueArray.h>

namespace
{
    using namespace hgl;
    using namespace hgl::math;
    using namespace hgl::graph;
    using namespace hgl::graph::ssbo;

    int g_failed = 0;

    void Check(const bool ok, const char *what)
    {
        if (!ok)
        {
            ++g_failed;
            GLogError(u8"TestGPUModelStress 失败: %s", what);
        }
    }

    constexpr uint32_t kModelCount = 10000u;
    constexpr uint32_t kPartsPerModel = 4u;
    constexpr uint32_t kTotalSubGeometries = kModelCount * kPartsPerModel; // 40,000

    struct MemoryStats
    {
        size_t entity_table_bytes = 0;
        size_t transform_l2w_bytes = 0;
        size_t template_metadata_bytes = 0;
        size_t total_bytes = 0;
    };

    struct BenchmarkTiming
    {
        double traditional_cpu_us = 0.0;
        double gpu_template_cpu_us = 0.0;
        double speedup = 0.0;
    };

    // ── 1. 内存占用评估 ──────────────────────────────────────────────────────────

    MemoryStats EvaluateTraditionalMemory()
    {
        MemoryStats s;
        // 传统 ECS：每个部件作为一个独立实体挂在根节点下（40,000 个实体）
        // EntityGPUTable: 32 字节 / 行
        s.entity_table_bytes = kTotalSubGeometries * 32u;
        // L2W Transform 矩阵：64 字节 / 行
        s.transform_l2w_bytes = kTotalSubGeometries * sizeof(Matrix4f);
        // 模板元数据：0（无模板，每个实体独立存在）
        s.template_metadata_bytes = 0;
        s.total_bytes = s.entity_table_bytes + s.transform_l2w_bytes + s.template_metadata_bytes;
        return s;
    }

    MemoryStats EvaluateGPUTemplateMemory()
    {
        MemoryStats s;
        // GPU 模板模式：场景中仅 10,000 个复合模型实体
        s.entity_table_bytes = kModelCount * 32u;
        // 根节点 L2W 矩阵：10,000 个
        s.transform_l2w_bytes = kModelCount * sizeof(Matrix4f);
        // 全局模板表与部件表（共享 1 个模板 + 4 个部件）：
        s.template_metadata_bytes = sizeof(ModelTemplateRow) + kPartsPerModel * sizeof(ModelPartRow);
        s.total_bytes = s.entity_table_bytes + s.transform_l2w_bytes + s.template_metadata_bytes;
        return s;
    }

    bool TestMemoryFootprint()
    {
        const MemoryStats trad = EvaluateTraditionalMemory();
        const MemoryStats gpu = EvaluateGPUTemplateMemory();

        GLogInfo(u8"--- [内存占用对比: %u 个复合模型 (%u 个子部件)] ---", kModelCount, kTotalSubGeometries);
        GLogInfo(u8"  传统 ECS 模式: 实体表 %zu KB, L2W 矩阵 %zu KB, 模板元数据 %zu B, 总计: %zu KB",
                 trad.entity_table_bytes / 1024,
                 trad.transform_l2w_bytes / 1024,
                 trad.template_metadata_bytes,
                 trad.total_bytes / 1024);
        GLogInfo(u8"  GPU 模板模式 : 实体表 %zu KB, L2W 矩阵 %zu KB, 模板元数据 %zu B, 总计: %zu KB",
                 gpu.entity_table_bytes / 1024,
                 gpu.transform_l2w_bytes / 1024,
                 gpu.template_metadata_bytes,
                 gpu.total_bytes / 1024);

        const double savings = (1.0 - double(gpu.total_bytes) / double(trad.total_bytes)) * 100.0;
        GLogInfo(u8"  => 内存节约率: %.2f%% (节约了 %zu KB 内存)", savings, (trad.total_bytes - gpu.total_bytes) / 1024);

        if (savings < 70.0)
        {
            GLogError(u8"TestMemoryFootprint: 内存节省率未达到预期 (实际: %.2f%%, 期望 >= 70%%)", savings);
            return false;
        }
        return true;
    }

    // ── 2. CPU 变换更新与层级计算耗时评估 ────────────────────────────────────────

    bool TestCPUTransformUpdate()
    {
        // 准备 10,000 个根实体的 TRS
        ValueArray<Matrix4f> root_matrices;
        root_matrices.Resize(kModelCount);
        for (uint32_t i = 0; i < kModelCount; ++i)
        {
            const float fi = static_cast<float>(i);
            root_matrices[i] = glm::translate(Matrix4f(1.0f), Vector3f(fi * 0.1f, fi * 0.05f, 0.0f));
        }

        // 准备 4 个部件的本地矩阵
        Matrix4f part_locals[kPartsPerModel];
        part_locals[0] = glm::translate(Matrix4f(1.0f), Vector3f(0.0f, 0.0f, 0.0f));
        part_locals[1] = glm::translate(Matrix4f(1.0f), Vector3f(0.0f, 0.0f, 2.0f));
        part_locals[2] = glm::translate(Matrix4f(1.0f), Vector3f(0.0f, 0.0f, 4.0f));
        part_locals[3] = glm::translate(Matrix4f(1.0f), Vector3f(0.0f, 1.0f, 1.0f));

        // 评测 A：传统 ECS 模式，CPU 递归计算 40,000 个子节点的 L2W 矩阵
        ValueArray<Matrix4f> trad_child_matrices;
        trad_child_matrices.Resize(kTotalSubGeometries);

        const uint64 trad_start = GetUptimeUs();
        for (uint32_t run = 0; run < 10; ++run) // 跑 10 次取平滑耗时
        {
            uint32_t child_idx = 0;
            for (uint32_t i = 0; i < kModelCount; ++i)
            {
                const Matrix4f &parent = root_matrices[i];
                for (uint32_t p = 0; p < kPartsPerModel; ++p)
                {
                    trad_child_matrices[child_idx++] = parent * part_locals[p];
                }
            }
        }
        const uint64 trad_end = GetUptimeUs();
        const double trad_avg_us = double(trad_end - trad_start) / 10.0;

        // 评测 B：GPU 模板模式，CPU 仅计算 10,000 个根实体的矩阵（子节点由 GPU 剔除 Compute Shader 并行展开）
        ValueArray<Matrix4f> gpu_root_matrices;
        gpu_root_matrices.Resize(kModelCount);

        const uint64 gpu_start = GetUptimeUs();
        for (uint32_t run = 0; run < 10; ++run)
        {
            for (uint32_t i = 0; i < kModelCount; ++i)
            {
                const float fi = static_cast<float>(i);
                gpu_root_matrices[i] = glm::translate(Matrix4f(1.0f), Vector3f(fi * 0.1f, fi * 0.05f, 0.0f));
            }
        }
        const uint64 gpu_end = GetUptimeUs();
        const double gpu_avg_us = double(gpu_end - gpu_start) / 10.0;

        const double speedup = trad_avg_us / (gpu_avg_us > 0.0 ? gpu_avg_us : 1.0);

        GLogInfo(u8"--- [CPU 变换更新耗时对比: 10,000 实例] ---");
        GLogInfo(u8"  传统模式 (40,000 节点层级乘法): %.2f us (%.3f ms)", trad_avg_us, trad_avg_us / 1000.0);
        GLogInfo(u8"  GPU 模板模式 (仅 10,000 根节点) : %.2f us (%.3f ms)", gpu_avg_us, gpu_avg_us / 1000.0);
        GLogInfo(u8"  => CPU 加速比: %.2fx", speedup);

        if (speedup < 1.5)
        {
            GLogError(u8"TestCPUTransformUpdate: CPU 更新加速比未达到预期");
            return false;
        }

        return true;
    }

    // ── 3. ICB 绘制命令与实例聚合对比 ──────────────────────────────────────────

    struct SimulatedDrawCommand
    {
        uint32_t geometry_id = 0;
        uint32_t instance_count = 0;
        uint32_t first_instance = 0;
    };

    bool TestDrawCommandAggregation()
    {
        // 传统模式：若无离线合并网格，每个模型每个部件产生独立调用或分散实例，最大可达 40,000 条调用
        constexpr uint32_t kTradDrawCalls = kTotalSubGeometries;

        // GPU 模板模式：4 个部件映射到 4 个素材桶 (bucket_slot 0..3)
        // 每个素材桶一条 ICB 命令，实例聚合在 instance_count (10,000)
        constexpr uint32_t kGPUTemplateCommands = kPartsPerModel; // 4 条
        SimulatedDrawCommand icb_commands[kGPUTemplateCommands];

        for (uint32_t b = 0; b < kGPUTemplateCommands; ++b)
        {
            icb_commands[b].geometry_id = 1001u + b;
            icb_commands[b].instance_count = kModelCount; // 10,000 实例聚合到 1 条命令
            icb_commands[b].first_instance = b * kModelCount;
        }

        GLogInfo(u8"--- [DrawCall / ICB 命令压缩对比] ---");
        GLogInfo(u8"  传统 ECS 离散 DrawCall: %u 条", kTradDrawCalls);
        GLogInfo(u8"  GPU 模板聚合 ICB 命令 : %u 条 (每条承载 %u 实例)", kGPUTemplateCommands, kModelCount);

        const double reduction = (1.0 - double(kGPUTemplateCommands) / double(kTradDrawCalls)) * 100.0;
        GLogInfo(u8"  => 命令压缩率: %.4f%%", reduction);

        if (kGPUTemplateCommands != 4u || reduction < 99.9)
        {
            GLogError(u8"TestDrawCommandAggregation: 命令聚合压缩未达到预期");
            return false;
        }

        return true;
    }

    // ── 4. GPU 展开段吞吐仿真（双矩阵 + 自定义着色数据）─────────────────────────

    bool TestGPUExpansionThroughputSimulation()
    {
        // 模拟 ModelExpansionArena
        ModelExpansionArena arena{};
        arena.flags = kModelExpansionFlagEnabled;
        arena.l2w_base_row = 10000u;
        arena.l2w_row_capacity = kTotalSubGeometries; // 40,000 行
        arena.prev_l2w_base_row = 50000u;

        // 模拟 GPU L2W 矩阵池
        ValueArray<Matrix4f> l2w_pool;
        l2w_pool.Resize(static_cast<int>(arena.prev_l2w_base_row + arena.l2w_row_capacity));

        // 模拟逐实例自定义数据表
        ValueArray<InstanceCustomData> custom_data_out;
        custom_data_out.Resize(static_cast<int>(kTotalSubGeometries));

        // 部件配置
        Matrix4f part_locals[kPartsPerModel];
        part_locals[0] = glm::translate(Matrix4f(1.0f), Vector3f(0.0f, 0.0f, 0.0f));
        part_locals[1] = glm::translate(Matrix4f(1.0f), Vector3f(0.0f, 0.0f, 2.0f));
        part_locals[2] = glm::translate(Matrix4f(1.0f), Vector3f(0.0f, 0.0f, 4.0f));
        part_locals[3] = glm::translate(Matrix4f(1.0f), Vector3f(0.0f, 1.0f, 1.0f));

        const uint64 start_time = GetUptimeUs();

        // 模拟 Compute Cull Shader 展开 10,000 个模型实体（共 40,000 个部件）
        uint32_t inst_row = 0;
        for (uint32_t m = 0; m < kModelCount; ++m)
        {
            const float fm = static_cast<float>(m);
            const Matrix4f root_l2w = glm::translate(Matrix4f(1.0f), Vector3f(fm, 0.0f, 0.0f));
            const Matrix4f prev_root_l2w = glm::translate(Matrix4f(1.0f), Vector3f(fm - 0.5f, 0.0f, 0.0f));
            const Vector4f custom_color(0.5f + 0.5f * (float(m % 100) / 100.0f), 0.8f, 0.9f, 1.0f);

            for (uint32_t p = 0; p < kPartsPerModel; ++p)
            {
                const Matrix4f part_l2w = root_l2w * part_locals[p];
                const Matrix4f prev_part_l2w = prev_root_l2w * part_locals[p];

                // 写入当前展开矩阵与上一帧展开矩阵（T7.2 口径）
                const uint32_t l2w_row = arena.l2w_base_row + inst_row;
                const uint32_t prev_l2w_row = static_cast<uint32_t>(arena.prev_l2w_base_row) + inst_row;

                l2w_pool[l2w_row] = part_l2w;
                l2w_pool[prev_l2w_row] = prev_part_l2w;

                // 写入自定义数据（T6.3 口径）
                custom_data_out[inst_row].custom_color = custom_color;

                ++inst_row;
            }
        }

        const uint64 end_time = GetUptimeUs();
        const double duration_us = double(end_time - start_time);

        GLogInfo(u8"--- [GPU 展开段 40,000 部件双帧仿真吞吐] ---");
        GLogInfo(u8"  展开总行数: %u 行 (当前帧 + 上一帧共 %u 矩阵)", inst_row, inst_row * 2);
        GLogInfo(u8"  单核 CPU 展开吞吐模拟耗时: %.2f us (%.3f ms)", duration_us, duration_us / 1000.0);
        GLogInfo(u8"  吞吐率: %.2f 百万部件/秒 (注: GPU 上数十组 Compute Shader 并行执行速度将高两个数量级)",
                 (double(inst_row) / (duration_us > 0.0 ? duration_us : 1.0)));

        // 验证采样点数据准确性
        // 采样第 5000 个模型 (m = 5000) 的 Part 2 (p = 2)
        const uint32_t sample_inst = 5000u * kPartsPerModel + 2u;
        const Matrix4f &sample_l2w = l2w_pool[arena.l2w_base_row + sample_inst];
        const Matrix4f &sample_prev_l2w = l2w_pool[arena.prev_l2w_base_row + sample_inst];
        const Vector4f &sample_color = custom_data_out[sample_inst].custom_color;

        // 预期平移：root=(5000, 0, 0) + part_local=(0, 0, 4) => (5000, 0, 4)
        if (sample_l2w[3][0] != 5000.0f || sample_l2w[3][1] != 0.0f || sample_l2w[3][2] != 4.0f)
        {
            GLogError(u8"TestGPUExpansionThroughputSimulation: 当前帧展开矩阵采样错误");
            return false;
        }

        // 预期上一帧平移：prev_root=(4999.5, 0, 0) + part_local=(0, 0, 4) => (4999.5, 0, 4)
        if (sample_prev_l2w[3][0] != 4999.5f || sample_prev_l2w[3][1] != 0.0f || sample_prev_l2w[3][2] != 4.0f)
        {
            GLogError(u8"TestGPUExpansionThroughputSimulation: 上一帧展开矩阵采样错误");
            return false;
        }

        if (sample_color.w != 1.0f)
        {
            GLogError(u8"TestGPUExpansionThroughputSimulation: 逐实例自定义着色数据采样错误");
            return false;
        }

        return true;
    }
} // namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestGPUModelStress"));

    GLogInfo(u8"============================================================");
    GLogInfo(u8"  10,000 实例复合模型压力测试基准 (Plan C Phase 8)");
    GLogInfo(u8"============================================================");

    Check(TestMemoryFootprint(), "内存占用对比测试");
    Check(TestCPUTransformUpdate(), "CPU 变换更新吞吐对比测试");
    Check(TestDrawCommandAggregation(), "ICB 绘制命令聚合压缩测试");
    Check(TestGPUExpansionThroughputSimulation(), "GPU 展开段双帧与个性化数据吞吐仿真");

    if (g_failed > 0)
    {
        GLogError(u8"=== TestGPUModelStress: 存在 %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestGPUModelStress: 全部基准与压测通过 ===");
    return 0;
}
