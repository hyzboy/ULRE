/**
 * ComputeTransformHierarchy — Step 1: L2W ComputeShader 分层拓扑计算
 *
 * 演示内容：
 * 1. 利用 TransformDataStorage 构建 8 层深度的多分支层级树（212 个节点）
 * 2. CPU 侧生成分层拓扑序列 eval_order 与各层偏移 level_offsets
 * 3. 将 local_matrices、parent_indices、eval_order 上传至 GPU SSBO
 * 4. Compute Shader 配合 PushConstants (level_start_offset, level_node_count)
 *    按 Level 逐层分派，层间插入 VkBufferMemoryBarrier 保证父级写入对子级可见
 * 5. GPU 端使用 column-major 与局部矩阵乘法 world_parent * local_child 计算世界矩阵
 * 6. CPU 读回 world_matrices 并与 CPU 平铺计算结果进行高精度数值比对
 * 7. 在每帧 Tick 中动态旋转根节点，验证动态更新下的 GPU-CPU 严格一致性
 */

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/ecs/support/TransformDataStorage.h>
#include<hgl/type/ValueArray.h>
#include<hgl/log/Log.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/quaternion.hpp>
#include<cmath>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    struct LevelPushConstant
    {
        uint32_t level_start_offset;
        uint32_t level_node_count;
        uint64_t local_addr;        // local_matrices 表设备地址（BDA）
        uint64_t parent_addr;       // parent_indices 表设备地址
        uint64_t eval_order_addr;   // eval_order 表设备地址
        uint64_t world_addr;        // world_matrices 表设备地址（读写）
    };

    static_assert(sizeof(LevelPushConstant) == 40, "LevelPushConstant 布局必须与 GLSL push_constant block 一致");

    constexpr const char COMPUTE_TRANSFORM_GLSL[] = R"(
#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(local_size_x = 64) in;

layout(push_constant) uniform LevelPushConstant
{
    uint level_start_offset;
    uint level_node_count;
    uint64_t local_addr;
    uint64_t parent_addr;
    uint64_t eval_order_addr;
    uint64_t world_addr;
} pc;

// 四张表全走 BDA：地址经 push constant 下发，无 set 无 binding
layout(buffer_reference, std430, buffer_reference_align=16) buffer Mat4ArrayRef { mat4 values[]; };
layout(buffer_reference, std430, buffer_reference_align=16) buffer UintArrayRef { uint values[]; };

const uint INVALID_HANDLE = 0xFFFFFFFFu;

void main()
{
    uint local_idx = gl_GlobalInvocationID.x;
    if (local_idx >= pc.level_node_count)
        return;

    Mat4ArrayRef local_matrices = Mat4ArrayRef(pc.local_addr);
    UintArrayRef parent_indices = UintArrayRef(pc.parent_addr);
    UintArrayRef eval_order     = UintArrayRef(pc.eval_order_addr);
    Mat4ArrayRef world_matrices = Mat4ArrayRef(pc.world_addr);

    uint node_idx = eval_order.values[pc.level_start_offset + local_idx];
    uint parent_idx = parent_indices.values[node_idx];

    mat4 local_mat = local_matrices.values[node_idx];

    if (parent_idx != INVALID_HANDLE)
    {
        world_matrices.values[node_idx] = world_matrices.values[parent_idx] * local_mat;
    }
    else
    {
        world_matrices.values[node_idx] = local_mat;
    }
}
)";
}

class ComputeTransformHierarchyApp: public WorkObject
{
    ecs::TransformDataStorage storage;

    DeviceBuffer *local_buffer      = nullptr;
    DeviceBuffer *parent_buffer     = nullptr;
    DeviceBuffer *eval_order_buffer = nullptr;
    DeviceBuffer *world_buffer      = nullptr;

    ComputePipeline *compute_pipeline = nullptr;
    ComputeCmdBuffer *compute_cmd     = nullptr;
    DeviceQueue      *compute_queue   = nullptr;

    uint32_t total_node_count = 0;
    ecs::TransformDataStorage::HandleID root_handles[4]{};
    float anim_time = 0.0f;
    uint32_t tick_frame_count = 0;

private:

    void BuildHierarchy()
    {
        // Level 0: 4 个独立根节点
        for (int r = 0; r < 4; ++r)
        {
            root_handles[r] = storage.Allocate();
            storage.SetPosition(root_handles[r], glm::vec3(float(r) * 10.0f, 0.0f, 0.0f));
            storage.SetRotation(root_handles[r], glm::angleAxis(glm::radians(float(r) * 20.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
            storage.SetScale(root_handles[r], glm::vec3(1.0f));
        }

        hgl::ValueArray<ecs::TransformDataStorage::HandleID> current_level;
        hgl::ValueArray<ecs::TransformDataStorage::HandleID> next_level;

        // Level 1: 每个根节点 4 个子节点 (共 16 个节点)
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                auto child = storage.Allocate();
                storage.SetParent(child, root_handles[r]);
                storage.SetPosition(child, glm::vec3(0.0f, float(c + 1) * 2.0f, 0.0f));
                storage.SetRotation(child, glm::angleAxis(glm::radians(15.0f * float(c + 1)), glm::vec3(1.0f, 0.0f, 0.0f)));
                storage.SetScale(child, glm::vec3(0.95f));
                current_level.Add(child);
            }
        }

        // Level 2: 每个 Level 1 节点 2 个子节点 (共 32 个节点)
        for (int i = 0; i < current_level.GetCount(); ++i)
        {
            auto p = current_level[i];
            for (int c = 0; c < 2; ++c)
            {
                auto child = storage.Allocate();
                storage.SetParent(child, p);
                storage.SetPosition(child, glm::vec3(float(c == 0 ? 1 : -1) * 1.5f, 0.0f, 2.0f));
                storage.SetRotation(child, glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
                storage.SetScale(child, glm::vec3(0.9f));
                next_level.Add(child);
            }
        }
        current_level = next_level;
        next_level.Clear();

        // Level 3 ~ 7: 每个节点单链向下延伸 5 层 (32 * 5 = 160 个节点)
        for (int depth = 3; depth < 8; ++depth)
        {
            for (int i = 0; i < current_level.GetCount(); ++i)
            {
                auto p = current_level[i];
                auto child = storage.Allocate();
                storage.SetParent(child, p);
                storage.SetPosition(child, glm::vec3(0.5f, 0.5f, 1.0f));
                storage.SetRotation(child, glm::angleAxis(glm::radians(10.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
                storage.SetScale(child, glm::vec3(0.98f));
                next_level.Add(child);
            }
            current_level = next_level;
            next_level.Clear();
        }

        storage.RebuildTopologyOrder();
        storage.UpdateAllLocalMatrices();
        total_node_count = static_cast<uint32_t>(storage.GetCount());

        GLogInfo(u8"[ComputeTransformHierarchy] Hierarchy built: %u nodes across %u levels",
                 total_node_count, storage.GetLevelCount());
    }

    bool DispatchAndVerify(bool log_verbose = true)
    {
        if (!compute_cmd || !compute_queue)
            return false;

        if (!compute_cmd->Begin())
            return false;

        compute_cmd->BindPipeline(compute_pipeline);
        GetGraphicsContext()->BindGlobalDescriptorSets(compute_cmd, compute_pipeline->GetPipelineLayout());

        // 四张表的设备地址（BDA）——每层 push 一次，随层 offset/count 一起下发
        VulkanDevice *dev = GetDevice();
        if (!dev)
            return false;

        const uint64_t local_addr      = dev->GetBufferDeviceAddressAligned16(local_buffer     ->GetBuffer());
        const uint64_t parent_addr     = dev->GetBufferDeviceAddressAligned16(parent_buffer    ->GetBuffer());
        const uint64_t eval_order_addr = dev->GetBufferDeviceAddressAligned16(eval_order_buffer->GetBuffer());
        const uint64_t world_addr      = dev->GetBufferDeviceAddressAligned16(world_buffer     ->GetBuffer());

        if (local_addr == 0 || parent_addr == 0 || eval_order_addr == 0 || world_addr == 0)
        {
            GLogError(u8"[ComputeTransformHierarchy] 缓冲区设备地址无效（BDA 16B 对齐承诺失败）");
            return false;
        }

        const uint32_t level_count = storage.GetLevelCount();

        // 核心：逐层级调度与内存屏障
        for (uint32_t level = 0; level < level_count; ++level)
        {
            const uint32_t offset = storage.GetLevelOffset(level);
            const uint32_t count  = storage.GetLevelNodeCount(level);

            if (count == 0)
                continue;

            LevelPushConstant pc{ offset, count, local_addr, parent_addr, eval_order_addr, world_addr };
            compute_cmd->PushConstants(compute_pipeline->GetPipelineLayout(), &pc, sizeof(LevelPushConstant));

            const uint32_t group_x = (count + 63) / 64;
            compute_cmd->Dispatch(group_x);

            // 当存在后续层级时，必须插入内存屏障，确保子层级能读到本层写入的世界矩阵
            if (level + 1 < level_count)
            {
                compute_cmd->BufferMemoryBarrier(
                    world_buffer->GetBuffer(),
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_SHADER_READ_BIT
                );
            }
        }

        if (!compute_cmd->End())
            return false;

        if (!compute_queue->Submit(compute_cmd, nullptr, nullptr))
            return false;

        compute_queue->WaitFence();

        // CPU 平铺参考计算
        storage.UpdateAllWorldMatricesFlat();
        const glm::mat4 *cpu_ref = storage.GetWorldMatricesData();

        // GPU 输出读回与对比
        void *mapped = world_buffer->Map(0, sizeof(glm::mat4) * total_node_count);
        if (!mapped)
            return false;

        const glm::mat4 *gpu_res = static_cast<const glm::mat4 *>(mapped);
        float max_diff = 0.0f;
        uint32_t worst_idx = 0;

        for (uint32_t i = 0; i < total_node_count; ++i)
        {
            for (int col = 0; col < 4; ++col)
            {
                for (int row = 0; row < 4; ++row)
                {
                    const float d = std::abs(gpu_res[i][col][row] - cpu_ref[i][col][row]);
                    if (d > max_diff)
                    {
                        max_diff = d;
                        worst_idx = i;
                    }
                }
            }
        }

        world_buffer->Unmap();

        if (max_diff > 1e-4f)
        {
            GLogError(u8"[ComputeTransformHierarchy] Validation FAILED! max_diff=%.6f at node %u", max_diff, worst_idx);
            return false;
        }

        if (log_verbose)
        {
            GLogInfo(u8"[ComputeTransformHierarchy] PASS: %u nodes across %u levels verified successfully! max_diff=%.7f",
                     total_node_count, level_count, max_diff);
        }

        return true;
    }

public:

    bool Init() override
    {
        GraphicsContext *gc  = GetGraphicsContext();
        VulkanDevice    *dev = GetDevice();

        if (!gc || !dev)
            return false;

        BuildHierarchy();

        const VkDeviceSize mat_bytes = sizeof(glm::mat4) * total_node_count;
        const VkDeviceSize idx_bytes = sizeof(uint32_t) * total_node_count;

        // 输入 SSBO
        local_buffer      = gc->GetBufferManager()->CreateSSBO("ComputeTransform.Local", mat_bytes, (void *)storage.GetLocalMatricesData());
        parent_buffer     = gc->GetBufferManager()->CreateSSBO("ComputeTransform.Parent", idx_bytes, (void *)storage.GetParentIndicesData());
        eval_order_buffer = gc->GetBufferManager()->CreateSSBO("ComputeTransform.EvalOrder", idx_bytes, (void *)storage.GetEvalOrderData());

        // 输出 SSBO（CPU 可见，方便读回验证；BDA usage：地址经 push constant 下发）
        world_buffer      = dev->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                              mat_bytes, mat_bytes,
                                              BufferAllocPolicy::CPUVisible);

        if (!local_buffer || !parent_buffer || !eval_order_buffer || !world_buffer)
            return false;

        compute_cmd   = dev->CreateComputeCommandBuffer("ComputeTransformHierarchy");
        compute_queue = dev->CreateQueue("ComputeTransformHierarchy");

        if (!compute_cmd || !compute_queue)
            return false;

        compute_pipeline = gc->GetMaterialManager()->CreateComputePipeline(
            "ComputeTransformHierarchy.Pipeline",
            COMPUTE_TRANSFORM_GLSL,
            sizeof(LevelPushConstant)
        );

        if (!compute_pipeline)
            return false;

        return DispatchAndVerify(true);
    }

    void Tick(double delta_time) override
    {
        anim_time += static_cast<float>(delta_time);

        // 动态旋转 4 个根节点
        for (int r = 0; r < 4; ++r)
        {
            const float angle = anim_time * (30.0f + float(r) * 15.0f);
            storage.SetRotation(root_handles[r], glm::angleAxis(glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f)));
        }

        storage.UpdateAllLocalMatrices();

        // 更新 local_buffer
        if (auto *gpu = local_buffer->GetGPUBuffer())
        {
            gpu->Write(storage.GetLocalMatricesData(), 0, sizeof(glm::mat4) * 4);
        }

        ++tick_frame_count;
        const bool should_log = (tick_frame_count % 60 == 1);

        DispatchAndVerify(should_log);

        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<ComputeTransformHierarchyApp>(OS_TEXT("Compute Transform Hierarchy (Step 1)"), argc, argv);
}
