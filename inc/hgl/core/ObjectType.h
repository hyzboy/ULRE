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
    
    // Vulkan resources
    Queue,
    Semaphore,
    Fence,
    RenderCommandBuffer,
    TextureCommandBuffer,
    ComputeCommandBuffer,
    Buffer,
    Memory,
    Image,
    ImageView,
    Sampler,
    Framebuffer,
    RenderPass,
    Pipeline,
    PipelineLayout,
    DescriptorSet,
    DescriptorSetLayout,
    ShaderModule,
    Swapchain,
    
    // Custom types (for logging)
    RenderTarget,
    Texture,
    Material,
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
        case ObjectTypeTag::Queue:                  return "Queue";
        case ObjectTypeTag::Semaphore:              return "Semaphore";
        case ObjectTypeTag::Fence:                  return "Fence";
        case ObjectTypeTag::RenderCommandBuffer:    return "RenderCmdBuf";
        case ObjectTypeTag::TextureCommandBuffer:   return "TextureCmdBuf";
        case ObjectTypeTag::ComputeCommandBuffer:   return "ComputeCmdBuf";
        case ObjectTypeTag::Buffer:                 return "Buffer";
        case ObjectTypeTag::Memory:                 return "Memory";
        case ObjectTypeTag::Image:                  return "Image";
        case ObjectTypeTag::ImageView:              return "ImageView";
        case ObjectTypeTag::Sampler:                return "Sampler";
        case ObjectTypeTag::Framebuffer:            return "Framebuffer";
        case ObjectTypeTag::RenderPass:             return "RenderPass";
        case ObjectTypeTag::Pipeline:               return "Pipeline";
        case ObjectTypeTag::PipelineLayout:         return "PipelineLayout";
        case ObjectTypeTag::DescriptorSet:          return "DescriptorSet";
        case ObjectTypeTag::DescriptorSetLayout:    return "DescriptorSetLayout";
        case ObjectTypeTag::ShaderModule:           return "ShaderModule";
        case ObjectTypeTag::Swapchain:              return "Swapchain";
        case ObjectTypeTag::RenderTarget:           return "RT";
        case ObjectTypeTag::Texture:                return "Texture";
        case ObjectTypeTag::Material:               return "Material";
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
