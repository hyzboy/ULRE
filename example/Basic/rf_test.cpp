// 该范例主要演示使用NDC坐标系直接绘制一个渐变色的三角形

#include"WorkObject.h"
#include<hgl/math/HalfFloat.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>

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

    MaterialInstance *  material_instance   =nullptr;
    Renderable *        render_obj          =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitAutoMaterial()
    {
        mtl::Material2DCreateConfig cfg(device->GetDeviceAttribute(),"VertexColor2d",Prim::Triangles);

        cfg.coordinate_system=CoordinateSystem2D::NDC;
        cfg.local_to_world=false;

        AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateVertexColor2D(&cfg);

        material_instance=db->CreateMaterialInstance(mci,&vil_config);

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
        PrimitiveCreater rpc(device,material_instance->GetVIL());
        
        rpc.Init("Triangle",VERTEX_COUNT);

        if(!rpc.WriteVAB(VAN::Position,   PositionFormat, position_data))return(false);
        if(!rpc.WriteVAB(VAN::Color,      ColorFormat,    color_data   ))return(false);
        
        render_obj=db->CreateRenderable(&rpc,material_instance,pipeline);
        return(render_obj);
    }

public:

    bool Init(uint w,uint h)
    {
        if(!VulkanApplicationFramework::Init(w,h))
            return(false);

        if(!InitAutoMaterial())
            return(false);

        if(!InitPipeline())
            return(false);

        if(!InitVBO())
            return(false);

        if(!BuildCommandBuffer(render_obj))
            return(false);

        return(true);
    }

    void Resize(uint w,uint h)override
    {
        VulkanApplicationFramework::Resize(w,h);

        BuildCommandBuffer(render_obj);
    }

    void Tick(double)override
    {}

    void Render(double)
    {}
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    RenderFramework rf(OS_TEXT("RenderFramework Test"));

    if(rf.Init(SCREEN_WIDTH,SCREEN_HEIGHT))
        return(-1);

    WorkManager wm(&rf);

    wm.Start(new TestApp);
}
