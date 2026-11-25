#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKRenderTarget.h>

namespace hgl::graph{

VulkanDevice *IRenderTarget::GetDevice  ()const
{
    return device;
}

VkDevice IRenderTarget::GetVkDevice()const
{
    auto *device = GetDevice();
    return device ? device->GetDevice() : nullptr;
}

ViewportInfo *IRenderTarget::GetViewportInfo()
{
    return &viewport_info;
}

const ViewportInfo *IRenderTarget::GetViewportInfo() const
{
    return &viewport_info;
}

IRenderTarget::IRenderTarget(VulkanDevice *dev,const VkExtent2D &ext)
{
    device=dev;
    OnResize(ext);
}

IRenderTarget::~IRenderTarget() = default;

void IRenderTarget::OnResize(const VkExtent2D &ext)
{
    extent=ext;
    viewport_info.Set(ext.width, ext.height);

    // 系统级视口 UBO 的同步不在这里做：RT 只持设备、不认识 ECS 世界。
    // 活动 RT 的 UBO 更新由持有世界句柄的 `RenderTargetManager` 收口
    //（见 `SyncViewportExtentForActiveRT`）。
}

}//namespace hgl::graph
