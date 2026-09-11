#pragma once

#include<hgl/vk/VKDevice.h>

namespace hgl::graph{

/**
 * 内部辅助（仅 src/Vulkan/buffer 使用）：把 BufferAllocPolicy::Auto 解析为具体策略。
 * Auto = 设备支持 ReBAR → CPUVisible（直写显存）；否则 StagedUpload（staging + 上传）。
 */
inline BufferAllocPolicy ResolveBufferPolicy(VulkanDevice *device, BufferAllocPolicy policy)
{
    if(policy!=BufferAllocPolicy::Auto)
        return policy;

    if(device->GetPhyDevice()->HasReBAR())
        return BufferAllocPolicy::CPUVisible;

    return BufferAllocPolicy::StagedUpload;
}

}//namespace hgl::graph
