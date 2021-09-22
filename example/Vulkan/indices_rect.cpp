﻿// indices_rect
// 该示例演示使用索引数据画一个矩形，并使用了颜色材质

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

constexpr uint32_t VERTEX_COUNT=4;

static Vector4f color(1,1,1,1);

constexpr float vertex_data[VERTEX_COUNT][2]=
{
    {0,0},
    {SCREEN_WIDTH,0},
    {0,SCREEN_HEIGHT},
    {SCREEN_WIDTH,SCREEN_HEIGHT}
};
 
constexpr uint32_t INDEX_COUNT=6;

constexpr uint16 index_data[INDEX_COUNT]=
{
    0,1,3,
    0,3,2
};

class TestApp:public VulkanApplicationFramework
{
private:

    Camera cam;

    MaterialInstance *  material_instance   =nullptr;
    RenderableInstance *renderable_instance =nullptr;
    GPUBuffer *         ubo_camera_info     =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/FragColor"));
        if(!material_instance)return(false);
        
        //        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Triangles);     //等同上一行，为Framework重载，默认使用swapchain的render target
        
        return pipeline;
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
            
        {        
            MaterialParameters *mp_global=material_instance->GetMP(DescriptorSetType::Global);
        
            if(!mp_global)
                return(false);

            if(!mp_global->BindUBO("g_camera",ubo_camera_info))return(false);

            mp_global->Update();
        }
        return(true);
    }

    bool InitVBO()
    {        
        auto render_obj=db->CreateRenderable(VERTEX_COUNT);
        if(!render_obj)return(false);

        if(!render_obj->Set(VAN::Position,db->CreateVAB(VF_VEC2,VERTEX_COUNT,vertex_data)))return(false);
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
        cam.vp_width=w;
        cam.vp_height=h;

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
