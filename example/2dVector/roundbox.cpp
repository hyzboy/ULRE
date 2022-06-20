#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>
#include<iostream>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=256;
constexpr uint32_t SCREEN_HEIGHT=256;

constexpr uint32_t VERTEX_COUNT=1;

constexpr int16_t position_data[VERTEX_COUNT][4]=
{
    SCREEN_WIDTH*0.25,  SCREEN_HEIGHT*0.25,             //left,top
    SCREEN_WIDTH*0.75 , SCREEN_HEIGHT*0.75              //right,bottom
};

struct RoundedBoxConfig
{
    Color4f Color;
    Color4f InColor;
    Color4f OutColor;
    float Radius[4];        //bottom-right,top-right,bottom-left,top-left

    float LineWidth;
};

static RoundedBoxConfig rb_config;

class TestApp:public VulkanApplicationFramework
{
private:

    Camera cam;

    MaterialInstance *  material_instance   =nullptr;
    RenderableInstance *render_instance     =nullptr;

    GPUBuffer *         ubo_camera_info     =nullptr;
    GPUBuffer *         ubo_rb_config       =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        //2D渲染Position坐标全部是使用整数，这里强制要求Position输入流使用RGBA16I格式
        VABConfigInfo vab_config;
        VAConfig va_cfg;

        va_cfg.format=VF_V4I16;
        va_cfg.instance=false;

        vab_config.Add("Position",va_cfg);

        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/RoundedBox"),&vab_config);
        if(!material_instance)
            return(false);
            
        pipeline=CreatePipeline(material_instance,OS_TEXT("res/pipeline/alpha2d"),Prim::SolidRectangles);     //等同上一行，为Framework重载，默认使用swapchain的render target

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

            if(!mp_global->BindUBO("g_camera",ubo_camera_info))
                return(false);

            mp_global->Update();
        }

        {
            MaterialParameters *mp_value=material_instance->GetMP(DescriptorSetsType::Value);
        
            if(!mp_value)
                return(false);

            rb_config.Color.Set(1,1,1,1);
            rb_config.OutColor.Set(0,0,0,0);
            rb_config.InColor=GetColor4f(COLOR::MozillaOrange,1.0);
            rb_config.Radius[0]=0;
            rb_config.Radius[1]=16;
            rb_config.Radius[2]=0;
            rb_config.Radius[3]=16;
            rb_config.LineWidth=2;

            ubo_rb_config  =db->CreateUBO(sizeof(RoundedBoxConfig),       &rb_config);

            if(!mp_value->BindUBO("rb_config",ubo_rb_config))
                return(false);

            mp_value->Update();
        }

        return(true);
    }
    
    bool InitVBO()
    {
        Renderable *render_obj=db->CreateRenderable(VERTEX_COUNT);
        if(!render_obj)return(false);

        if(!render_obj->Set(VAN::Position,  db->CreateVBO(VF_V4I16,VERTEX_COUNT,position_data)))return(false);
        
        render_instance=db->CreateRenderableInstance(render_obj,material_instance,pipeline);
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