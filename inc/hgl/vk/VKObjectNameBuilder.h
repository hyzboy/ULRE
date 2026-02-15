#pragma once

#include<hgl/vk/VKNamespace.h>
#include<hgl/type/String.h>
#include<cstdint>
#include<cstring>

VK_NAMESPACE_BEGIN

/**
 * 对象类型标签，用于层级命名
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
};

/**
 * 对象名字构建器
 * 使用轻量级结构延迟计算完整名字，只在真正需要输出时才生成字符串
 */
struct ObjectNameBuilder
{
    char base_name[32];         // 基础名字（定长，避免动态分配）
    ObjectTypeTag tags[16];     // 类型标签数组（最多16层）
    uint8_t depth;              // 当前层级深度

    // 构造函数：从C字符串创建
    ObjectNameBuilder(const char* name = "")
        : tags{}, depth(0)
    {
        std::strncpy(base_name, name, sizeof(base_name) - 1);
        base_name[sizeof(base_name) - 1] = '\0';
    }

    // 构造函数：从AnsiString创建
    ObjectNameBuilder(const AnsiString& name)
        : ObjectNameBuilder(name.c_str())
    {
    }

    // 追加一个类型标签（不修改当前对象，返回新对象）
    ObjectNameBuilder Append(ObjectTypeTag tag) const
    {
        ObjectNameBuilder result = *this;
        if (result.depth < 16)
        {
            result.tags[result.depth] = tag;
            result.depth++;
        }
        return result;
    }

    // 获取类型标签的字符串表示
    static const char* GetTagString(ObjectTypeTag tag)
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
            default:                                    return "";
        }
    }

    // 生成完整的层级名字（只在需要输出时调用）
    AnsiString ToString() const
    {
        AnsiString result(base_name);
        
        for (uint8_t i = 0; i < depth; ++i)
        {
            if (tags[i] != ObjectTypeTag::None)
            {
                result += ":";
                result += GetTagString(tags[i]);
            }
        }
        
        return result;
    }

    // 类型转换操作符（方便使用）
    operator AnsiString() const
    {
        return ToString();
    }
};

VK_NAMESPACE_END
