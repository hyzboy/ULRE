// 2.RectanglePrimivate
// 该示例是texture_rect的进化，演示使用GeometryShader画矩形

#include"VulkanAppFramework.h"
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTextureFromFile(GPUDevice *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_SIZE=512;

constexpr uint32_t VERTEX_COUNT=1;

constexpr float BORDER=0.1f;

constexpr float vertex_data[4]=
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
    Camera cam;

private:

    Texture2D *         texture             =nullptr;
    Sampler *           sampler             =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    Renderable *        render_obj          =nullptr;
    RenderableInstance *render_instance     =nullptr;
    GPUBuffer *         ubo_camera_matrix    =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/TextureRect2D"));
        if(!material_instance)return(false);

        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Rectangles);
        if(!pipeline)return(false);

        texture=db->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"));
        if(!texture)return(false);

        sampler=db->CreateSampler();

        material_instance->BindSampler("tex",texture,sampler);
        material_instance->BindUBO("camera",ubo_camera_matrix);
        material_instance->Update();

        db->Add(texture);
        return(true);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        cam.width=extent.width;
        cam.height=extent.height;

        cam.Refresh();

        ubo_camera_matrix=db->CreateUBO(sizeof(CameraInfo),&cam.matrix);

        if(!ubo_camera_matrix)
            return(false);

        return(true);
    }

    bool InitVBO()
    {        
        render_obj=db->CreateRenderable(VERTEX_COUNT);

        if(!render_obj)return(false);

        render_obj->Set(VAN::Position,db->CreateVAB(VAF_VEC4,VERTEX_COUNT,vertex_data));
        render_obj->Set(VAN::TexCoord,db->CreateVAB(VAF_VEC4,VERTEX_COUNT,tex_coord_data));
        
        render_instance=db->CreateRenderableInstance(render_obj,material_instance,pipeline);

        return(render_instance);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_SIZE,SCREEN_SIZE))
            return(false);

        if(!InitUBO())
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
        cam.width=w;
        cam.height=h;

        cam.Refresh();

        ubo_camera_matrix->Write(&cam.matrix);
        
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
