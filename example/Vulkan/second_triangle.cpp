// second_triangle
// 该范例主要演示使用2D坐系统直接绘制一个渐变色的三角形,使用UBO传递Viewport信息

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/SceneInfo.h>
#include<hgl/graph/VKRenderablePrimitiveCreater.h>
#include<hgl/graph/mtl/2d/Material2DCreateConfig.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

constexpr uint32_t VERTEX_COUNT=3;

static float position_data[VERTEX_COUNT][2]=
{
    {0.5,   0.25},
    {0.75,  0.75},
    {0.25,  0.75}
};

constexpr float color_data[VERTEX_COUNT][4]=
{   
    {1,0,0,1},
    {0,1,0,1},
    {0,0,1,1}
};

//#define USE_ZERO2ONE_COORD      //使用左上角0,0右下角1,1的坐标系

class TestApp:public VulkanApplicationFramework
{
private:

    MaterialInstance *  material_instance   =nullptr;
    Renderable *        render_obj          =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        mtl::Material2DCreateConfig cfg(device->GetDeviceAttribute(),"VertexColor2D",Prim::Triangles);

#ifdef USE_ZERO2ONE_COORD
        cfg.coordinate_system=CoordinateSystem2D::ZeroToOne;
#else
        cfg.coordinate_system=CoordinateSystem2D::Ortho;
#endif//USE_ZERO2ONE_COORD

        cfg.local_to_world=false;

        AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateVertexColor2D(&cfg);

        material_instance=db->CreateMaterialInstance(mci);

        if(!material_instance)
            return(false);

        db->global_descriptor.Bind(material_instance->GetMaterial());
            
//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Triangles);     //等同上一行，为Framework重载，默认使用swapchain的render target

        return pipeline;
    }
   
    bool InitVBO()
    {
        RenderablePrimitiveCreater rpc(db,VERTEX_COUNT);

#ifndef USE_ZERO2ONE_COORD      //使用ortho坐标系

        for(uint i=0;i<VERTEX_COUNT;i++)
        {
            position_data[i][0]*=SCREEN_WIDTH;
            position_data[i][1]*=SCREEN_HEIGHT;
        }

#endif//USE_ZERO2ONE_COORD

        if(!rpc.SetVBO(VAN::Position,   VF_V2F, position_data))return(false);
        if(!rpc.SetVBO(VAN::Color,      VF_V4F, color_data   ))return(false);
        
        render_obj=rpc.Create(material_instance,pipeline);
        return(true);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);

        if(!BuildCommandBuffer(render_obj))
            return(false);

        return(true);
    }

    void Resize(int w,int h)override
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
