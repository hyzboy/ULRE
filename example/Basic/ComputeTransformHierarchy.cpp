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
    };

    constexpr const char COMPUTE_TRANSFORM_GLSL[] = R"(
#version 450
layout(local_size_x = 64) in;

layout(push_constant) uniform LevelPushConstant
{
    uint level_start_offset;
    uint level_node_count;
} pc;

layout(std430, set = 2, binding = 0) restrict readonly buffer LocalMatricesBuf
{
    mat4 local_matrices[];
};

layout(std430, set = 2, binding = 1) restrict readonly buffer ParentIndicesBuf
{
    uint parent_indices[];
};

layout(std430, set = 2, binding = 2) restrict readonly buffer EvalOrderBuf
{
    uint eval_order[];
};

layout(std430, set = 2, binding = 3) restrict buffer WorldMatricesBuf
{
    mat4 world_matrices[];
};

const uint INVALID_HANDLE = 0xFFFFFFFFu;

void main()
{
    uint local_idx = gl_GlobalInvocationID.x;
    if (local_idx >= pc.level_node_count)
        return;

    uint node_idx = eval_order[pc.level_start_offset + local_idx];
    uint parent_idx = parent_indices[node_idx];

    mat4 local_mat = local_matrices[node_idx];

    if (parent_idx != INVALID_HANDLE)
    {
        world_matrices[node_idx] = world_matrices[parent_idx] * local_mat;
    }
    else
    {
        world_matrices[node_idx] = local_mat;
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

    VkDescriptorSetLayout user_layout = VK_NULL_HANDLE;
    VkDescriptorPool      desc_pool   = VK_NULL_HANDLE;
    VkDescriptorSet       user_set    = VK_NULL_HANDLE;

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

    bool CreateUserDescriptorSet(VulkanDevice *dev)
    {
        VkDescriptorSetLayoutBinding bindings[4]{};

        for (uint32_t i = 0; i < 4; ++i)
        {
            bindings[i].binding         = i;
            bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo dsl_ci{};
        dsl_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dsl_ci.bindingCount = 4;
        dsl_ci.pBindings    = bindings;

        if (vkCreateDescriptorSetLayout(dev->GetDevice(), &dsl_ci, nullptr, &user_layout) != VK_SUCCESS)
            return false;

        VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4};

        VkDescriptorPoolCreateInfo pool_ci{};
        pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_ci.maxSets       = 1;
        pool_ci.poolSizeCount = 1;
        pool_ci.pPoolSizes    = &pool_size;

        if (vkCreateDescriptorPool(dev->GetDevice(), &pool_ci, nullptr, &desc_pool) != VK_SUCCESS)
            return false;

        VkDescriptorSetAllocateInfo alloc_ci{};
        alloc_ci.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_ci.descriptorPool     = desc_pool;
        alloc_ci.descriptorSetCount = 1;
        alloc_ci.pSetLayouts        = &user_layout;

        if (vkAllocateDescriptorSets(dev->GetDevice(), &alloc_ci, &user_set) != VK_SUCCESS)
            return false;

        VkDescriptorBufferInfo b_info[4]{
            *local_buffer     ->GetBufferInfo(),
            *parent_buffer    ->GetBufferInfo(),
            *eval_order_buffer->GetBufferInfo(),
            *world_buffer     ->GetBufferInfo()
        };

        VkWriteDescriptorSet writes[4]{};

        for (uint32_t i = 0; i < 4; ++i)
        {
            writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet          = user_set;
            writes[i].dstBinding      = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo     = &b_info[i];
        }

        vkUpdateDescriptorSets(dev->GetDevice(), 4, writes, 0, nullptr);
        return true;
    }

    bool DispatchAndVerify(bool log_verbose = true)
    {
        if (!compute_cmd || !compute_queue)
            return false;

        if (!compute_cmd->Begin())
            return false;

        compute_cmd->BindPipeline(compute_pipeline);
        GetGraphicsContext()->BindGlobalDescriptorSets(compute_cmd, compute_pipeline->GetPipelineLayout());
        compute_cmd->BindDescriptorSets(compute_pipeline->GetPipelineLayout(), 2, &user_set, 1);

        const uint32_t level_count = storage.GetLevelCount();

        // 核心：逐层级调度与内存屏障
        for (uint32_t level = 0; level < level_count; ++level)
        {
            const uint32_t offset = storage.GetLevelOffset(level);
            const uint32_t count  = storage.GetLevelNodeCount(level);

            if (count == 0)
                continue;

            LevelPushConstant pc{ offset, count };
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

        // 输出 SSBO（CPU 可见，方便读回验证）
        world_buffer      = dev->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                              mat_bytes, mat_bytes,
                                              BufferAllocPolicy::CPUVisible);

        if (!local_buffer || !parent_buffer || !eval_order_buffer || !world_buffer)
            return false;

        if (!CreateUserDescriptorSet(dev))
            return false;

        compute_cmd   = dev->CreateComputeCommandBuffer("ComputeTransformHierarchy");
        compute_queue = dev->CreateQueue("ComputeTransformHierarchy");

        if (!compute_cmd || !compute_queue)
            return false;

        compute_pipeline = gc->GetMaterialManager()->CreateComputePipeline(
            "ComputeTransformHierarchy.Pipeline",
            COMPUTE_TRANSFORM_GLSL,
            user_layout,
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
