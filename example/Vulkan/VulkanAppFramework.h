#pragma once
#include<hgl/platform/Window.h>
#include<hgl/graph/vulkan/VKInstance.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKBuffer.h>
#include<hgl/graph/vulkan/VKShaderModule.h>
#include<hgl/graph/vulkan/VKShaderModuleManage.h>
#include<hgl/graph/vulkan/VKImageView.h>
#include<hgl/graph/vulkan/VKRenderable.h>
#include<hgl/graph/vulkan/VKDescriptorSets.h>
#include<hgl/graph/vulkan/VKRenderPass.h>
#include<hgl/graph/vulkan/VKPipeline.h>
#include<hgl/graph/vulkan/VKCommandBuffer.h>
#include<hgl/graph/vulkan/VKFormat.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>
#include<hgl/graph/vulkan/VKMaterial.h>

using namespace hgl;
using namespace hgl::graph;

class VulkanApplicationFramework
{
private:    

            Window *            win             =nullptr;
    vulkan::Instance *          inst            =nullptr;

protected:

    vulkan::Device *            device          =nullptr;
    vulkan::ShaderModuleManage *shader_manage   =nullptr;

public:

    virtual ~VulkanApplicationFramework()
    {   
        SAFE_CLEAR(shader_manage);
        SAFE_CLEAR(win);        //win中会删除device，所以必须放在instance前删除
        SAFE_CLEAR(inst);
    }

    virtual bool Init(int w,int h) 
    {
    #ifdef _DEBUG
        if(!vulkan::CheckStrideBytesByFormat())
            return(false);
    #endif//

        InitNativeWindowSystem();

        win=CreateRenderWindow(OS_TEXT("VulkanTest"));
        if(!win)
            return(false);

        if(!win->Create(w,h))
            return(false);

        inst=vulkan::CreateInstance(U8_TEXT("VulkanTest"));

        if(!inst)
            return(false);

        device=win->CreateRenderDevice(inst);

        if(!device)
            return(false);

        shader_manage=device->CreateShaderModuleManage();
        
        const vulkan::PhysicalDevice *render_device=device->GetPhysicalDevice();

        std::cout<<"auto select physical device: "<<render_device->GetDeviceName()<<std::endl;
        return(true);
    }

    void AcquireNextFrame()
    {
        device->AcquireNextImage();
    }

    void Submit(const VkCommandBuffer cmd_buf)
    {
        device->SubmitDraw(&cmd_buf);
        device->Wait();
        device->QueuePresent();
    }

    virtual void Draw()=0;

    bool Run()
    {
        if(!win->Update())return(false);

        if(win->IsVisible())
        {
            AcquireNextFrame();
            Draw();
        }

        return(true);
    }
};//class VulkanApplicationFramework
