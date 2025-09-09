// 该范例主要演示使用RenderCollector系统绘制多个三角形，并利用RenderCollector进行排序以及自动合并进行Instance渲染

#include<hgl/WorkManager.h>
#include<hgl/math/Math.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/component/MeshComponent.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t VERTEX_COUNT=3;

constexpr uint32_t TRIANGLE_NUMBER=12;

constexpr float position_data[VERTEX_COUNT*2]=
{
     0.0,  0.0,
    -0.1,  0.9,
     0.1,  0.9
};

constexpr uint8 color_data[VERTEX_COUNT][4]=
{
    {255,0,0,255},
    {0,255,0,255},
    {0,0,255,255}
};

class TestApp:public WorkObject
{
private:

    MaterialInstance *  material_instance   =nullptr;
    Mesh *              render_obj          =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        {
            mtl::Material2DCreateConfig cfg(PrimitiveType::Triangles,
                                            CoordinateSystem2D::NDC,
                                            mtl::WithLocalToWorld::With);

            VILConfig vil_config;

            vil_config.Add(VAN::Color,VF_V4UN8);

            material_instance=CreateMaterialInstance(mtl::inline_material::VertexColor2D,&cfg,&vil_config);
        }

        if(!material_instance)
            return(false);

//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D);     //等同上一行，为Framework重载，默认使用swapchain的render target

        return pipeline;
    }

    bool InitVBO()
    {
        render_obj=CreateMesh("Triangle",VERTEX_COUNT,material_instance,pipeline,
                                    {
                                        {VAN::Position,   VF_V2F,     position_data},
                                        {VAN::Color,      VF_V4UN8,   color_data   }
                                    });

        if(!render_obj)
            return(false);

        double rad;

        CreateComponentInfo cci(GetSceneRoot());
        
        for(uint i=0;i<TRIANGLE_NUMBER;i++)
        {
            rad=deg2rad((360.0f/double(TRIANGLE_NUMBER))*i);       //这里一定要加<double>或<float>，否则结果用int保存会出现问题
            cci.mat=AxisRotate(rad,Vector3f(0,0,1));

            CreateComponent<MeshComponent>(&cci,render_obj);
        }

        return(true);
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        GetSceneRenderer()->SetClearColor(Color4f(0.2f,0.2f,0.2f,1.0f));

        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("AutoInstance"),1024,1024);
}
