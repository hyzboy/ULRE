// 画一个带纹理的矩形，2D模式专用

#include"VulkanAppFramework.h"
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/VKInlinePipeline.h>
#include<hgl/graph/VKRenderablePrimitiveCreater.h>
#include<hgl/graph/mtl/2d/Material2DCreateConfig.h>
#include<hgl/math/Math.h>
#include<hgl/filesystem/Filename.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
//Texture2D *CreateTexture2DFromFile(GPUDevice *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_WIDTH=256;
constexpr uint32_t SCREEN_HEIGHT=256;

float position_data[4]=
{
    0,      //left
    0,      //top
    1,      //right
    1       //bottom
};

constexpr float tex_coord_data[4]=
{
    0,0,
    1,1
};

constexpr const os_char *tex_filename[]=
{
    OS_TEXT("001-online resume.Tex2D"),
    OS_TEXT("002-salary.Tex2D"),
    OS_TEXT("003-application.Tex2D"),
    OS_TEXT("004-job interview.Tex2D")
};

constexpr const size_t TexCount=sizeof(tex_filename)/sizeof(os_char *);

class TestApp:public VulkanApplicationFramework
{
private:

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;

    Texture2DArray *    texture             =nullptr;
    Sampler *           sampler             =nullptr;
    Material *          material            =nullptr;

    Pipeline *          pipeline            =nullptr;
    DeviceBuffer *      tex_id_ubo          =nullptr;

    struct
    {
        MaterialInstance *  mi;
        Renderable *        r;
    }render_obj[TexCount]{};

private:

    bool InitTexture()
    {
        texture=db->CreateTexture2DArray(   512,512,            ///<纹理尺寸
                                            TexCount,           ///<纹理层数
                                            PF_BC1_RGBAUN,      ///<纹理格式
                                            false);             ///<是否自动产生mipmaps
        
        if(!texture)return(false);

        OSString filename;

        for(uint i=0;i<TexCount;i++)
        {
            filename=filesystem::MergeFilename(OS_TEXT("res/image/icon/freepik"),tex_filename[i]);

            if(!db->LoadTexture2DToArray(texture,i,filename))
                return(false);
        }

        return(true);
    }

    bool InitMaterial()
    {
        mtl::Material2DCreateConfig cfg(device->GetDeviceAttribute(),"RectTexture2DArray",Prim::SolidRectangles);

        cfg.coordinate_system=CoordinateSystem2D::ZeroToOne;
        cfg.local_to_world=true;

        AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateRectTexture2DArray(&cfg);

        material=db->CreateMaterial(mci);

        if(!material)
            return(false);

        pipeline=CreatePipeline(material,InlinePipeline::Solid2D,Prim::SolidRectangles);     //等同上一行，为Framework重载，默认使用swapchain的render target

        if(!pipeline)
            return(false);

        sampler=db->CreateSampler();

        if(!material->BindImageSampler( DescriptorSetType::PerMaterial,     ///<描述符合集
                                        mtl::SamplerName::Color,            ///<采样器名称
                                        texture,                            ///<纹理
                                        sampler))                           ///<采样器
            return(false);

        for(uint32_t i=0;i<TexCount;i++)
        {
            render_obj[i].mi=db->CreateMaterialInstance(material);

            if(!render_obj[i].mi)
                return(false);

            render_obj[i].mi->WriteMIData(i);       //设置MaterialInstance的数据
        }

        return(true);
    }

    bool InitVBOAndRenderList()
    {
        RenderablePrimitiveCreater rpc(db,1);

        position_data[2]=1.0f/float(TexCount);

        if(!rpc.SetVBO(VAN::Position,VF_V4F,position_data))return(false);
        if(!rpc.SetVBO(VAN::TexCoord,VF_V4F,tex_coord_data))return(false);

        Vector3f offset(1.0f/float(TexCount),0,0);

        for(uint32_t i=0;i<TexCount;i++)
        {
            render_obj[i].r=rpc.Create(render_obj[i].mi,pipeline);

            if(!render_obj[i].r)
                return(false);

            offset.x=position_data[2]*float(i);

            render_root.CreateSubNode(translate(offset),render_obj[i].r);
        }

        render_root.RefreshMatrix();

        render_list->Expend(&render_root);

        return(true);
    }

public:

    ~TestApp()
    {
        SAFE_CLEAR(render_list);
    }

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH*TexCount,SCREEN_HEIGHT))
            return(false);

        render_list=new RenderList(device);

        if(!InitTexture())
            return(false);

        if(!InitMaterial())
            return(false);

        if(!InitVBOAndRenderList())
            return(false);

        BuildCommandBuffer(render_list);

        return(true);
    }

    void Resize(int w,int h)override
    {
        VulkanApplicationFramework::Resize(w,h);
        
        BuildCommandBuffer(render_list);
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
#ifdef _DEBUG
    if(!CheckStrideBytesByFormat())
        return 0xff;
#endif//

    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
