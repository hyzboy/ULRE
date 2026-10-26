#ifndef HGL_VULKAN_GRAPH_FENCE_INCLUDE
#define HGL_VULKAN_GRAPH_FENCE_INCLUDE

#include<hgl/vk/VK.h>
#include<hgl/object/Object.h>

namespace hgl::graph{

/**
 * Fence - Vulkan栅栏对象的RAII包装
 *
 * 继承 hgl::Object：构造即分配对象唯一ID并绑定logger身份；
 * 销毁时由 OnRelease() 调用 vkDestroyFence 并向设备注销追踪记录。
 */
class Fence:public hgl::Object
{
public:

    HGL_OBJECT(Fence,hgl::Object)

    friend class DeviceQueue;

    VkDevice device;
    VkFence fence;
    bool queue_submitted = false;

protected:

    bool OnInit() override { return(true); }

    void OnRelease() override;

private:

    friend class VulkanDevice;

    Fence(VkDevice d,VkFence f,const OSString &fence_name=OS_TEXT("Fence"));

public:

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
