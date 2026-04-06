#pragma once

#include <cstdint>

namespace hgl::core
{

/**
 * 对象类型标签
 * 用于：
 *  1. VKObjectNameBuilder - 层级命名
 *  2. ObjectTracker - 分配追踪
 *  3. 通用资源分类
 */
enum class ObjectTypeTag : uint8_t
{
    None = 0,

    // Vulkan resources (with VK prefix to avoid conflicts with other APIs)
    VKQueue,
    VKSemaphore,
    VKFence,
    VKRenderCommandBuffer,
    VKTextureCommandBuffer,
    VKComputeCommandBuffer,
    VKBuffer,
    VKMemory,
    VKImage,
    VKImageView,
    VKSampler,
    VKFramebuffer,
    VKRenderPass,
    VKPipeline,
    VKPipelineLayout,
    VKDescriptorSet,
    VKDescriptorSetLayout,
    VKShaderModule,
    VKSwapchain,

    // Custom types (for logging)
    RenderTarget,
    Texture,
    ShaderProgram,
    MaterialInstance,
    Mesh,

    // ECS and system types
    IndirectDrawBuffer,
    IndirectDrawIndexedBuffer,
    IndirectDispatchBuffer,
    VertexBuffer,
    IndexBuffer,
    UniformBuffer,
    StorageBuffer,
    TextureBuffer,
    ReadbackBuffer,

    // High-level types
    RenderSystem,
    BatchSystem,
    CommandRecorder,
    FrameResource,
    SwapchainFrame,
};

/**
 * 获取对象类型标签的字符串表示
 */
inline const char* GetTagString(ObjectTypeTag tag)
{
    switch (tag)
    {
        case ObjectTypeTag::VKQueue:                  return "VKQueue";
        case ObjectTypeTag::VKSemaphore:              return "VKSemaphore";
        case ObjectTypeTag::VKFence:                  return "VKFence";
        case ObjectTypeTag::VKRenderCommandBuffer:    return "VKRenderCmdBuf";
        case ObjectTypeTag::VKTextureCommandBuffer:   return "VKTextureCmdBuf";
        case ObjectTypeTag::VKComputeCommandBuffer:   return "VKComputeCmdBuf";
        case ObjectTypeTag::VKBuffer:                 return "VKBuffer";
        case ObjectTypeTag::VKMemory:                 return "VKMemory";
        case ObjectTypeTag::VKImage:                  return "VKImage";
        case ObjectTypeTag::VKImageView:              return "VKImageView";
        case ObjectTypeTag::VKSampler:                return "VKSampler";
        case ObjectTypeTag::VKFramebuffer:            return "VKFramebuffer";
        case ObjectTypeTag::VKRenderPass:             return "VKRenderPass";
        case ObjectTypeTag::VKPipeline:               return "VKPipeline";
        case ObjectTypeTag::VKPipelineLayout:         return "VKPipelineLayout";
        case ObjectTypeTag::VKDescriptorSet:          return "VKDescriptorSet";
        case ObjectTypeTag::VKDescriptorSetLayout:    return "VKDescriptorSetLayout";
        case ObjectTypeTag::VKShaderModule:           return "VKShaderModule";
        case ObjectTypeTag::VKSwapchain:              return "VKSwapchain";
        case ObjectTypeTag::RenderTarget:           return "RT";
        case ObjectTypeTag::Texture:                return "Texture";
        case ObjectTypeTag::ShaderProgram:               return "ShaderProgram";
        case ObjectTypeTag::MaterialInstance:       return "MaterialInstance";
        case ObjectTypeTag::Mesh:                   return "Mesh";
        case ObjectTypeTag::IndirectDrawBuffer:     return "IndirectDrawBuf";
        case ObjectTypeTag::IndirectDrawIndexedBuffer: return "IndirectDrawIdxBuf";
        case ObjectTypeTag::IndirectDispatchBuffer: return "IndirectDispatchBuf";
        case ObjectTypeTag::VertexBuffer:           return "VertexBuf";
        case ObjectTypeTag::IndexBuffer:            return "IndexBuf";
        case ObjectTypeTag::UniformBuffer:          return "UniformBuf";
        case ObjectTypeTag::StorageBuffer:          return "StorageBuf";
        case ObjectTypeTag::TextureBuffer:          return "TextureBuf";
        case ObjectTypeTag::ReadbackBuffer:         return "ReadbackBuf";
        case ObjectTypeTag::RenderSystem:           return "RenderSys";
        case ObjectTypeTag::BatchSystem:            return "BatchSys";
        case ObjectTypeTag::CommandRecorder:        return "CmdRecorder";
        case ObjectTypeTag::FrameResource:          return "FrameRes";
        case ObjectTypeTag::SwapchainFrame:         return "SwapchainFrame";
        default:                                    return "";
    }
}

}  // namespace hgl::core
