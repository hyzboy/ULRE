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

    VulkanDevice *          device                      =nullptr;
    RenderPass *            device_render_pass          =nullptr;
    SwapchainRenderTarget * sc_render_target            =nullptr;

protected:

            int32_t         swap_chain_count            =0;

    RenderCmdBuffer **      cmd_buf                     =nullptr;

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

        win->UnregisterEventDispatch(this);

        SAFE_CLEAR(db);
        SAFE_CLEAR_OBJECT_ARRAY_OBJECT(cmd_buf,swap_chain_count);

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
        }

        device_render_pass=device->GetRenderPass();

        db=new RenderResource(device);

        InitCommandBuffer();

        win->RegisterEventDispatch(this);

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

        InitCommandBuffer();
        Resize(w,h);
    }

    void InitCommandBuffer()
    {
        if(cmd_buf)
            SAFE_CLEAR_OBJECT_ARRAY_OBJECT(cmd_buf,swap_chain_count);

        sc_render_target=device->GetSwapchainRT();

        swap_chain_count=sc_render_target->GetImageCount();
        {
            const VkExtent2D extent=sc_render_target->GetExtent();

            cmd_buf=hgl_zero_new<RenderCmdBuffer *>(swap_chain_count);

            for(int32_t i=0;i<swap_chain_count;i++)
                cmd_buf[i]=device->CreateRenderCommandBuffer(device->GetPhyDevice()->GetDeviceName()+AnsiString(":RenderCmdBuffer_")+AnsiString::numberOf(i));
        }
    }

    bool BuildCommandBuffer(RenderCmdBuffer *cb,Framebuffer *fbo,Mesh *ri)
    {   
        if(!ri)return(false);

        cb->Begin();
            cb->BindFramebuffer(fbo);
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

    bool BuildCommandBuffer(uint32_t index,Mesh *ri)
    {   
        if(!ri)return(false);

        return BuildCommandBuffer(cmd_buf[index],
                                  sc_render_target->GetFramebuffer(),ri);
    }

    bool BuildCommandBuffer(Mesh *ri)
    {
        if(!ri)return(false);

        for(int32_t i=0;i<swap_chain_count;i++)
            BuildCommandBuffer(i,ri);

        return(true);
    }

    bool BuildCurrentCommandBuffer(Mesh *ri)
    {
        if(!ri)return(false);
    
        return BuildCommandBuffer(sc_render_target->GetCurrentFrameIndices(),ri);
    }

    void BuildCommandBuffer(uint32_t index,RenderList *rl)
    {
        if(!rl)return;

        RenderCmdBuffer *cb=cmd_buf[index];

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
        for(int32_t i=0;i<swap_chain_count;i++)
            BuildCommandBuffer(i,rl);
    }

    void BuildCurrentCommandBuffer(RenderList *rl)
    {
        BuildCommandBuffer(sc_render_target->GetCurrentFrameIndices(),rl);
    }

    template<typename ...ARGS>
    Pipeline *CreatePipeline(ARGS...args)
    {
        Pipeline *p=device_render_pass->CreatePipeline(args...);

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

        device->WaitIdle();

        return(true);
    }
};//class VulkanApplicationFramework

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

        win->RegisterEventDispatch(ckc);
        win->RegisterEventDispatch(cmc);

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

        SubmitDraw(index);

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
