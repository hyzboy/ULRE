// 2.RectanglePrimivate
// 该示例是texture_rect的进化，演示使用GeometryShader画矩形

#include"VulkanAppFramework.h"
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTexture2DFromFile(GPUDevice *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_SIZE=512;

constexpr uint32_t VERTEX_COUNT=1;

constexpr float BORDER=0.1f;

constexpr float position_data[4]=
{
    SCREEN_SIZE*BORDER,         SCREEN_SIZE*BORDER,
    SCREEN_SIZE*(1.0-BORDER),   SCREEN_SIZE*(1.0-BORDER)
};

constexpr float tex_coord_data[4]=
{
    0,0,1,1
};

class TestApp:public VulkanApplicationFramework
{
private:

    Texture2D *         texture             =nullptr;
    Sampler *           sampler             =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    Renderable *        render_obj          =nullptr;
    RenderableInstance *render_instance     =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/TextureRect2D"));
        if(!material_instance)return(false);

        BindCameraUBO(material_instance);

        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::SolidRectangles);
        if(!pipeline)return(false);

        texture=db->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"));
        if(!texture)return(false);

        sampler=db->CreateSampler();

        if(!material_instance->BindSampler(DescriptorSetsType::Value,"tex",texture,sampler))return(false);

        return(true);
    }

    bool InitVBO()
    {        
        render_obj=db->CreateRenderable(VERTEX_COUNT);

        if(!render_obj)return(false);

        render_obj->Set(VAN::Position,db->CreateVBO(VF_V4F,VERTEX_COUNT,position_data));
        render_obj->Set(VAN::TexCoord,db->CreateVBO(VF_V4F,VERTEX_COUNT,tex_coord_data));
        
        render_instance=db->CreateRenderableInstance(render_obj,material_instance,pipeline);

        return(render_instance);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_SIZE,SCREEN_SIZE))
            return(false);

        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);
            
        BuildCommandBuffer(render_instance);

        return(true);
    }

    void Resize(int w,int h)override
    {
        VulkanApplicationFramework::Resize(w,h);
        
        BuildCommandBuffer(render_instance);
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
