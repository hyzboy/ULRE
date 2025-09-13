// 画一个带纹理的矩形，2D模式专用

#include<hgl/WorkManager.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/math/Math.h>
#include<hgl/filesystem/Filename.h>

#include<hgl/component/MeshComponent.h>

using namespace hgl;
using namespace hgl::graph;

constexpr const os_char *tex_filename[]=
{
    OS_TEXT("001-online resume.Tex2D"),
    OS_TEXT("002-salary.Tex2D"),
    OS_TEXT("003-application.Tex2D"),
    OS_TEXT("004-job interview.Tex2D")
};

constexpr const size_t TexCount=sizeof(tex_filename)/sizeof(os_char *);

constexpr const float position_data[4]=
{
    0,      //left
    0,      //top
    1.0f/float(TexCount),      //right
    1       //bottom
};

constexpr float tex_coord_data[4]=
{
    0,0,
    1,1
};

class TestApp:public WorkObject
{
private:

    Texture2DArray *    texture             =nullptr;
    Sampler *           sampler             =nullptr;
    Material *          material            =nullptr;

    Pipeline *          pipeline            =nullptr;

    struct
    {
        MaterialInstance *  mi;
        MeshComponent *     component;
    }render_obj[TexCount]{};

private:

    bool InitTexture()
    {
        texture=CreateTexture2DArray(   "freepik icons",
                                        512,512,            ///<纹理尺寸
                                        TexCount,           ///<纹理层数
                                     PF_BC7UN,      ///<纹理格式
                                        false);             ///<是否自动产生mipmaps
        
        if(!texture)return(false);

        OSString filename;

        for(uint i=0;i<TexCount;i++)
        {
            filename=filesystem::JoinPathWithFilename(OS_TEXT("res/image/icon/freepik"),tex_filename[i]);

            if(!LoadTexture2DArray(texture,i,filename))
                return(false);
        }

        return(true);
    }

    bool InitMaterial()
    {
        mtl::Material2DCreateConfig cfg(PrimitiveType::SolidRectangles,
                                        CoordinateSystem2D::ZeroToOne,
                                        mtl::WithLocalToWorld::With);

        material=LoadMaterial("Std2D/RectTexture2DArray",&cfg);

        if(!material)
            return(false);

        pipeline=CreatePipeline(material,InlinePipeline::Solid2D);     //等同上一行，为Framework重载，默认使用swapchain的render target

        if(!pipeline)
            return(false);

        sampler=CreateSampler();

        if(!material->BindImageSampler( DescriptorSetType::PerMaterial,     ///<描述符合集
                                        mtl::SamplerName::BaseColor,        ///<采样器名称
                                        texture,                            ///<纹理
                                        sampler))                           ///<采样器
            return(false);

        for(uint32_t i=0;i<TexCount;i++)
        {
            render_obj[i].mi=CreateMaterialInstance(material);

            if(!render_obj[i].mi)
                return(false);

            render_obj[i].mi->WriteMIData(i);       //设置MaterialInstance的数据
        }

        return(true);
    }

    bool InitVBOAndRenderList()
    {
        //CreateMesh传入MaterialInstance本质要的是VIL和Material
        //当前Mesh也是需要mi才能渲染。
        //这里因为所有render_obj的VIL/Material都是一样的，所以可以直接使用render_obj[0].mi

        Mesh *mesh_rect=CreateMesh( "TextureRect",1,render_obj[0].mi,pipeline,
                                    {
                                        {VAN::Position,VF_V4F,position_data},
                                        {VAN::TexCoord,VF_V4F,tex_coord_data}
                                    });

        if(!mesh_rect)
            return(false);

        CreateComponentInfo cci(GetSceneRoot());

        Vector3f offset(1.0f/float(TexCount),0,0);

        for(uint32_t i=0;i<TexCount;i++)
        {
            offset.x=position_data[2]*float(i);

            cci.mat=TranslateMatrix(offset);

            render_obj[i].component=CreateComponent<MeshComponent>(&cci,mesh_rect);

            if(!render_obj[i].component)
                return(false);

            render_obj[i].component->SetOverrideMaterial(render_obj[i].mi);
        }

        return(true);
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        if(!InitTexture())
            return(false);

        if(!InitMaterial())
            return(false);

        if(!InitVBOAndRenderList())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Draw many rectangle with texture"),256*TexCount,256);
}
