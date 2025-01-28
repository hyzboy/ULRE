// 该范例主要演示使用NDC坐标系直接绘制一个渐变色的三角形
#include<hgl/WorkManager.h>
#include<hgl/math/HalfFloat.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/VKMaterialInstance.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

constexpr uint32_t VERTEX_COUNT=3;

constexpr float position_data[VERTEX_COUNT*2]=
{
     0.0, -0.5,
    -0.5,  0.5,
     0.5,  0.5
};

constexpr float color_data[VERTEX_COUNT*4]=
{   1,0,0,1,
    0,1,0,1,
    0,0,1,1
};

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
        mtl::Material2DCreateConfig cfg(GetDeviceAttribute(),"VertexColor2D",Prim::Triangles);

        cfg.coordinate_system=CoordinateSystem2D::NDC;
        cfg.local_to_world=false;

        AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateVertexColor2D(&cfg);

        material_instance=db->CreateMaterialInstance(mci);

        return material_instance;
    }

    bool InitPipeline()
    {
//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Triangles);     //等同上一行，为Framework重载，默认使用swapchain的render target

        return pipeline;
    }

    bool InitVBO()
    {
        PrimitiveCreater rpc(GetDevice(),material_instance->GetVIL());
        
        rpc.Init("Triangle",VERTEX_COUNT);

        if(!rpc.WriteVAB(VAN::Position,   VF_V2F,   position_data))return(false);
        if(!rpc.WriteVAB(VAN::Color,      VF_V4F,   color_data   ))return(false);
        
        render_obj=db->CreateRenderable(&rpc,material_instance,pipeline);
        return(render_obj);
    }

public:

    TestApp(RenderFramework *rf):WorkObject()
    {
        Join(rf,rf->GetSwapchainRenderTarget());
    }

    void Join(RenderFramework *rf,IRenderTarget *rt)
    {
        WorkObject::Join(rf,rt);

        if(!InitAutoMaterial())
            return;

        if(!InitPipeline())
            return;

        if(!InitVBO())
            return;
    }

    void Tick(double)override
    {}

    void Render(double delta_time,graph::RenderCmdBuffer *cmd)
    {
        if(!cmd)
            return;

        cmd->SetClearColor(0,clear_color);

        cmd->BeginRenderPass();
            cmd->Render(render_obj);
        cmd->EndRenderPass();
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    RenderFramework rf(OS_TEXT("RenderFramework Test"));

    if(!rf.Init(SCREEN_WIDTH,SCREEN_HEIGHT))
        return(-1);

    // RenderFramework存在于外部，提供的是整体的渲染控制。

    // WorkManager是提供一个工作业务管理，但开发者并不一定要使用它，所以我们不将它们整合在一起。

    SwapchainWorkManager wm(&rf);

    wm.Run(new TestApp(&rf));
}
