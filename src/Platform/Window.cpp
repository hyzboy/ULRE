#include<hgl/platform/Window.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKInstance.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>

VK_NAMESPACE_BEGIN
Device *CreateRenderDevice(VkInstance inst,const PhysicalDevice *physical_device,VkSurfaceKHR surface,const VkExtent2D &);
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

        VkExtent2D extent={width,height};

        device=VK_NAMESPACE::CreateRenderDevice(*vk_inst,pd,surface,extent);

        if(!device)
        {
            vkDestroySurfaceKHR(*vk_inst,surface,nullptr);
            return(nullptr);
        }

        active=true;

        return(device);
    }

    void Window::CloseRenderDevice()
    {
        SAFE_CLEAR(device);
    }

    void Window::ProcKeyDown(KeyboardButton kb)
    {
        if(key_push[kb])
            ProcKeyPress(kb);
        else
            key_push[kb]=true;

       SafeCallEvent(OnKeyDown,(kb));
    }

    void Window::ProcKeyUp(KeyboardButton kb)
    {
        key_push[kb]=false;

        SafeCallEvent(OnKeyUp,(kb));
    }

    void Window::ProcResize(uint w,uint h)
    {
        if(w==width&&height==h)
            return;

        width=w;
        height=h;

        if(w==0||h==0)
        {
            is_min=true;
        }
        else
        {
            is_min=false;

            if(device)
                device->Resize(width,height);
        }

        SafeCallEvent(OnResize,(w,h));
    }

    void Window::ProcActive(bool a)
    {
        active=a; 
        SafeCallEvent(OnActive,(a));
    }

    void Window::ProcClose()
    { 
        is_close=true; 
        SafeCallEvent(OnClose,());
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
