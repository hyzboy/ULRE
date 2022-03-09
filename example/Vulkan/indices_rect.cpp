// indices_rect
// 该示例演示使用索引数据画一个矩形，并使用了颜色材质

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=256;
constexpr uint32_t SCREEN_HEIGHT=256;

constexpr uint32_t VERTEX_COUNT=4;

static Vector4f color(1,1,1,1);

constexpr float position_data[VERTEX_COUNT][2]=
{
    {0,0},
    {SCREEN_WIDTH,0},
    {0,SCREEN_HEIGHT},
    {SCREEN_WIDTH,SCREEN_HEIGHT}
};
 
constexpr uint32_t INDEX_COUNT=6;

constexpr uint16 index_data[INDEX_COUNT]=
{
    0,1,3,
    0,3,2
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
        if(!material_instance)return(false);

        BindCameraUBO(material_instance);
        
        //        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Triangles);     //等同上一行，为Framework重载，默认使用swapchain的render target
        
        return pipeline;
    }

    bool InitVBO()
    {        
        auto render_obj=db->CreateRenderable(VERTEX_COUNT);
        if(!render_obj)return(false);

        if(!render_obj->Set(VAN::Position,db->CreateVBO(VF_V2F,VERTEX_COUNT,position_data)))return(false);
        if(!render_obj->Set(db->CreateIBO16(INDEX_COUNT,index_data)))return(false);

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
    #ifdef _DEBUG
    if(!CheckStrideBytesByFormat())
        return 0xff;
    #endif//

    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
