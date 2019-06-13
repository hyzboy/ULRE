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
#include<hgl/graph/SceneDB.h>
#include<hgl/graph/RenderList.h>

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

private:

            uint32_t            swap_chain_count=0;

    vulkan::CommandBuffer **    cmd_buf         =nullptr;
   
protected:

            SceneDB *           db              =nullptr;

public:

    virtual ~VulkanApplicationFramework()
    {
        SAFE_CLEAR(db);
        SAFE_CLEAR_OBJECT_ARRAY(cmd_buf,swap_chain_count);

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
        db=new SceneDB(device);

        InitCommandBuffer();

        return(true);
    }

    void InitCommandBuffer()
    {
        if(cmd_buf)
        {
            for(uint i=0;i<swap_chain_count;i++)
                delete cmd_buf[i];

            delete[] cmd_buf;
        }

        swap_chain_count=device->GetSwapChainImageCount();
        {
            cmd_buf=hgl_zero_new<vulkan::CommandBuffer *>(swap_chain_count);

            for(uint i=0;i<swap_chain_count;i++)
                cmd_buf[i]=device->CreateCommandBuffer();
        }
    }

    void BuildCommandBuffer(vulkan::Pipeline *p,vulkan::DescriptorSets *ds,vulkan::Renderable *r)
    {
        if(!p||!ds||!r)
            return;

        const vulkan::IndexBuffer *ib=r->GetIndexBuffer();

        for(uint i=0;i<swap_chain_count;i++)
        {
            cmd_buf[i]->Begin();
            cmd_buf[i]->BeginRenderPass(device->GetRenderPass(),device->GetFramebuffer(i));
            cmd_buf[i]->Bind(p);
            cmd_buf[i]->Bind(ds);
            cmd_buf[i]->Bind(r);

            if (ib)
                cmd_buf[i]->DrawIndexed(ib->GetCount());
            else
                cmd_buf[i]->Draw(r->GetDrawCount());

            cmd_buf[i]->EndRenderPass();
            cmd_buf[i]->End();
        }
    }

    void BuildCommandBuffer(RenderList *rl)
    {
        if(!rl)return;

        for(uint i=0;i<swap_chain_count;i++)
        {
            cmd_buf[i]->Begin();
            cmd_buf[i]->BeginRenderPass(device->GetRenderPass(),device->GetFramebuffer(i));
            rl->Render(cmd_buf[i]);
            cmd_buf[i]->EndRenderPass();
            cmd_buf[i]->End();
        }
    }

private:

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

public:

    virtual void Draw()
    {
        const vulkan::CommandBuffer *cb=cmd_buf[device->GetCurrentFrameIndices()];

        Submit(*cb);
    }

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
