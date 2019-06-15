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

    void OnResize(int w,int h)
    {
        InitCommandBuffer();
        Resize(w,h);
    }

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

    vulkan::Device *            device          =nullptr;
    vulkan::ShaderModuleManage *shader_manage   =nullptr;

private:

            uint32_t            swap_chain_count=0;

    vulkan::CommandBuffer **    cmd_buf         =nullptr;

protected:

            SceneDB *           db              =nullptr;

            bool                key_status[kbRangeSize];

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
        hgl_zero(key_status);

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

public:

    virtual void Draw()
    {
        uint32_t index=device->GetCurrentFrameIndices();

        VkCommandBuffer cb=*cmd_buf[index];
        
        device->SubmitDraw(&cb);
        device->Wait(&index);
        device->QueuePresent();
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

        InitCamera();
        return(true);
    }

    virtual void InitCamera()
    {
        camera.type=CameraType::Perspective;
        camera.center.Set(0,0,0,1);
        camera.eye.Set(100,100,100,1);

        camera.Refresh();      //更新矩阵计算
    }

    bool InitCameraUBO(vulkan::DescriptorSets *desc_set,uint world_matrix_bindpoint)
    {
        InitCamera();

        const VkExtent2D extent=device->GetExtent();

        camera.width=extent.width;
        camera.height=extent.height;

        ubo_world_matrix=db->CreateUBO(sizeof(WorldMatrix),&camera.matrix);

        if(!ubo_world_matrix)
            return(false);

        return desc_set->BindUBO(world_matrix_bindpoint,*ubo_world_matrix);
    }

    virtual void Draw()override
    {
        camera.Refresh();                           //更新相机矩阵
        ubo_world_matrix->Write(&camera.matrix);    //写入缓冲区

        VulkanApplicationFramework::Draw();

        if(key_status[kbW])camera.Forward   (move_speed);else
        if(key_status[kbS])camera.Backward  (move_speed);else
        if(key_status[kbA])camera.Left      (move_speed);else
        if(key_status[kbD])camera.Right     (move_speed);else
        if(key_status[kbR])camera.Up        (move_speed);else
        if(key_status[kbF])camera.Down      (move_speed);else

        if(key_status[kbLeft    ])camera.WrapHorzRotate(move_speed);else
        if(key_status[kbRight   ])camera.WrapHorzRotate(-move_speed);else
        if(key_status[kbUp      ])camera.WrapVertRotate(move_speed);else
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
        if(gap.x!=0){update=true;if(mouse_key&mbLeft)camera.HorzRotate(-gap.x);else camera.WrapHorzRotate(gap.x);}
        if(gap.y!=0){update=true;if(mouse_key&mbLeft)camera.VertRotate(-gap.y);else camera.WrapVertRotate(gap.y);}

        mouse_last_pos=mouse_pos;
    }

    virtual void MouseWheel(int v,int h,uint)
    {
        camera.Distance(1+(v/1000.0f));
    }
};//class CameraAppFramework
