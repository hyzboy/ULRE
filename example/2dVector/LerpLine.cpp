// 主要用于测试几种2D插值算法

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

constexpr float position_data[VERTEX_COUNT*2]=
{
    -1.0,  0.0,
     1.0,  0.0,
};

constexpr VkFormat PositionFormat=VF_V2F;

constexpr float color_data[VERTEX_COUNT*4]=
{   1,0,0,1,
    0,1,0,1,
    0,0,1,1
};

constexpr VkFormat ColorFormat=VF_V4F;

class TestApp:public VulkanApplicationFramework
{
private:

    MaterialInstance *  material_instance   =nullptr;
    Renderable *        render_obj          =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitAutoMaterial()
    {
        mtl::Material2DCreateConfig cfg(device->GetDeviceAttribute(),"VertexColor2D",Prim::Lines);

        cfg.coordinate_system=CoordinateSystem2D::NDC;
        cfg.local_to_world=false;

        AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateVertexColor2D(&cfg);

        material_instance=db->CreateMaterialInstance(mci);

        return material_instance;
    }

    bool InitPipeline()
    {
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Lines);

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

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
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
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
