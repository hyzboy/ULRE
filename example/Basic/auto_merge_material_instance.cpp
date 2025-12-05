// 该范例主要演示使用一个材质下的不同材质实例传递颜色参数绘制三角形，并依赖RenderCollector中的自动合并功能，让同一材质下所有不同材质实例的对象一次渲染完成。

#include<hgl/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/color/Color.h>
#include<hgl/component/PrimitiveComponent.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t VERTEX_COUNT=3;

constexpr float position_data[VERTEX_COUNT*2]=
{
     0.0,  0.0,
    -0.1,  0.9,
     0.1,  0.9
};

constexpr uint DRAW_OBJECT_COUNT=12;
constexpr double TRI_ROTATE_ANGLE=360.0f/DRAW_OBJECT_COUNT;

//#define USE_MATERIAL_FILE   true        //是否使用材质文件

class TestApp:public WorkObject
{
    Material *          material            =nullptr;

    struct
    {
        MaterialInstance *  mi;
        Primitive *              primitive;
    }render_obj[DRAW_OBJECT_COUNT]{};

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        {
            mtl::Material2DCreateConfig cfg(PrimitiveType::Triangles,CoordinateSystem2D::NDC,mtl::WithLocalToWorld::With);

        #ifndef USE_MATERIAL_FILE
            mtl::MaterialCreateInfo *mci=mtl::CreatePureColor2D(GetDevAttr(),&cfg);                       //走程序内置材质创建函数

            material=CreateMaterial("PureColor2D",mci);
        #else
            material=LoadMaterial("Std2D/PureColor2D",&cfg);                                            //走材质文件加载
        #endif//USE_MATERIAL_FILE

            if(!material)
                return(false);

            for(uint i=0;i<DRAW_OBJECT_COUNT;i++)
            {
                render_obj[i].mi=CreateMaterialInstance(material);

                if(!render_obj[i].mi)
                    return(false);

                Color4f color=GetColor4f((COLOR)(i+int(COLOR::Blue)),1.0f);

                render_obj[i].mi->WriteMIData(color);       //设置MaterialInstance的数据
            }
        }

        pipeline=CreatePipeline(material,InlinePipeline::Solid2D);
        
        return pipeline;
    }

    bool InitVBOAndRenderList()
    {
        Geometry *geometry=CreateGeometry("Triangle",VERTEX_COUNT,material->GetDefaultVIL(),
                                        {{VAN::Position,   VF_V2F, position_data}});

        if(!geometry)
            return(false);

        Add(geometry);

        CreateComponentInfo cci(GetWorldRootNode());
        
        for(uint i=0;i<DRAW_OBJECT_COUNT;i++)
        {
            render_obj[i].primitive=CreatePrimitive(geometry,render_obj[i].mi,pipeline);

            if(!render_obj[i].primitive)
                return(false);

            cci.mat=AxisRotate(deg2rad(TRI_ROTATE_ANGLE*i),AxisVector::Z);

            CreateComponent<PrimitiveComponent>(&cci,render_obj[i].primitive);
        }

        return(true);
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        if(!InitMaterial())
            return(false);

        if(!InitVBOAndRenderList())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("AutoInstance"),1024,1024);
}
