// 该范例主要演示使用NDC坐标系直接绘制一个渐变色的三角形

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/math/HalfFloat.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/SceneInfo.h>
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

#define USE_HALF_FLOAT_POSITION

#ifdef USE_HALF_FLOAT_POSITION
constexpr VkFormat PositionFormat=VF_V2HF;

half_float position_data_hf[VERTEX_COUNT*2];

#define position_data   position_data_hf
#else
constexpr VkFormat PositionFormat=VF_V2F;

#define position_data   position_data_float
#endif//USE_HALF_FLOAT_POSITION

#define USE_UNORM8_COLOR

#ifdef USE_UNORM8_COLOR
constexpr uint8 color_data[VERTEX_COUNT*4]=
{   255,0,0,255,
    0,255,0,255,
    0,0,255,255
};

constexpr VkFormat ColorFormat=VF_V4UN8;
#else
constexpr float color_data[VERTEX_COUNT*4]=
{   1,0,0,1,
    0,1,0,1,
    0,0,1,1
};

constexpr VkFormat ColorFormat=VF_V4F;
#endif//USE_UNORM8_COLOR

class TestApp:public VulkanApplicationFramework
{
private:

#if defined(USE_HALF_FLOAT_POSITION)||defined(USE_UNORM8_COLOR)
    VILConfig vil_config;
#endif

    MaterialInstance *  material_instance   =nullptr;
    Renderable *        render_obj          =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    void InitVIL()
    {
    #ifdef USE_HALF_FLOAT_POSITION
        vil_config.Add(VAN::Position,PositionFormat);
    #endif//USE_HALF_FLOAT_POSITION

    #ifdef USE_UNORM8_COLOR
        vil_config.Add(VAN::Color,ColorFormat);
    #endif//USE_HALF_FLOAT_POSITION
    }

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

#ifdef USE_HALF_FLOAT_POSITION
        Float32toFloat16(position_data_hf,position_data_float,VERTEX_COUNT*2);
#endif//USE_HALF_FLOAT_POSITION

        if(!rpc.WriteVAB(VAN::Position,   PositionFormat, position_data))return(false);
        if(!rpc.WriteVAB(VAN::Color,      ColorFormat,    color_data   ))return(false);
        
        render_obj=db->CreateRenderable(&rpc,material_instance,pipeline);
        return(render_obj);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        InitVIL();

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
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
