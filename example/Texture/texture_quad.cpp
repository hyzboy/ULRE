// 画一个带纹理的四边形
#include<hgl/WorkManager.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/math/Math.h>

#include<hgl/component/MeshComponent.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTexture2DFromFile(VulkanDevice *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_WIDTH=256;
constexpr uint32_t SCREEN_HEIGHT=256;

constexpr uint32_t VERTEX_COUNT=4;

constexpr float position_data[VERTEX_COUNT][2]=
{
    {-1, -1},
    { 1, -1},
    { 1,  1},
    {-1,  1},
};

constexpr float tex_coord_data[VERTEX_COUNT][2]=
{
    {0,0},
    {1,0},
    {1,1},
    {0,1}
};

class TestApp:public WorkObject
{
private:

    Texture2D *         texture             =nullptr;
    Sampler *           sampler             =nullptr;
    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    Mesh *              render_obj          =nullptr;
    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        mtl::Material2DCreateConfig cfg(PrimitiveType::Fan,
                                        CoordinateSystem2D::NDC,
                                        mtl::WithLocalToWorld::Without);

        material=db->LoadMaterial("Std2D/PureTexture2D",&cfg);

        if(!material)
            return(false);

//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material,InlinePipeline::Solid2D);     //等同上一行，为Framework重载，默认使用swapchain的render target

        if(!pipeline)
            return(false);

        TextureManager *tex_manager=GetTextureManager();

        texture=tex_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"),true);
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
        render_obj=CreateMesh(  "TextureQuad",VERTEX_COUNT,material_instance,pipeline,
                                {
                                    {VAN::Position,   VF_V2F, position_data},
                                    {VAN::TexCoord,   VF_V2F, tex_coord_data}
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
};//class TestApp:public VulkanApplicationFramework

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Draw a quad with texture"));
}
