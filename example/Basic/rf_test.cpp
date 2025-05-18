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
    Mesh *              render_obj          =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitAutoMaterial()
    {
        mtl::Material2DCreateConfig cfg(PrimitiveType::Triangles,
                                        CoordinateSystem2D::NDC,
                                        mtl::WithLocalToWorld::Without);

        material_instance=CreateMaterialInstance(mtl::inline_material::VertexColor2D,&cfg);    //这个是使用名称创建

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
        render_obj=CreateMesh("Triangle",VERTEX_COUNT,material_instance,pipeline,
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
