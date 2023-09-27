// 该范例主要演示使用RenderList系统绘制多个三角形，并利用RenderList进行排序以及自动合并进行Instance渲染

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/VKRenderablePrimitiveCreater.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/mtl/2d/Material2DCreateConfig.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1024;
constexpr uint32_t SCREEN_HEIGHT=1024;

constexpr uint32_t VERTEX_COUNT=3;

constexpr float position_data[VERTEX_COUNT*2]=
{
     0.0,  0.0,
    -0.1,  0.9,
     0.1,  0.9
};

constexpr uint8 color_data[VERTEX_COUNT][4]=
{   {255,0,0,255},
    {0,255,0,255},
    {0,0,255,255}
};

class TestApp:public VulkanApplicationFramework
{
private:

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;

    MaterialInstance *  material_instance   =nullptr;
    Renderable *        render_obj          =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        {
            mtl::Material2DCreateConfig cfg(device->GetDeviceAttribute(),"VertexColor2D",Prim::Triangles);

            cfg.coordinate_system=CoordinateSystem2D::NDC;
            cfg.local_to_world=true;

            AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateVertexColor2D(&cfg);

            VILConfig vil_config;

            vil_config.Add(VAN::Color,VF_V4UN8);

            material_instance=db->CreateMaterialInstance(mci,&vil_config);
        }

        if(!material_instance)
            return(false);

//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Triangles);     //等同上一行，为Framework重载，默认使用swapchain的render target

        return pipeline;
    }

    bool InitVBO()
    {
        RenderablePrimitiveCreater rpc(db,VERTEX_COUNT);

        if(!rpc.SetVBO(VAN::Position,   VF_V2F,     position_data))return(false);
        if(!rpc.SetVBO(VAN::Color,      VF_V4UN8,   color_data   ))return(false);
        
        render_obj=rpc.Create(material_instance,pipeline);

        if(!render_obj)
            return(false);
        
        for(uint i=0;i<12;i++)
            render_root.CreateSubNode(rotate(deg2rad(30*i),Vector3f(0,0,1)),render_obj);

        render_root.RefreshMatrix();

        render_list->Expend(&render_root);

        return(true);
    }

public:

    ~TestApp()
    {
        SAFE_CLEAR(render_list);
    }

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        render_list=new RenderList(device);

        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);

        BuildCommandBuffer(render_list);

        return(true);
    }

    void Resize(int w,int h)override
    {
        VulkanApplicationFramework::Resize(w,h);

        BuildCommandBuffer(render_list);
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
