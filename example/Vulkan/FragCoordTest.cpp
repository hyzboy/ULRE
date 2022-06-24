// 该范例主要用于测试gl_FragCoord值

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=256;
constexpr uint32_t SCREEN_HEIGHT=256;

constexpr uint32_t VERTEX_COUNT=4;

constexpr float position_data[VERTEX_COUNT][2]=
{
    {0,0},
    {SCREEN_WIDTH,0},
    {0,SCREEN_HEIGHT},
    {SCREEN_WIDTH,SCREEN_HEIGHT}
};

class TestApp:public VulkanApplicationFramework
{
private:

    MaterialInstance *  material_instance   =nullptr;
    RenderableInstance *renderable_instance =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/FragColor"));

        if(!material_instance)
            return(false);

        BindCameraUBO(material_instance);
        
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::TriangleStrip);

        return pipeline;
    }

    bool InitVBO()
    {
        auto render_obj=db->CreatePrimitive(VERTEX_COUNT);
        if(!render_obj)return(false);

        if(!render_obj->Set(VAN::Position,db->CreateVBO(VF_V2F,VERTEX_COUNT,position_data)))return(false);

        renderable_instance=db->CreateRenderableInstance(render_obj,material_instance,pipeline);
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

        BuildCommandBuffer(renderable_instance);

        return(true);
    }

    void Resize(int w,int h)override
    {
        VulkanApplicationFramework::Resize(w,h);

        BuildCommandBuffer(renderable_instance);
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
