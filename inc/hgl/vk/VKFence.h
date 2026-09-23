#ifndef HGL_VULKAN_GRAPH_FENCE_INCLUDE
#define HGL_VULKAN_GRAPH_FENCE_INCLUDE

#include<hgl/vk/VK.h>
#include<hgl/object/ObjectBase.h>
#include<source_location>

namespace hgl::graph{

/**
 * Fence - Vulkan栅栏对象的RAII包装
 * 现已继承ObjectBase以支持对象追踪
 */
class Fence : public hgl::utils::ObjectBase
{
    VkDevice device;
    VkFence fence;

private:

    friend class VulkanDevice;

    /**
     * 构造函数 - 强制传入创建位置以支持追踪
     */
    Fence(
        VkDevice d,
        VkFence f,
        const std::string& fence_name = "Fence",
        const std::source_location& loc = std::source_location::current()
    ) : ObjectBase(hgl::core::ObjectTypeTag::VKFence, fence_name, loc)
        , device(d)
        , fence(f)
    {
    }

public:

    /**
     * 析构函数 - 自动记录销毁位置
     */
    virtual ~Fence() override;

    /**
     * 隐式转换为VkFence
     */
    operator VkFence() const { return fence; }

    /**
     * 获取VkDevice
     */
    VkDevice GetDevice() const noexcept { return device; }

    /**
     * 获取VkFence
     */
    VkFence GetHandle() const noexcept { return fence; }

    VkResult GetStatus() const noexcept { return vkGetFenceStatus(device, fence); }
    VkResult Wait(uint64_t timeout = UINT64_MAX) const noexcept { return vkWaitForFences(device, 1, &fence, VK_TRUE, timeout); }
    VkResult Reset() const noexcept { return vkResetFences(device, 1, &fence); }

};//class Fence

}//namespace hgl::graph
#endif//HGL_VULKAN_GRAPH_FENCE_INCLUDE
