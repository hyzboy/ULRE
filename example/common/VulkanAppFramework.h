#pragma once
#include<hgl/platform/Window.h>
#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSemaphore.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKImageView.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKDescriptorSets.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

class VulkanApplicationFramework
{
private:

            Window *    win             =nullptr;
    VulkanInstance *    inst            =nullptr;

    void OnKeyPressed   (KeyboardButton kb){key_status[kb]=true;}
    void OnKeyReleased  (KeyboardButton kb){key_status[kb]=false;}
    void OnKeyRepeat    (KeyboardButton kb){KeyRepeat(kb);}

protected:

    uint        mouse_key=0;
    Vector2f    mouse_pos;

    void OnMousePressed (int,int,uint mk){mouse_key=mk;MousePressed(mk);}
    void OnMouseReleased(int,int,uint mk){mouse_key=0;MouseReleased(mk);}
    void OnMouseMove    (int x,int y){mouse_pos.Set(x,y);MouseMove();}
    void OnMouseWheel   (int v,int h,uint mk){MouseWheel(v,h,mk);}

protected:

    GPUDevice *             device                      =nullptr;
    SwapchainRenderTarget * sc_render_target            =nullptr;

protected:

            int32_t         swap_chain_count            =0;

    RenderCmdBuffer **      cmd_buf                     =nullptr;

            Color4f         clear_color;

protected:

    RenderResource *        db                          =nullptr;

            bool            key_status[kbRangeSize];

public:

    virtual ~VulkanApplicationFramework()
    {
        SAFE_CLEAR(db);
        SAFE_CLEAR_OBJECT_ARRAY(cmd_buf,swap_chain_count);

        SAFE_CLEAR(device);
        SAFE_CLEAR(win);
        SAFE_CLEAR(inst);
    }

    virtual bool Init(int w,int h)
    {
        hgl_zero(key_status);

        clear_color.Zero();

    #ifdef _DEBUG
        if(!CheckStrideBytesByFormat())
            return(false);
    #endif//

        InitNativeWindowSystem();

        InitVulkanInstanceProperties();

        win=CreateRenderWindow(OS_TEXT("VulkanTest"));
        if(!win)
            return(false);

        if(!win->Create(w,h))
            return(false);

        {
            CreateInstanceLayerInfo cili;

            hgl_zero(cili);

            cili.lunarg.standard_validation = true;
            cili.khronos.validation = true;
//            cili.RenderDoc.Capture = true;

            inst=CreateInstance("VulkanTest",nullptr,&cili);

            if(!inst)
                return(false);
        }

        device=CreateRenderDevice(inst,win);

        if(!device)
            return(false);

        db=new RenderResource(device);

        InitCommandBuffer();

        SetEventCall(win->OnResize,         this,VulkanApplicationFramework,OnResize        );
        SetEventCall(win->OnKeyPressed,     this,VulkanApplicationFramework,OnKeyPressed    );
        SetEventCall(win->OnKeyReleased,    this,VulkanApplicationFramework,OnKeyReleased   );
        SetEventCall(win->OnKeyRepeat,      this,VulkanApplicationFramework,OnKeyRepeat     );

        SetEventCall(win->OnMousePressed,   this,VulkanApplicationFramework,OnMousePressed  );
        SetEventCall(win->OnMouseReleased,  this,VulkanApplicationFramework,OnMouseReleased );
        SetEventCall(win->OnMouseMove,      this,VulkanApplicationFramework,OnMouseMove     );
        SetEventCall(win->OnMouseWheel,     this,VulkanApplicationFramework,OnMouseWheel    );

        return(true);
    }

    virtual void Resize(int,int)=0;
    virtual void KeyRepeat(KeyboardButton){}
    virtual void MousePressed(uint){}
    virtual void MouseReleased(uint){}
    virtual void MouseMove(){}
    virtual void MouseWheel(int,int,uint){}

    void SetClearColor(COLOR cc)
    {
        clear_color.Use(cc,1.0);
    }

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

            cmd_buf=hgl_zero_new<RenderCmdBuffer *>(swap_chain_count);

            for(int32_t i=0;i<swap_chain_count;i++)
                cmd_buf[i]=device->CreateRenderCommandBuffer();
        }
    }

    bool BuildCommandBuffer(RenderCmdBuffer *cb,RenderPass *rp,Framebuffer *fb,RenderableInstance *ri)
    {   
        if(!ri)return(false);

        const IndexBuffer *ib=ri->GetIndexBuffer();

        cb->Begin();
            cb->BindFramebuffer(rp,fb);
            cb->SetClearColor(0,clear_color.r,clear_color.g,clear_color.b);
            cb->BeginRenderPass();
                cb->BindPipeline(ri->GetPipeline());
                cb->BindDescriptorSets(ri->GetDescriptorSets());
                cb->BindVAB(ri);

                    if (ib)
                        cb->DrawIndexed(ib->GetCount());
                    else
                        cb->Draw(ri->GetDrawCount());

            cb->EndRenderPass();
        cb->End();

        return(true);
    }

    void BuildCommandBuffer(RenderCmdBuffer *cb,RenderTarget *rt,RenderableInstance *ri)
    {
        if(!cb||!rt||!ri)
            return;

        BuildCommandBuffer(cb,rt->GetRenderPass(),rt->GetFramebuffer(),ri);
    }

    bool BuildCommandBuffer(uint32_t index,RenderableInstance *ri)
    {   
        if(!ri)return(false);

        return BuildCommandBuffer(cmd_buf[index],sc_render_target->GetRenderPass(),sc_render_target->GetFramebuffer(index),ri);
    }

    bool BuildCommandBuffer(RenderableInstance *ri)
    {
        if(!ri)return(false);

        for(int32_t i=0;i<swap_chain_count;i++)
            BuildCommandBuffer(i,ri);

        return(true);
    }

    bool BuildCurrentCommandBuffer(RenderableInstance *ri)
    {
        if(!ri)return(false);
    
        return BuildCommandBuffer(sc_render_target->GetCurrentFrameIndices(),ri);
    }

    void BuildCommandBuffer(uint32_t index,RenderList *rl)
    {
        if(!rl)return;

        RenderCmdBuffer *cb=cmd_buf[index];

        cb->Begin();
        cb->BindFramebuffer(sc_render_target->GetRenderPass(),sc_render_target->GetFramebuffer(index));        
        cb->SetClearColor(0,clear_color.r,clear_color.g,clear_color.b);
            cb->BeginRenderPass();
                rl->Render(cb);
            cb->EndRenderPass();
        cb->End();
    }

    void BuildCommandBuffer(RenderList *rl)
    {
        for(int32_t i=0;i<swap_chain_count;i++)
            BuildCommandBuffer(i,rl);
    }

    void BuildCurrentCommandBuffer(RenderList *rl)
    {
        BuildCommandBuffer(sc_render_target->GetCurrentFrameIndices(),rl);
    }

    template<typename ...ARGS>
    Pipeline *CreatePipeline(ARGS...args){return sc_render_target->CreatePipeline(args...);}

public:

    int AcquireNextImage()
    {
        return sc_render_target->AcquireNextImage();
    }

    virtual void SubmitDraw(int index)
    {
        VkCommandBuffer cb=*cmd_buf[index];

        sc_render_target->Submit(cb);
        sc_render_target->PresentBackbuffer();
        sc_render_target->WaitQueue();
        sc_render_target->WaitFence();
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

    GPUBuffer *         ubo_world_matrix    =nullptr;

protected:

    WalkerCamera *      camera;
    float               move_speed=1;

    Vector2f            mouse_last_pos;

public:

    virtual ~CameraAppFramework()
    {
        SAFE_CLEAR(camera);
    }

    virtual bool Init(int w,int h)
    {
        if(!VulkanApplicationFramework::Init(w,h))
            return(false);

        InitCamera(w,h);
        return(true);
    }

    virtual void InitCamera(int w,int h)
    {
        camera=new WalkerCamera;

        camera->type=CameraType::Perspective;
        camera->width=w;
        camera->height=h;
        camera->vp_width=w;
        camera->vp_height=h;

        camera->pos=Vector4f(10,10,10,1);
        camera->target=Vector4f(0,0,0,1);

        camera->Refresh();      //更新矩阵计算
        
        ubo_world_matrix=db->CreateUBO(sizeof(WorldMatrix),&camera->matrix);
    }

    void Resize(int w,int h)override
    {
        camera->width=w;
        camera->height=h;
    }

    GPUBuffer *GetCameraMatrixBuffer()
    {
        return ubo_world_matrix;
    }

    virtual void BuildCommandBuffer(uint32_t index)=0;

    virtual void Draw()override
    {
        camera->Refresh();                           //更新相机矩阵
        ubo_world_matrix->Write(&camera->matrix);    //写入缓冲区

        const uint32_t index=AcquireNextImage();

        BuildCommandBuffer(index);

        SubmitDraw(index);

        if(key_status[kbW])camera->Up       (move_speed);else
        if(key_status[kbS])camera->Down     (move_speed);else
        if(key_status[kbA])camera->Left     (move_speed);else
        if(key_status[kbD])camera->Right    (move_speed);else
        if(key_status[kbR])camera->Forward  (move_speed);else
        if(key_status[kbF])camera->Backward (move_speed);else

        if(key_status[kbLeft    ])camera->HoriRotate( move_speed);else
        if(key_status[kbRight   ])camera->HoriRotate(-move_speed);else
        if(key_status[kbUp      ])camera->VertRotate( move_speed);else
        if(key_status[kbDown    ])camera->VertRotate(-move_speed);else
            return;
    }

    virtual void KeyRepeat(KeyboardButton kb)override
    {
        if(kb==kbMinus)move_speed*=0.9f;else
        if(kb==kbEquals)move_speed*=1.1f;else
            return;
    }

    virtual void MousePressed(uint) override
    {
        mouse_last_pos=mouse_pos;
    }

    virtual void MouseMove() override
    {
        if(!(mouse_key&(mbLeft|mbRight)))return;

        Vector2f gap=mouse_pos-mouse_last_pos;

        bool update=false;
        if(gap.x!=0){update=true;if(mouse_key&mbLeft)camera->HoriRotate( gap.x/10.0f);else camera->WrapHoriRotate(gap.x);}
        if(gap.y!=0){update=true;if(mouse_key&mbLeft)camera->VertRotate(-gap.y/10.0f);else camera->WrapVertRotate(gap.y);}

        mouse_last_pos=mouse_pos;
    }

    virtual void MouseWheel(int v,int h,uint)
    {
        camera->Distance(1+(v/1000.0f));
    }
};//class CameraAppFramework
