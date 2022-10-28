// second_triangle
// 该范例主要演示使用场景树系统绘制三角形

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

constexpr uint32_t VERTEX_COUNT=3;

constexpr float position_data[VERTEX_COUNT][2]=
{
    {SCREEN_WIDTH*0.5,   SCREEN_HEIGHT*0.25},
    {SCREEN_WIDTH*0.25,  SCREEN_HEIGHT*0.75},
    {SCREEN_WIDTH*0.75,  SCREEN_HEIGHT*0.75}
};

constexpr float color_data[VERTEX_COUNT][4]=
{   {1,0,0,1},
    {0,1,0,1},
    {0,0,1,1}
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
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/VertexColor2D"));

        if(!material_instance)
            return(false);

        BindCameraUBO(material_instance);
            
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

        render_root.CreateSubNode(render_obj);

        render_root.RefreshMatrix();

        render_list->Expend(GetCameraInfo(),&render_root);

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
