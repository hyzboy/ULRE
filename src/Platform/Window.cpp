#include<hgl/platform/Window.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKInstance.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>

VK_NAMESPACE_BEGIN
Device *CreateRenderDevice(VkInstance inst,const PhysicalDevice *physical_device,VkSurfaceKHR surface,uint width,uint height);
VK_NAMESPACE_END

namespace hgl
{
    Window::~Window()
    {
        SAFE_CLEAR(device);
    }

    VK_NAMESPACE::Device *Window::CreateRenderDevice(VK_NAMESPACE::Instance *vk_inst,const VK_NAMESPACE::PhysicalDevice *pd)
    {
        if(device)
            return(device);

        if(!vk_inst)
            return(nullptr);

        if(!pd)pd=vk_inst->GetDevice(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);      //先找独显
        if(!pd)pd=vk_inst->GetDevice(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);    //再找集显
        if(!pd)pd=vk_inst->GetDevice(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU);       //最后找虚拟显卡

        if(!pd)
            return(nullptr);

        VkSurfaceKHR surface=CreateSurface(*vk_inst);

        if(!surface)
            return(nullptr);

        device=VK_NAMESPACE::CreateRenderDevice(*vk_inst,pd,surface,width,height);

        if(!device)
        {
            vkDestroySurfaceKHR(*vk_inst,surface,nullptr);
            return(nullptr);
        }

        return(device);
    }

    void Window::CloseRenderDevice()
    {
        SAFE_CLEAR(device);
    }

    void Window::OnKeyDown(KeyboardButton kb)
    {
        if(key_push[kb])
            OnKeyPress(kb);
        else
        {
            OnKeyDown(kb);
            key_push[kb]=true;
        }
    }

    void Window::OnKeyUp(KeyboardButton kb)
    {
        OnKeyUp(kb);
        key_push[kb]=false;
    }

    void Window::OnResize(uint w,uint h)
    {
        if(w==width&&height==h)
            return;

        if(w==0||h==0)
        {
            is_min=true;
        }
        else
        {
            is_min=false;
            width=w;
            height=h;

            if(device)
                device->Resize(width,height);
        }
    }

    bool Window::Update()
    {
        while(MessageProc())
        {
        }

        if(is_close)
            return(false);

        if(!active||is_min)
            this->WaitMessage();

        return(true);
    }
}//namespace hgl
