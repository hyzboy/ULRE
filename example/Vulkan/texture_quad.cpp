// texture quad
// 画一个带纹理的四边形

#include"VulkanAppFramework.h"
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/VKInlinePipeline.h>
#include<hgl/graph/VKRenderablePrimitiveCreater.h>
#include<hgl/graph/mtl/2d/Material2DCreateConfig.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTexture2DFromFile(GPUDevice *device,const OSString &filename);
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

constexpr float color_data[VERTEX_COUNT][4]=
{   
    {1,0,0,1},
    {1,1,0,1},
    {0,1,0,1},
    {0,1,1,1}
};

class TestApp:public VulkanApplicationFramework
{
private:

//    Texture2D *         texture             =nullptr;
//    Sampler *           sampler             =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    Renderable *        render_obj          =nullptr;
    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        mtl::Material2DCreateConfig cfg(device->GetDeviceAttribute(),"VertexColor2d");

        cfg.coordinate_system=CoordinateSystem2D::NDC;
        cfg.local_to_world=false;

        AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateVertexColor2D(&cfg);

        material_instance=db->CreateMaterialInstance(mci);

        if(!material_instance)
            return(false);

//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Fan);     //等同上一行，为Framework重载，默认使用swapchain的render target

        if(!pipeline)
            return(false);

        //texture=db->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"),true);
        //if(!texture)return(false);

        //sampler=db->CreateSampler();

        //if(!material_instance->BindImageSampler(DescriptorSetType::Value,"tex",texture,sampler))return(false);

        return(true);
    }

    bool InitVBO()
    {
        RenderablePrimitiveCreater rpc(db,VERTEX_COUNT);

        if(!rpc.SetVBO(VAN::Position,   VF_V2F, position_data))return(false);
        if(!rpc.SetVBO(VAN::Color,      VF_V4F, color_data   ))return(false);

        render_obj=rpc.Create(material_instance,pipeline);
        return(render_obj);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);
            
        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);
            
        BuildCommandBuffer(render_obj);

        return(true);
    }

    void Resize(int w,int h)override
    {
        VulkanApplicationFramework::Resize(w,h);
        
        BuildCommandBuffer(render_obj);
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
