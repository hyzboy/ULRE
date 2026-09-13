/**
 * ComputeDispatch — ComputeShader 直接源码路径最小示例
 *
 * 演示内容：
 * 1. 直接给 GLSL 源码（不走 ShaderGen 生成器）创建 compute 管线
 *    ShaderProgramManager::CreateComputePipeline(name, glsl, user_layout)
 * 2. 用户自己的 SSBO 描述符集挂在 Set 2（Set 0/1 为引擎全局 Scene/Bindless 集）
 * 3. ComputeCmdBuffer 记录 vkCmdDispatch 并提交
 * 4. 读回结果验证（256 个 float 求平方）
 *
 * 对应 GLSL（#version 450）：
 *   layout(local_size_x = 64) in;
 *   layout(std430, set = 2, binding = 0) readonly  buffer InBuf  { float in_data[];  };
 *   layout(std430, set = 2, binding = 1) writeonly buffer OutBuf { float out_data[]; };
 *   out_data[gl_GlobalInvocationID.x] = in_data[gl_GlobalInvocationID.x] * in_data[gl_GlobalInvocationID.x];
 */

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/log/Log.h>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    constexpr uint32_t DATA_COUNT = 256;
    constexpr uint32_t GROUP_SIZE = 64;                             // 与 local_size_x 一致

    // compute GLSL 直接给源码。Set0/1 是引擎全局集（Scene/Bindless），本例不使用；
    // 用户数据一律放 Set2 起——这与 CreateComputePipeline 的专用 layout 约定一致。
    constexpr const char COMPUTE_GLSL[] = R"(
#version 450
layout(local_size_x = 64) in;

layout(std430, set = 2, binding = 0) restrict readonly  buffer InBuf  { float in_data[];  };
layout(std430, set = 2, binding = 1) restrict writeonly buffer OutBuf { float out_data[]; };

void main()
{
    const uint idx = gl_GlobalInvocationID.x;

    out_data[idx] = in_data[idx] * in_data[idx];
}
)";
}

class ComputeDispatchApp: public WorkObject
{
    DeviceBuffer *input_buffer  = nullptr;
    DeviceBuffer *output_buffer = nullptr;

    ComputePipeline *compute_pipeline = nullptr;

    ComputeCmdBuffer *compute_cmd = nullptr;
    DeviceQueue      *compute_queue = nullptr;

    VkDescriptorSetLayout user_layout = VK_NULL_HANDLE;
    VkDescriptorPool      desc_pool   = VK_NULL_HANDLE;
    VkDescriptorSet       user_set    = VK_NULL_HANDLE;

private:

    // 用户描述符集（Set 2）：binding0=输入 SSBO，binding1=输出 SSBO
    bool CreateUserDescriptorSet(VulkanDevice *dev)
    {
        VkDescriptorSetLayoutBinding bindings[2]{};

        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo dsl_ci{};
        dsl_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dsl_ci.bindingCount = 2;
        dsl_ci.pBindings    = bindings;

        if (vkCreateDescriptorSetLayout(dev->GetDevice(), &dsl_ci, nullptr, &user_layout) != VK_SUCCESS)
            return false;

        VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};

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

        VkDescriptorBufferInfo in_info  = *input_buffer ->GetBufferInfo();
        VkDescriptorBufferInfo out_info = *output_buffer->GetBufferInfo();

        VkWriteDescriptorSet writes[2]{};

        writes[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet           = user_set;
        writes[0].dstBinding       = 0;
        writes[0].descriptorCount  = 1;
        writes[0].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[0].pBufferInfo      = &in_info;

        writes[1].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet           = user_set;
        writes[1].dstBinding       = 1;
        writes[1].descriptorCount  = 1;
        writes[1].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].pBufferInfo      = &out_info;

        vkUpdateDescriptorSets(dev->GetDevice(), 2, writes, 0, nullptr);
        return true;
    }

    // 记录并提交一次 dispatch，然后读回输出验证
    bool DispatchAndVerify()
    {
        VulkanDevice *dev = GetDevice();

        compute_cmd   = dev->CreateComputeCommandBuffer("ComputeDispatch");
        compute_queue = dev->CreateQueue("ComputeDispatch");

        if (!compute_cmd || !compute_queue)
            return false;

        if (!compute_cmd->Begin())
            return false;

        compute_cmd->BindPipeline(compute_pipeline);

        // 全局集绑定（COMPUTE bind point，每 cmd 一次）——compute 管线复用共享全局 layout 时使用
        GetGraphicsContext()->BindGlobalDescriptorSets(compute_cmd, compute_pipeline->GetPipelineLayout());

        // 用户数据集在 Set 2
        compute_cmd->BindDescriptorSets(compute_pipeline->GetPipelineLayout(), 2, &user_set, 1);

        compute_cmd->Dispatch(DATA_COUNT / GROUP_SIZE);

        if (!compute_cmd->End())
            return false;

        if (!compute_queue->Submit(compute_cmd, nullptr, nullptr))
            return false;

        compute_queue->WaitFence();

        // 读回验证：in = 1..DATA_COUNT，out 应为 i*i
        void *mapped = output_buffer->Map(0, output_buffer->GetSize());

        if (!mapped)
            return false;

        const float *out_data = (const float *)mapped;
        bool all_ok = true;

        for (uint32_t i = 0; i < DATA_COUNT; i++)
        {
            const float expect = float(i + 1) * float(i + 1);

            if (out_data[i] != expect)
            {
                GLogError(u8"[ComputeDispatch] verify failed at %u: got %f, expect %f", i, out_data[i], expect);
                all_ok = false;
                break;
            }
        }

        output_buffer->Unmap();

        if (all_ok)
            GLogInfo(u8"[ComputeDispatch] %u elements verified OK (compute via direct GLSL source)", DATA_COUNT);

        return all_ok;
    }

public:

    bool Init() override
    {
        GraphicsContext *gc  = GetGraphicsContext();
        VulkanDevice    *dev = GetDevice();

        if (!gc || !dev)
            return false;

        float in_data[DATA_COUNT];

        for (uint32_t i = 0; i < DATA_COUNT; i++)
            in_data[i] = float(i + 1);

        // 输入：SSBO（Auto 策略，数据经创建接口写入）
        input_buffer = gc->GetBufferManager()->CreateSSBO("ComputeDispatch.In", sizeof(in_data), in_data);

        // 输出：HOST_VISIBLE|HOST_COHERENT（CPUVisible），dispatch 后可直接映射读取
        output_buffer = dev->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                          sizeof(in_data), sizeof(in_data),
                                          BufferAllocPolicy::CPUVisible);

        if (!input_buffer || !output_buffer)
            return false;

        if (!CreateUserDescriptorSet(dev))
            return false;

        // 直接源码创建 compute 管线（编译 GLSL → ShaderModule → ComputePipeline）
        compute_pipeline = gc->GetMaterialManager()->CreateComputePipeline("ComputeDispatch.Squares",
                                                                           COMPUTE_GLSL,
                                                                           user_layout);
        if (!compute_pipeline)
            return false;

        return DispatchAndVerify();
    }
};//class ComputeDispatchApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<ComputeDispatchApp>(OS_TEXT("Compute dispatch (direct GLSL source)"),argc,argv);
}
