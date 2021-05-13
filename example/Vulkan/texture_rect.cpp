// 2.texture_rect
// 该示例是1.indices_rect的进化，演示在矩形上贴上贴图

#include"VulkanAppFramework.h"
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/VKInlinePipeline.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTextureFromFile(GPUDevice *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_WIDTH=256;
constexpr uint32_t SCREEN_HEIGHT=256;

constexpr uint32_t VERTEX_COUNT=4;

constexpr float vertex_data[VERTEX_COUNT][2]=
{
    {0,             0},
    {SCREEN_WIDTH,  0},
    {0,             SCREEN_HEIGHT},
    {SCREEN_WIDTH,  SCREEN_HEIGHT}
};

constexpr float tex_coord_data[VERTEX_COUNT][2]=
{
    {0,0},
    {1,0},
    {0,1},
    {1,1}
};

constexpr uint32_t INDEX_COUNT=6;

constexpr uint16 index_data[INDEX_COUNT]=
{
    0,1,3,
    0,3,2
};

class TestApp:public VulkanApplicationFramework
{
    Camera cam;

private:

    Texture2D *         texture             =nullptr;
    Sampler *           sampler             =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    RenderableInstance *renderable_instance =nullptr;
    GPUBuffer *         ubo_camera_info    =nullptr;
    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/Texture2D"));        
        if(!material_instance)return(false);

        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D);
        if(!pipeline)return(false);

        texture=db->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"));
        if(!texture)return(false);

        sampler=db->CreateSampler();

        material_instance->BindSampler("tex",texture,sampler);

        return(true);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        cam.vp_width=cam.width=extent.width;
        cam.vp_height=cam.height=extent.height;        

        cam.Refresh();

        ubo_camera_info=db->CreateUBO(sizeof(CameraInfo),&cam.info);

        if(!ubo_camera_info)
            return(false);

        if(!material_instance->BindUBO("camera",ubo_camera_info))return(false);
        material_instance->Update();
        return(true);
    }

    bool InitVBO()
    {
        auto render_obj=db->CreateRenderable(VERTEX_COUNT);
        if(!render_obj)return(false);

        if(!render_obj->Set(VAN::Position,db->CreateVAB(VAF_VEC2,VERTEX_COUNT,vertex_data)))return(false);
        if(!render_obj->Set(VAN::TexCoord,db->CreateVAB(VAF_VEC2,VERTEX_COUNT,tex_coord_data)))return(false);
        if(!render_obj->Set(db->CreateIBO16(INDEX_COUNT,index_data)))return(false);

        renderable_instance=db->CreateRenderableInstance(render_obj,material_instance,pipeline);

        return(true);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);
            
        if(!InitMaterial())
            return(false);

        if(!InitUBO())
            return(false);

        if(!InitVBO())
            return(false);
            
        BuildCommandBuffer(renderable_instance);

        return(true);
    }

    void Resize(int w,int h)override
    {
        cam.width=w;
        cam.height=h;

        cam.Refresh();

        ubo_camera_info->Write(&cam.info);
        
        BuildCommandBuffer(renderable_instance);
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
