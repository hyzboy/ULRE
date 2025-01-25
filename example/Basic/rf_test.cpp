// 该范例主要演示使用NDC坐标系直接绘制一个渐变色的三角形

#include"WorkObject.h"
#include<hgl/math/HalfFloat.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/VKMaterialInstance.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

constexpr uint32_t VERTEX_COUNT=3;

constexpr float position_data_float[VERTEX_COUNT*2]=
{
     0.0, -0.5,
    -0.5,  0.5,
     0.5,  0.5
};

constexpr VkFormat PositionFormat=VF_V2F;

#define position_data   position_data_float

constexpr float color_data[VERTEX_COUNT*4]=
{   1,0,0,1,
    0,1,0,1,
    0,0,1,1
};

constexpr VkFormat ColorFormat=VF_V4F;

class TestApp:public WorkObject
{
private:

    Color4f             clear_color         =Color4f(0.2f,0.2f,0.2f,1.0f);

    MaterialInstance *  material_instance   =nullptr;
    Renderable *        render_obj          =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitAutoMaterial()
    {
        mtl::Material2DCreateConfig cfg(GetDeviceAttribute(),"VertexColor2d",Prim::Triangles);

        cfg.coordinate_system=CoordinateSystem2D::NDC;
        cfg.local_to_world=false;

        AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateVertexColor2D(&cfg);

        material_instance=db->CreateMaterialInstance(mci);

        return material_instance;
    }

    bool InitPipeline()
    {
        RenderPass *sc_render_pass=GetRenderFramework()->GetSwapchainModule()->GetRenderPass();

//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=sc_render_pass->CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Triangles);     //等同上一行，为Framework重载，默认使用swapchain的render target

        return pipeline;
    }

    bool InitVBO()
    {
        PrimitiveCreater rpc(GetDevice(),material_instance->GetVIL());
        
        rpc.Init("Triangle",VERTEX_COUNT);

        if(!rpc.WriteVAB(VAN::Position,   PositionFormat, position_data))return(false);
        if(!rpc.WriteVAB(VAN::Color,      ColorFormat,    color_data   ))return(false);
        
        render_obj=db->CreateRenderable(&rpc,material_instance,pipeline);
        return(render_obj);
    }

public:

    TestApp(RenderFramework *rf):WorkObject(rf)
    {}

    bool Init()
    {
        if(!InitAutoMaterial())
            return(false);

        if(!InitPipeline())
            return(false);

        if(!InitVBO())
            return(false);

        return(true);
    }

    void Tick(double)override
    {}

    void Render(RenderCmdBuffer *cb,Renderable *ri)
    {
        if(!ri)return;
    
        cb->BeginRenderPass();
            cb->BindPipeline(ri->GetPipeline());
            cb->BindDescriptorSets(ri->GetMaterial());
            cb->BindDataBuffer(ri->GetDataBuffer());
            cb->Draw(ri->GetDataBuffer(),ri->GetRenderData());
        cb->EndRenderPass();
    }

    void Render(double)
    {
        //WorkObject是工作对象，不是渲染对象，所以不应该直接自动指定RenderTarget，更不能直接指定RenderCmdBuffer

        //目前这里只是为了测试，所以这样写

        RenderFramework *rf=GetRenderFramework();
        SwapchainModule *sm=rf->GetSwapchainModule();

        sm->BeginFrame();       //这里会有AcquireNextImage操作

            //这个使用完全不合理，录制CMD和推送swapchain是两回事，需要分开操作。
            //比如场景有的物件分静态和动态

            //可能静态物件就全部一次性录制好，而动态物件则是每帧录制

            RenderCmdBuffer *cb=sm->RecordCmdBuffer();  //这里会获取当前帧的RenderCmdBuffer、开启cmd录制、绑定FBO

            if(cb)
            {
                cb->SetClearColor(0,clear_color);
             
                Render(cb,render_obj);

                cb->End();      //结束cmd录制
            }
        sm->EndFrame();             //这里会Submit和PresentBackbuffer
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    RenderFramework rf(OS_TEXT("RenderFramework Test"));

    if(!rf.Init(SCREEN_WIDTH,SCREEN_HEIGHT))
        return(-1);

    WorkManager wm(&rf);

    AutoDelete<TestApp> app=new TestApp(&rf);

    if(!app->Init())
        return(-2);

    wm.Start(app);
}
