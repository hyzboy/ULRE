#pragma once
#include<hgl/platform/Window.h>
#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKDeviceCreater.h>
#include<hgl/graph/VKSemaphore.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKImageView.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKRenderResource.h>
#ifdef _DEBUG
#include<hgl/graph/VKDeviceAttribute.h>
#endif//_DEBUG
#include<hgl/graph/RenderList.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/color/Color.h>
#include<hgl/Time.h>
#include<hgl/log/LogInfo.h>

//#include<hgl/graph/LookAtCameraControl.h>
#include<hgl/graph/FirstPersonCameraControl.h>

#include<hgl/io/event/KeyboardEvent.h>
#include<hgl/io/event/MouseEvent.h>

using namespace hgl;
using namespace hgl::io;
using namespace hgl::graph;

namespace hgl{namespace graph
{
    bool InitShaderCompiler();
    void CloseShaderCompiler();
}}//namespace hgl::graph

class VulkanApplicationFramework:WindowEvent
{
protected:

            Window *    win             =nullptr;
    VulkanInstance *    inst            =nullptr;

protected:

    GPUDevice *             device                      =nullptr;
    SwapchainRenderTarget *           sc_render_target            =nullptr;

protected:

            Color4f         clear_color;

protected:

    RenderResource *        db                          =nullptr;
    
protected:

    ViewportInfo            vp_info;
    DeviceBuffer *          ubo_vp_info                 =nullptr;

public:

    virtual void BindRenderResource(RenderResource *rr)
    {
        if(!rr)
            return;

        rr->static_descriptor.AddUBO(mtl::SBS_ViewportInfo.name,ubo_vp_info);
    }

    virtual ~VulkanApplicationFramework()
    {
        CloseShaderCompiler();

        win->Unjoin(this);

        SAFE_CLEAR(db);

        SAFE_CLEAR(device);
        SAFE_CLEAR(win);
        SAFE_CLEAR(inst);
    }

    virtual bool Init(uint w,uint h)
    {
        logger::InitLogger(OS_TEXT("VulkanTest"));

        if(!InitShaderCompiler())
            return(false);

        SetClearColor(COLOR::MozillaCharcoal);

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

            inst=CreateInstance("VulkanTest",nullptr,&cili);

            if(!inst)
                return(false);
        }

        {
            VulkanHardwareRequirement vh_req;

            vh_req.wideLines=VulkanHardwareRequirement::SupportLevel::Want;

            device=CreateRenderDevice(inst,win,&vh_req);

            if(!device)
                return(false);

            sc_render_target=device->GetSwapchainRT();
        }

        db=new RenderResource(device);

        win->Join(this);

        {
            vp_info.Set(w,h);

            ubo_vp_info=db->CreateUBO("Viewport",sizeof(ViewportInfo),&vp_info);
        }

        BindRenderResource(db);

        return(true);
    }

    virtual void Resize(uint w,uint h)
    {
        vp_info.Set(w,h);
        ubo_vp_info->Write(&vp_info);
    }

    ViewportInfo *GetViewportInfo()
    {
        return &vp_info;
    }

    DeviceBuffer *GetViewportInfoBuffer()
    {
        return ubo_vp_info;
    }

    void SetClearColor(const Color4f &cc)
    {
        clear_color=cc;
    }

    void SetClearColor(const enum class COLOR &ce)
    {
        clear_color=GetColor4f(ce,1.0f);
    }

    void OnResize(uint w,uint h) override
    {
        if(w>0&&h>0)
            device->Resize(w,h);

        sc_render_target=device->GetSwapchainRT();

        Resize(w,h);
    }

    bool BuildCommandBuffer(RenderCmdBuffer *cb,Framebuffer *fb,Renderable *ri)
    {   
        if(!ri)return(false);

        cb->Begin();
            cb->BindFramebuffer(fb);
            cb->SetClearColor(0,clear_color);
            cb->BeginRenderPass();
                cb->BindPipeline(ri->GetPipeline());
                cb->BindDescriptorSets(ri->GetMaterial());
                cb->BindDataBuffer(ri->GetDataBuffer());
                cb->Draw(ri->GetDataBuffer(),ri->GetRenderData());
            cb->EndRenderPass();
        cb->End();

        return(true);
    }

    bool BuildCommandBuffer(RenderCmdBuffer *cb,RenderTarget *rt,Renderable *ri)
    {
        if(!cb||!rt||!ri)
            return(false);

        return BuildCommandBuffer(cb,rt->GetFramebuffer(),ri);
    }

    bool BuildCommandBuffer(uint32_t index,Renderable *ri)
    {   
        if(!ri)return(false);

        IRenderTarget *rt=sc_render_target->GetCurrentFrameRenderTarget();

        return BuildCommandBuffer(rt->GetRenderCmdBuffer(),
                                  rt->GetFramebuffer(),
                                  ri);
    }

    bool BuildCommandBuffer(Renderable *ri)
    {
        if(!ri)return(false);

        for(uint32_t i=0;i<sc_render_target->GetFrameCount();i++)
            BuildCommandBuffer(i,ri);

        return(true);
    }

    bool BuildCurrentCommandBuffer(Renderable *ri)
    {
        if(!ri)return(false);
    
        return BuildCommandBuffer(sc_render_target->GetCurrentFrameIndices(),ri);
    }

    void BuildCommandBuffer(uint32_t index,RenderList *rl)
    {
        if(!rl)return;

        RenderCmdBuffer *cb=sc_render_target->GetRenderCmdBuffer(index);

        cb->Begin();
        cb->BindFramebuffer(sc_render_target->GetFramebuffer(index));        
        cb->SetClearColor(0,clear_color);
            cb->BeginRenderPass();
                rl->Render(cb);
            cb->EndRenderPass();
        cb->End();
    }

    void BuildCommandBuffer(RenderList *rl)
    {
        for(uint32_t i=0;i<sc_render_target->GetFrameCount();i++)
            BuildCommandBuffer(i,rl);
    }

    void BuildCurrentCommandBuffer(RenderList *rl)
    {
        BuildCommandBuffer(sc_render_target->GetCurrentFrameIndices(),rl);
    }

    template<typename ...ARGS>
    Pipeline *CreatePipeline(ARGS...args)
    {
        Pipeline *p=sc_render_target->GetRenderPass()->CreatePipeline(args...);

        if(!p)
            return(nullptr);

    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();
        
        if(du)
            du->SetPipeline(*p,"Pipeline:"+p->GetName());
    #endif//_DEBUG

        return p;
    }

public:

    int AcquireNextImage()
    {
        return sc_render_target->AcquireNextImage();
    }

    virtual void SubmitDraw()
    {
        sc_render_target->Submit();
        sc_render_target->PresentBackbuffer();
        sc_render_target->WaitQueue();
        sc_render_target->WaitFence();
    }

    virtual void Draw()
    {
        int index=AcquireNextImage();

        if(index<0||index>=sc_render_target->GetFrameCount())return;

        SubmitDraw();
    }

    bool Run()
    {
        if(!win->Update())return(false);

        if(win->IsVisible())
            Draw();

        device->WaitIdle();

        return(true);
    }
};//class VulkanApplicationFramework

class CameraKeyboardControl:public KeyboardStateEvent
{
    FirstPersonCameraControl *camera;
    float move_speed;

public:

    CameraKeyboardControl(FirstPersonCameraControl *wc)
    {
        camera=wc;
        move_speed=1.0f;
    }

    bool OnPressed(const KeyboardButton &kb)override
    {
        if(!KeyboardStateEvent::OnPressed(kb))
            return(false);

        if(kb==KeyboardButton::Minus    )move_speed*=0.9f;else
        if(kb==KeyboardButton::Equals   )move_speed*=1.1f;

        return(true);
    }

    void Update()
    {
        if(HasPressed(KeyboardButton::W     ))camera->Forward   (move_speed);else
        if(HasPressed(KeyboardButton::S     ))camera->Backward  (move_speed);else
        if(HasPressed(KeyboardButton::A     ))camera->Left      (move_speed);else
        if(HasPressed(KeyboardButton::D     ))camera->Right     (move_speed);else
        //if(HasPressed(KeyboardButton::R     ))camera->Up        (move_speed);else
        //if(HasPressed(KeyboardButton::F     ))camera->Down      (move_speed);else

        //if(HasPressed(KeyboardButton::Left  ))camera->HoriRotate( move_speed);else
        //if(HasPressed(KeyboardButton::Right ))camera->HoriRotate(-move_speed);else
        //if(HasPressed(KeyboardButton::Up    ))camera->VertRotate( move_speed);else
        //if(HasPressed(KeyboardButton::Down  ))camera->VertRotate(-move_speed);else
            return;
    }
};

class CameraMouseControl:public MouseEvent
{
    FirstPersonCameraControl *camera;
    double cur_time;
    double last_time;

    Vector2f mouse_pos;
    Vector2f mouse_last_pos;

protected:

    bool OnPressed(int x,int y,MouseButton) override
    {
        mouse_last_pos.x=x;
        mouse_last_pos.y=y;

        last_time=cur_time;

        return(true);
    }
    
    bool OnWheel(int,int y) override
    {
        if(y==0)return(false);

        camera->Forward(float(y)/10.0f);

        return(true);
    }

    bool OnMove(int x,int y) override
    {
        mouse_pos.x=x;
        mouse_pos.y=y;

        bool left=HasPressed(MouseButton::Left);
        bool right=HasPressed(MouseButton::Right);
        
        Vector2f pos(x,y);
        Vector2f gap=pos-mouse_last_pos;
        
        if(left)
        {
            gap/=-5.0f;

            camera->Rotate(gap);
        }
        else
        if(right)
        {
            gap/=10.0f;

            camera->Move(Vector3f(gap.x,0,gap.y));
        }

        last_time=cur_time;
        mouse_last_pos=Vector2f(x,y);

        return(true);
    }

public:

    CameraMouseControl(FirstPersonCameraControl *wc)
    {
        camera=wc;
        cur_time=0;
        last_time=0;
    }

    const Vector2f &GetMouseCoord()const{return mouse_pos;}

    void Update()
    {
        cur_time=GetDoubleTime();
    }
};

class CameraAppFramework:public VulkanApplicationFramework
{
protected:

    Camera *                camera                      =nullptr;

    DeviceBuffer *          ubo_camera_info             =nullptr;

    FirstPersonCameraControl *camera_control=nullptr;

    CameraKeyboardControl * ckc=nullptr;
    CameraMouseControl *    cmc=nullptr;

    const Vector2f &GetMouseCoord()const{return cmc->GetMouseCoord();}

public:

    virtual void BindRenderResource(RenderResource *rr) override
    {
        if(!rr)return;
        
        VulkanApplicationFramework::BindRenderResource(rr);

        rr->global_descriptor.AddUBO(mtl::SBS_CameraInfo.name,ubo_camera_info);
    }

    virtual ~CameraAppFramework()
    {
        SAFE_CLEAR(ckc);
        SAFE_CLEAR(cmc);
        SAFE_CLEAR(camera);
    }

    virtual bool Init(uint w,uint h) override
    {
        if(!VulkanApplicationFramework::Init(w,h))
            return(false);

        InitCamera(w,h);
        
        BindRenderResource(db);
        return(true);
    }

    virtual void InitCamera(int w,int h)
    {
        camera=new Camera;

        camera->pos=Vector3f(10,10,10);

        camera_control=new FirstPersonCameraControl(&vp_info,camera);

        camera_control->Refresh();      //更新矩阵计算

        ckc=new CameraKeyboardControl(camera_control);
        cmc=new CameraMouseControl(camera_control);

        win->Join(ckc);
        win->Join(cmc);

        RefreshCameraInfo(camera_control->GetCameraInfo(),&vp_info,camera);
        
        ubo_camera_info=db->CreateUBO("CameraInfo",sizeof(CameraInfo),camera_control->GetCameraInfo());
    }

    void Resize(uint w,uint h)override
    {
        vp_info.Set(w,h);

        camera_control->Refresh();

        ubo_camera_info->Write(camera_control->GetCameraInfo());
    }

    CameraInfo *GetCameraInfo()
    {
        return camera_control->GetCameraInfo();
    }

    DeviceBuffer *GetCameraInfoBuffer()
    {
        return ubo_camera_info;
    }

    virtual void BuildCommandBuffer(uint32_t index)=0;

    virtual void Draw()override
    {
        camera_control->Refresh();                                  //更新相机矩阵

        ubo_camera_info->Write(camera_control->GetCameraInfo());    //写入缓冲区

        const uint32_t index=AcquireNextImage();

        BuildCommandBuffer(index);

        SubmitDraw();

        ckc->Update();
        cmc->Update();
    }
};//class CameraAppFramework

class SceneAppFramework:public CameraAppFramework
{
protected:

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;

public:

    SceneAppFramework()=default;

    virtual ~SceneAppFramework()
    {
        SAFE_CLEAR(render_list);
    }

    virtual bool Init(uint width,uint height) override
    {
        if(!CameraAppFramework::Init(width,height))
            return(false);
            
        render_list=new RenderList(device);

        return(true);
    }

    virtual void BuildCommandBuffer(uint32 index) override
    {
        VulkanApplicationFramework::BuildCommandBuffer(index,render_list);
    }

    virtual void Resize(uint w,uint h) override
    {
        CameraAppFramework::Resize(w,h);
        
        VulkanApplicationFramework::BuildCommandBuffer(render_list);
    }
};//class SceneAppFramework:public CameraAppFramework

template<typename T> int RunApp(uint w,uint h)
{
    T app;

    if(!app.Init(w,h))
        return(-1);

    while(app.Run());

    return 0;
}