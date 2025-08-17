// 画一个圆角矩形，它是UI的基本绘图元件
// 通过控制尺寸、每个角的半径，可绘制出正圆、矩形、圆角矩形

#include<hgl/WorkManager.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/math/Math.h>

#include<hgl/component/MeshComponent.h>

using namespace hgl;
using namespace hgl::graph;

constexpr float position_data[4]=
{
    0,     //left
    0,     //top
    1,     //right
    1      //bottom
};

constexpr float tex_coord_data[4]=
{
    0,0,
    1,1
};

struct RoundedRectConfig
{
    
};

class TestApp:public WorkObject
{
private:

    Texture2D *         texture             =nullptr;
    Sampler *           sampler             =nullptr;
    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        mtl::Material2DCreateConfig cfg(PrimitiveType::SolidRectangles,
                                        CoordinateSystem2D::ZeroToOne,
                                        mtl::WithLocalToWorld::Without);

        material=db->LoadMaterial("Std2D/RectTexture2D",&cfg);

        if(!material)
            return(false);

        //        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material,InlinePipeline::Solid2D);     //等同上一行，为Framework重载，默认使用swapchain的render target

        if(!pipeline)
            return(false);

        texture=LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"),true);

        if(!texture)return(false);

        sampler=db->CreateSampler();

        if(!material->BindImageSampler( DescriptorSetType::PerMaterial,     ///<描述符合集
           mtl::SamplerName::BaseColor,        ///<采样器名称
           texture,                            ///<纹理
           sampler))                           ///<采样器
            return(false);

        material_instance=db->CreateMaterialInstance(material);

        return(true);
    }

    bool InitVBO()
    {
        Mesh *render_obj=CreateMesh("TextureRect",1,material_instance,pipeline,
                                    {
                                        {VAN::Position,VF_V4F,position_data},
                                    {VAN::TexCoord,VF_V4F,tex_coord_data}
                                    });

        if(!render_obj)
            return(false);

        CreateComponentInfo cci(GetSceneRoot());

        return CreateComponent<MeshComponent>(&cci,render_obj); //创建一个静态网格组件
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
    return RunFramework<TestApp>(OS_TEXT("Draw a rectangle with texture"),256,256);
}
