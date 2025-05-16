#include<hgl/WorkManager.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/mtl/MaterialLibrary.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

constexpr uint32_t VERTEX_COUNT=3;

constexpr float position_data[VERTEX_COUNT*2]=
{
     0.0, -0.5,
    -0.5,  0.5,
     0.5,  0.5
};

constexpr float color_data[VERTEX_COUNT*4]=
{
    1,0,0,1,
    0,1,0,1,
    0,0,1,1
};

class TestApp:public WorkObject
{
private:

    Color4f             clear_color         =Color4f(0.2f,0.2f,0.2f,1.0f);

    MaterialInstance *  material_instance   =nullptr;
    Renderable *        render_obj          =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitAutoMaterial()
    {
        mtl::Material2DCreateConfig cfg("VertexColor2D",PrimitiveType::Triangles);

        cfg.coordinate_system=CoordinateSystem2D::NDC;
        cfg.local_to_world=false;

        //AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateVertexColor2D(&cfg);                       //这个是直接创建

        //AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateMaterialCreateInfo("VertexColor2D",&cfg);    //这个是使用名称创建
        //这两种方式都可以，上一种方式肯定是会快些，主要用于一些程序中直接写死的地方。
        //而下面这种方式很明显是为了可以将使用的材质写入配置文件中。

        //material_instance=CreateMaterialInstance(mci);

        //下面这个方式更直接，在WorkObject中封装了CreateMaterialCreateInfo(name,...)这个方法一步到位
        material_instance=CreateMaterialInstance("VertexColor2D",&cfg);    //这个是使用名称创建

        //再下一步我们会更直接将上面的Material2DCreateConfig写在配置文件中，这样只需要一个配置文件名称就可以完全加载了。

        return material_instance;
    }

    bool InitPipeline()
    {
//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,PrimitiveType::Triangles);     //等同上一行，为Framework重载，默认使用swapchain的render target

        return pipeline;
    }

    bool InitVBO()
    {
        render_obj=CreateRenderable("Triangle",VERTEX_COUNT,material_instance,pipeline,
                                    {
                                        {VAN::Position,VF_V2F,position_data},
                                        {VAN::Color,   VF_V4F,color_data}
                                    });
        return(render_obj);
    }

public:

    TestApp(RenderFramework *rf):WorkObject(rf,rf->GetSwapchainRenderTarget())
    {
        if(!InitAutoMaterial())
            return;

        if(!InitPipeline())
            return;

        if(!InitVBO())
            return;
    }

    void Tick(double)override{}

    void Render(double delta_time,graph::RenderCmdBuffer *cmd)
    {
        cmd->SetClearColor(0,clear_color);

        cmd->BeginRenderPass();
            cmd->Render(render_obj);
        cmd->EndRenderPass();
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("RenderFramework Test"));
}
