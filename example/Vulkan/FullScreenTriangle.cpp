// 全屏三角形
// 该范例用于演示使用索引画一个覆盖全屏的三角形，但是不传递任何顶点信息，顶点坐标在vertex shader中通过gl_VertexIndex计算出来。

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=256;
constexpr uint32_t SCREEN_HEIGHT=256;

class TestApp:public VulkanApplicationFramework
{
private:

    MaterialInstance *  material_instance   =nullptr;
    RenderableInstance *renderable_instance =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/fullscreen"));
        if(!material_instance)return(false);

        BindCameraUBO(material_instance);
        
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Triangles);
        
        return pipeline;
    }

    bool InitVBO()
    {
        auto render_obj=db->CreatePrimitive(3);
        if(!render_obj)return(false);

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
