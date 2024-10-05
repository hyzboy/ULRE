// 该范例主要演示使用2D坐系统直接绘制一个渐变色的三角形,使用UBO传递Viewport信息

#include"VulkanAppFramework.h"
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

static uint16 position_data_u16[VERTEX_COUNT][2]={};

constexpr uint8 color_data[VERTEX_COUNT*4]=
{   
    255,0,0,255,
    0,255,0,255,
    0,0,255,255
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

        VILConfig vil_config;

#ifdef USE_ZERO2ONE_COORD
        cfg.coordinate_system=CoordinateSystem2D::ZeroToOne;
#else
        cfg.coordinate_system=CoordinateSystem2D::Ortho;

        cfg.position_format         =VAT_UVEC2;     //这里指定shader中使用uvec2当做顶点输入格式
                                //      ^
                                //      +  这上下两种格式要配套，否则会出错
                                //      v
        vil_config.Add(VAN::Position,VF_V2U16);     //这里指定VAB中使用RG16U当做顶点数据格式
#endif//USE_ZERO2ONE_COORD

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
        PrimitiveCreater rpc(device,material_instance->GetVIL());

        rpc.Init("Triangle",VERTEX_COUNT);

#ifdef USE_ZERO2ONE_COORD               //使用0 to 1坐标系
        if(!rpc.WriteVAB(VAN::Position,   VF_V2F,     position_data_float ))return(false);
#else                                   //使用ortho坐标系
        if(!rpc.WriteVAB(VAN::Position,   VF_V2U16,   position_data_u16   ))return(false);
#endif//USE_ZERO2ONE_COORD

        if(!rpc.WriteVAB(VAN::Color,      VF_V4UN8,   color_data          ))return(false);
        
        render_obj=db->CreateRenderable(&rpc,material_instance,pipeline);
        return(true);
    }

public:

    bool Init(uint w,uint h)
    {
        if(!VulkanApplicationFramework::Init(w,h))
            return(false);

    #ifndef USE_ZERO2ONE_COORD
        for(uint i=0;i<VERTEX_COUNT;i++)
        {
            position_data_u16[i][0]=position_data_float[i][0]*w;
            position_data_u16[i][1]=position_data_float[i][1]*h;
        }
    #endif//

        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);

        if(!BuildCommandBuffer(render_obj))
            return(false);

        return(true);
    }

    void Resize(uint w,uint h)override
    {
        VulkanApplicationFramework::Resize(w,h);

        BuildCommandBuffer(render_obj);
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    return RunApp<TestApp>(1280,720);
}
