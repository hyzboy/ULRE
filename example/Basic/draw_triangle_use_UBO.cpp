// 该范例主要演示使用2D坐系统直接绘制一个渐变色的三角形,使用UBO传递Viewport信息

#include<hgl/WorkManager.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>

#include<hgl/component/MeshComponent.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t VERTEX_COUNT=3;

static float position_data_float[VERTEX_COUNT][2]=
{
    {0.5,   0.25},
    {0.75,  0.75},
    {0.25,  0.75}
};

static int16 position_data[VERTEX_COUNT][2]={};

constexpr uint8 color_data[VERTEX_COUNT*4]=
{   
    255,0,0,255,
    0,255,0,255,
    0,0,255,255
};

constexpr VAType   POSITION_SHADER_FORMAT   =VAT_IVEC2;
constexpr VkFormat POSITION_DATA_FORMAT     =VF_V2I16;

constexpr VkFormat COLOR_DATA_FORMAT        =VF_V4UN8;

class TestApp:public WorkObject
{
private:

    MaterialInstance *  material_instance   =nullptr;
    Mesh *              mesh_triangle       =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        mtl::Material2DCreateConfig cfg(PrimitiveType::Triangles,
                                        CoordinateSystem2D::Ortho,
                                        mtl::WithLocalToWorld::With);

        VILConfig vil_config;

        cfg.position_format     =       POSITION_SHADER_FORMAT;     //这里指定shader中使用ivec2当做顶点输入格式
                                //      ^
                                //      +  这上下两种格式要配套，否则会出错
                                //      v
        vil_config.Add(VAN::Position,   POSITION_DATA_FORMAT);     //这里指定VAB中使用RG16I当做顶点数据格式

        vil_config.Add(VAN::Color,      COLOR_DATA_FORMAT);        //这里指定VAB中使用RGBA8UNorm当做颜色数据格式

        material_instance=CreateMaterialInstance(mtl::inline_material::VertexColor2D,&cfg,&vil_config);

        if(!material_instance)
            return(false);
           
//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D);     //等同上一行，为Framework重载，默认使用swapchain的render target

        return pipeline;
    }
   
    bool InitVBO()
    {
        const auto ext=GetExtent();

        for(uint i=0;i<VERTEX_COUNT;i++)
        {
            position_data[i][0]=position_data_float[i][0]*ext.width;
            position_data[i][1]=position_data_float[i][1]*ext.height;
        }

        mesh_triangle=CreateMesh("Triangle",VERTEX_COUNT,material_instance,pipeline,
                                    {
                                        {VAN::Position,POSITION_DATA_FORMAT,position_data},
                                        {VAN::Color,   COLOR_DATA_FORMAT,   color_data}
                                    });

        if(!mesh_triangle)
            return(false);

        return CreateComponent<MeshComponent>(GetSceneRoot(),mesh_triangle); //创建一个静态网格组件
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Draw triangle use UBO"));
}
