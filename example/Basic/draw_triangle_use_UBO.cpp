// 该范例主要演示使用2D坐系统直接绘制一个渐变色的三角形,使用UBO传递Viewport信息

#include<hgl/WorkManager.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t VERTEX_COUNT=3;

static float position_data_float[VERTEX_COUNT][2]=
{
    {0.5,   0.25},
    {0.75,  0.75},
    {0.25,  0.75}
};

static uint16 position_data[VERTEX_COUNT][2]={};

constexpr uint8 color_data[VERTEX_COUNT*4]=
{   
    255,0,0,255,
    0,255,0,255,
    0,0,255,255
};

class TestApp:public WorkObject
{
private:

    Color4f             clear_color         =Color4f(0.2f,0.2f,0.2f,1.0f);

    MaterialInstance *  material_instance   =nullptr;
    Renderable *        render_obj          =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        mtl::Material2DCreateConfig cfg(GetDeviceAttribute(),"VertexColor2D",Prim::Triangles);

        VILConfig vil_config;

        cfg.coordinate_system=CoordinateSystem2D::Ortho;

        cfg.position_format         =VAT_UVEC2;     //这里指定shader中使用uvec2当做顶点输入格式
                                //      ^
                                //      +  这上下两种格式要配套，否则会出错
                                //      v
        vil_config.Add(VAN::Position,VF_V2U16);     //这里指定VAB中使用RG16U当做顶点数据格式
        
        const auto ext=GetExtent2D();

        for(uint i=0;i<VERTEX_COUNT;i++)
        {
            position_data[i][0]=position_data_float[i][0]*ext.width;
            position_data[i][1]=position_data_float[i][1]*ext.height;
        }

        vil_config.Add(VAN::Color,VF_V4UN8);        //这里指定VAB中使用RGBA8UNorm当做颜色数据格式

        cfg.local_to_world=false;

        AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateVertexColor2D(&cfg);

        material_instance=db->CreateMaterialInstance(mci,&vil_config);

        if(!material_instance)
            return(false);
           
//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Triangles);     //等同上一行，为Framework重载，默认使用swapchain的render target

        return pipeline;
    }
   
    bool InitVBO()
    {
        render_obj=CreateRenderable("Triangle",VERTEX_COUNT,material_instance,pipeline,
                                    {
                                        {VAN::Position,VF_V2U16,position_data},
                                        {VAN::Color,   VF_V4UN8,color_data}
                                    });
        return(render_obj);
    }

public:
    
    TestApp(RenderFramework *rf):WorkObject(rf,rf->GetSwapchainRenderTarget())
    {
        if(!InitMaterial())
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
    return RunFramework<TestApp>(OS_TEXT("Draw triangle use UBO"));
}
