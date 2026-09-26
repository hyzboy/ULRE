/**
 * ComputeDispatch — ComputeShader 直接源码路径最小示例
 *
 * 演示内容：
 * 1. 直接给 GLSL 源码（不走 ShaderGen 生成器）创建 compute 管线
 *    ShaderProgramManager::CreateComputePipeline(name, glsl, push_constant_size)
 * 2. 用户数据走 BDA：输入/输出 SSBO 的设备地址经 push constant 下发，shader 侧
 *    layout(buffer_reference) 解引用——无用户描述符集（Set2 已随 BDA 终态退场）
 * 3. ComputeCmdBuffer 记录 vkCmdDispatch 并提交
 * 4. 读回结果验证（256 个 float 求平方）
 */

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/log/Log.h>
#include<cstdint>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    constexpr uint32_t DATA_COUNT = 256;
    constexpr uint32_t GROUP_SIZE = 64;                             // 与 local_size_x 一致

    // push constant 只承载两个缓冲区设备地址（BDA）；Set0/1 引擎全局集本例不使用。
    struct DispatchPushConstants
    {
        uint64_t in_addr;
        uint64_t out_addr;
    };

    static_assert(sizeof(DispatchPushConstants) == 16, "DispatchPushConstants 布局必须与 GLSL push_constant block 一致");

    constexpr const char COMPUTE_GLSL[] = R"(
#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(local_size_x = 64) in;

// 缓冲区地址经 push constant 下发（BDA），无 set 无 binding
layout(buffer_reference, std430, buffer_reference_align=16) buffer FloatArrayRef { float data[]; };

layout(push_constant) uniform PushConstants {
    uint64_t in_addr;
    uint64_t out_addr;
} pc;

void main()
{
    const uint idx = gl_GlobalInvocationID.x;

    FloatArrayRef in_buf  = FloatArrayRef(pc.in_addr);
    FloatArrayRef out_buf = FloatArrayRef(pc.out_addr);

    out_buf.data[idx] = in_buf.data[idx] * in_buf.data[idx];
}
)";
}

class ComputeDispatchApp: public WorkObject
{
    DeviceBuffer *input_buffer  = nullptr;
    DeviceBuffer *output_buffer = nullptr;

    ComputePipeline *compute_pipeline = nullptr;

    ComputeCmdBuffer *compute_cmd    = nullptr;
    DeviceQueue      *compute_queue  = nullptr;

private:

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

        // 用户数据缓冲区地址经 push constant 下发（BDA）
        DispatchPushConstants pc{};
        pc.in_addr  = dev->GetBufferDeviceAddressAligned16(input_buffer ->GetBuffer());
        pc.out_addr = dev->GetBufferDeviceAddressAligned16(output_buffer->GetBuffer());

        if (pc.in_addr == 0 || pc.out_addr == 0)
        {
            GLogError(u8"[ComputeDispatch] 缓冲区设备地址无效（BDA 16B 对齐承诺失败）");
            return false;
        }

        compute_cmd->PushConstants(compute_pipeline->GetPipelineLayout(), &pc, sizeof(pc));

        compute_cmd->Dispatch(DATA_COUNT / GROUP_SIZE);

        if (!compute_cmd->End())
            return false;

        if (!compute_queue->Submit(compute_cmd, nullptr, 0, nullptr, 0))
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
        // 输入/输出都带 SHADER_DEVICE_ADDRESS_BIT——地址经 push constant 下发（BDA）
        output_buffer = dev->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                          sizeof(in_data), sizeof(in_data),
                                          BufferAllocPolicy::CPUVisible);

        if (!input_buffer || !output_buffer)
            return false;

        // 直接源码创建 compute 管线（编译 GLSL → ShaderModule → ComputePipeline）
        compute_pipeline = gc->GetMaterialManager()->CreateComputePipeline("ComputeDispatch.Squares",
                                                                           COMPUTE_GLSL,
                                                                           sizeof(DispatchPushConstants));
        if (!compute_pipeline)
            return false;

        return DispatchAndVerify();
    }
};//class ComputeDispatchApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<ComputeDispatchApp>(OS_TEXT("Compute dispatch (direct GLSL source)"),argc,argv);
}
