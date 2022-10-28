// Instance Triangle

// 基本的Instance绘制测试用例,不使用场景树

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/SceneInfo.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_SIZE=512;

constexpr uint32_t VERTEX_COUNT=3;

constexpr float position_data[VERTEX_COUNT][2]=
{
    {0,0},
    {0.25,-0.75},
    {0,-1}
};

constexpr float color_data[VERTEX_COUNT][4]=
{   {1,0,0,1},
    {0,1,0,1},
    {0,0,1,1}
};

class TestApp:public VulkanApplicationFramework
{
private:

    MaterialInstance *  material_instance   =nullptr;
    Renderable *        render_obj          =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/VertexColor2DNDC"));

        if(!material_instance)
            return(false);
            
//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Triangles);     //等同上一行，为Framework重载，默认使用swapchain的render target

        return pipeline;
    }
   
    bool InitVBO()
    {
        Primitive *primitive=db->CreatePrimitive(VERTEX_COUNT);
        if(!primitive)return(false);

        if(!primitive->Set(VAN::Position,  db->CreateVBO(VF_V2F,VERTEX_COUNT,position_data  )))return(false);
        if(!primitive->Set(VAN::Color, db->CreateVBO(VF_V4F,VERTEX_COUNT,color_data     )))return(false);
        
        render_obj=db->CreateRenderable(primitive,material_instance,pipeline);
        return(true);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_SIZE,SCREEN_SIZE))
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
