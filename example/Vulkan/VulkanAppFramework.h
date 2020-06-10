#pragma once
#include<hgl/platform/Window.h>
#include<hgl/graph/vulkan/VKInstance.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKSemaphore.h>
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
#include<hgl/graph/vulkan/VKMaterialInstance.h>
#include<hgl/graph/vulkan/VKRenderTarget.h>
#include<hgl/graph/SceneDB.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

class VulkanApplicationFramework
{
private:

            Window *            win             =nullptr;
    vulkan::Instance *          inst            =nullptr;

    void OnKeyDown  (KeyboardButton kb){key_status[kb]=true;}
    void OnKeyUp    (KeyboardButton kb){key_status[kb]=false;}
    void OnKeyPress (KeyboardButton kb){KeyPress(kb);}

protected:

    uint        mouse_key=0;
    Vector2f    mouse_pos;

    void OnMouseDown(int,int,uint mk){mouse_key=mk;MouseDown(mk);}
    void OnMouseUp  (int,int,uint mk){mouse_key=0;MouseUp(mk);}
    void OnMouseMove(int x,int y){mouse_pos.Set(x,y);MouseMove();}
    void OnMouseWheel(int v,int h,uint mk){MouseWheel(v,h,mk);}

protected:

    vulkan::Device *                device                      =nullptr;
    vulkan::SwapchainRenderTarget * sc_render_target            =nullptr;

    vulkan::Semaphore *             present_complete_semaphore  =nullptr,
                      *             render_complete_semaphore   =nullptr;

    vulkan::ShaderModuleManage *    shader_manage               =nullptr;

protected:

            int32_t                 swap_chain_count            =0;

    vulkan::CommandBuffer **        cmd_buf                     =nullptr;

protected:

            SceneDB *           db                              =nullptr;

            bool                key_status[kbRangeSize];

public:

    virtual ~VulkanApplicationFramework()
    {
        SAFE_CLEAR(present_complete_semaphore);
        SAFE_CLEAR(render_complete_semaphore);

        SAFE_CLEAR(db);
        SAFE_CLEAR_OBJECT_ARRAY(cmd_buf,swap_chain_count);

        SAFE_CLEAR(shader_manage);
        SAFE_CLEAR(device);
        SAFE_CLEAR(win);
        SAFE_CLEAR(inst);
    }

    virtual bool Init(int w,int h)
    {
        hgl_zero(key_status);

    #ifdef _DEBUG
        if(!vulkan::CheckStrideBytesByFormat())
            return(false);
    #endif//

        InitNativeWindowSystem();

        VK_NAMESPACE::InitVulkanProperties();

        win=CreateRenderWindow(OS_TEXT("VulkanTest"));
        if(!win)
            return(false);

        if(!win->Create(w,h))
            return(false);

        {
            VK_NAMESPACE::CreateInstanceLayerInfo cili;

            memset(&cili, 0, sizeof(VK_NAMESPACE::CreateInstanceLayerInfo));

            cili.lunarg.standard_validation = true;
            cili.khronos.validation = true;
            cili.RenderDoc.Capture = true;

            inst=vulkan::CreateInstance(U8_TEXT("VulkanTest"));

            if(!inst)
                return(false);
        }

        device=CreateRenderDevice(inst,win);

        if(!device)
            return(false);

        present_complete_semaphore  =device->CreateSem();
        render_complete_semaphore   =device->CreateSem();

        shader_manage=device->CreateShaderModuleManage();
        db=new SceneDB(device);

        InitCommandBuffer();

        SetEventCall(win->OnResize,     this,VulkanApplicationFramework,OnResize    );
        SetEventCall(win->OnKeyDown,    this,VulkanApplicationFramework,OnKeyDown   );
        SetEventCall(win->OnKeyUp,      this,VulkanApplicationFramework,OnKeyUp     );
        SetEventCall(win->OnKeyPress,   this,VulkanApplicationFramework,OnKeyPress  );

        SetEventCall(win->OnMouseDown,  this,VulkanApplicationFramework,OnMouseDown );
        SetEventCall(win->OnMouseUp,    this,VulkanApplicationFramework,OnMouseUp   );
        SetEventCall(win->OnMouseMove,  this,VulkanApplicationFramework,OnMouseMove );
        SetEventCall(win->OnMouseWheel, this,VulkanApplicationFramework,OnMouseWheel);

        return(true);
    }

    virtual void Resize(int,int)=0;
    virtual void KeyPress(KeyboardButton){}
    virtual void MouseDown(uint){}
    virtual void MouseUp(uint){}
    virtual void MouseMove(){}
    virtual void MouseWheel(int,int,uint){}

    void OnResize(int w,int h)
    {
        if(w>0&&h>0)
            device->Resize(w,h);

        InitCommandBuffer();
        Resize(w,h);
    }

    void InitCommandBuffer()
    {
        if(cmd_buf)
            SAFE_CLEAR_OBJECT_ARRAY(cmd_buf,swap_chain_count);

        sc_render_target=device->GetSwapchainRT();

        swap_chain_count=sc_render_target->GetImageCount();
        {
            const VkExtent2D extent=sc_render_target->GetExtent();

            cmd_buf=hgl_zero_new<vulkan::CommandBuffer *>(swap_chain_count);

            for(uint i=0;i<swap_chain_count;i++)
                cmd_buf[i]=device->CreateCommandBuffer(extent,2);
        }
    }

    void BuildCommandBuffer(uint32_t index,vulkan::Pipeline *p,vulkan::MaterialInstance *mi,vulkan::Renderable *r)
    {
        if(!p||!mi||!r)
            return;

        const vulkan::IndexBuffer *ib=r->GetIndexBuffer();

        vulkan::CommandBuffer *cb=cmd_buf[index];

        cb->Begin();
        cb->BeginRenderPass(sc_render_target->GetRenderPass(),sc_render_target->GetFramebuffer(index));
        cb->Bind(p);
        cb->Bind(mi->GetDescriptorSets());
        cb->Bind(r);

        if (ib)
            cb->DrawIndexed(ib->GetCount());
        else
            cb->Draw(r->GetDrawCount());

        cb->EndRenderPass();
        cb->End();
    }

    void BuildCommandBuffer(vulkan::Pipeline *p,vulkan::MaterialInstance *mi,vulkan::Renderable *r)
    {
        for(uint32_t i=0;i<swap_chain_count;i++)
            BuildCommandBuffer(i,p,mi,r);
    }

    void BuildCurrentCommandBuffer(vulkan::Pipeline *p,vulkan::MaterialInstance *mi,vulkan::Renderable *r)
    {
        BuildCommandBuffer(sc_render_target->GetCurrentFrameIndices(),p,mi,r);
    }

    void BuildCommandBuffer(uint32_t index,RenderList *rl)
    {
        if(!rl)return;

        vulkan::CommandBuffer *cb=cmd_buf[index];

        cb->Begin();
        cb->BeginRenderPass(sc_render_target->GetRenderPass(),sc_render_target->GetFramebuffer(index));
        rl->Render(cb);
        cb->EndRenderPass();
        cb->End();
    }

    void BuildCommandBuffer(RenderList *rl)
    {
        for(uint32_t i=0;i<swap_chain_count;i++)
            BuildCommandBuffer(i,rl);
    }

    void BuildCurrentCommandBuffer(RenderList *rl)
    {
        BuildCommandBuffer(sc_render_target->GetCurrentFrameIndices(),rl);
    }

public:

    int AcquireNextImage()
    {
        return sc_render_target->AcquireNextImage(present_complete_semaphore);
    }

    virtual void SubmitDraw(int index)
    {
        VkCommandBuffer cb=*cmd_buf[index];

        sc_render_target->Submit(cb,present_complete_semaphore,render_complete_semaphore);
        sc_render_target->PresentBackbuffer(render_complete_semaphore);
        sc_render_target->Wait();
    }

    virtual void Draw()
    {
        int index=AcquireNextImage();

        if(index<0||index>=swap_chain_count)return;

        SubmitDraw(index);
    }

    bool Run()
    {
        if(!win->Update())return(false);

        if(win->IsVisible())
            Draw();

        return(true);
    }
};//class VulkanApplicationFramework

class CameraAppFramework:public VulkanApplicationFramework
{
private:

    vulkan::Buffer *            ubo_world_matrix    =nullptr;

protected:

    ControlCamera               camera;
    float                       move_speed=1;

    Vector2f                    mouse_last_pos;

public:

    virtual ~CameraAppFramework()=default;

    virtual bool Init(int w,int h)
    {
        if(!VulkanApplicationFramework::Init(w,h))
            return(false);

        InitCamera(w,h);
        return(true);
    }

    virtual void InitCamera(int w,int h)
    {
        camera.type=CameraType::Perspective;
        camera.width=w;
        camera.height=h;
        camera.center.Set(0,0,0,1);
        camera.eye.Set(100,100,100,1);      //xyz三个值不要一样，以方便调试
        camera.znear=16;
        camera.zfar=256;

        camera.Refresh();      //更新矩阵计算
    }

    void Resize(int w,int h)override
    {
        camera.width=w;
        camera.height=h;
    }

    bool InitCameraUBO(vulkan::MaterialInstance *mi,const UTF8String &node_name)
    {
        ubo_world_matrix=db->CreateUBO(sizeof(WorldMatrix),&camera.matrix);

        if(!ubo_world_matrix)
            return(false);

        return mi->BindUBO(node_name,ubo_world_matrix);
    }

    virtual void BuildCommandBuffer(uint32_t index)=0;

    virtual void Draw()override
    {
        camera.Refresh();                           //更新相机矩阵
        ubo_world_matrix->Write(&camera.matrix);    //写入缓冲区

        const uint32_t index=AcquireNextImage();

        BuildCommandBuffer(index);

        SubmitDraw(index);

        if(key_status[kbW])camera.Forward   (move_speed);else
        if(key_status[kbS])camera.Backward  (move_speed);else
        if(key_status[kbA])camera.Left      (move_speed);else
        if(key_status[kbD])camera.Right     (move_speed);else
        if(key_status[kbR])camera.Up        (move_speed);else
        if(key_status[kbF])camera.Down      (move_speed);else

        if(key_status[kbLeft    ])camera.WrapHorzRotate( move_speed);else
        if(key_status[kbRight   ])camera.WrapHorzRotate(-move_speed);else
        if(key_status[kbUp      ])camera.WrapVertRotate( move_speed);else
        if(key_status[kbDown    ])camera.WrapVertRotate(-move_speed);else
            return;
    }

    virtual void KeyPress(KeyboardButton kb)override
    {
        if(kb==kbMinus)move_speed*=0.9f;else
        if(kb==kbEquals)move_speed*=1.1f;else
            return;
    }

    virtual void MouseDown(uint) override
    {
        mouse_last_pos=mouse_pos;
    }

    virtual void MouseMove() override
    {
        if(!(mouse_key&(mbLeft|mbRight)))return;

        Vector2f gap=mouse_pos-mouse_last_pos;

        bool update=false;
        if(gap.x!=0){update=true;if(mouse_key&mbLeft)camera.HorzRotate(-gap.x/10.0f);else camera.WrapHorzRotate(gap.x);}
        if(gap.y!=0){update=true;if(mouse_key&mbLeft)camera.VertRotate(-gap.y/10.0f);else camera.WrapVertRotate(gap.y);}

        mouse_last_pos=mouse_pos;
    }

    virtual void MouseWheel(int v,int h,uint)
    {
        camera.Distance(1+(v/1000.0f));
    }
};//class CameraAppFramework
