#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>
#include<iostream>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

constexpr uint32_t VERTEX_COUNT=4;

constexpr float position_data[VERTEX_COUNT][2]=
{
    {SCREEN_WIDTH*0.5,   SCREEN_HEIGHT*0.25},
    {SCREEN_WIDTH*0.75,  SCREEN_HEIGHT*0.75},
    {SCREEN_WIDTH*0.25,  SCREEN_HEIGHT*0.75},
    {SCREEN_WIDTH*0.5,   SCREEN_HEIGHT*0.25},
};

static Vector4f color(1,1,1,1);

struct Line2DConfig
{
    float width=20.0f;
    float border=5.0f;
};

static Line2DConfig line_2d_config;

class TestApp:public VulkanApplicationFramework
{
private:

    Camera cam;

    MaterialInstance *  material_instance   =nullptr;
    RenderableInstance *render_instance     =nullptr;
    GPUBuffer *         ubo_camera_info     =nullptr;
    GPUBuffer *         ubo_color_material  =nullptr;
    GPUBuffer *         ubo_line_config     =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/Line2D"));

        if(!material_instance)
            return(false);
            
//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,OS_TEXT("res/pipeline/alpha2d"),Prim::LineStrip);     //等同上一行，为Framework重载，默认使用swapchain的render target
        //pipeline=CreatePipeline(material_instance,InlinePipeline::Alpha2D,Prim::LineStrip);     //等同上一行，为Framework重载，默认使用swapchain的render target

        if(!pipeline)
            return(false);
        
        return(true);
    }

    GPUBuffer *CreateUBO(const AnsiString &name,const VkDeviceSize size,void *data)
    {
        GPUBuffer *ubo=db->CreateUBO(size,data);

        if(!ubo)
            return(nullptr);

        return ubo;
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        cam.width=extent.width;
        cam.height=extent.height;

        cam.RefreshCameraInfo();

        {
            MaterialParameters *mp_global=material_instance->GetMP(DescriptorSetsType::Global);
        
            if(!mp_global)
                return(false);
        
            ubo_camera_info     =db->CreateUBO(sizeof(CameraInfo),     &cam.info);

            mp_global->BindUBO("g_camera",ubo_camera_info);
            mp_global->Update();
        }

        {
        
            MaterialParameters *mp_value=material_instance->GetMP(DescriptorSetsType::Value);
        
            if(!mp_value)
                return(false);

            ubo_color_material  =db->CreateUBO(sizeof(Vector4f),       &color);
            ubo_line_config     =db->CreateUBO(sizeof(Line2DConfig),   &line_2d_config);

            mp_value->BindUBO("color_material",ubo_color_material);
            mp_value->BindUBO("line2d_config",ubo_line_config);
            mp_value->Update();
        }

        return(true);
    }
    
    bool InitVBO()
    {
        Primitive *primitive=db->CreatePrimitive(VERTEX_COUNT);
        if(!primitive)return(false);

        if(!primitive->Set(VAN::Position,  db->CreateVBO(VF_V2F,VERTEX_COUNT,position_data)))return(false);
        
        render_instance=db->CreateRenderableInstance(primitive,material_instance,pipeline);
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

        BuildCommandBuffer(render_instance);

        return(true);
    }

    void Resize(int w,int h)override
    {
        cam.width=w;
        cam.height=h;

        cam.RefreshCameraInfo();

        ubo_camera_info->Write(&cam.info);

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
