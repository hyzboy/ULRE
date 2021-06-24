﻿// 1.two_triangle
// 该范例主要演示使用CmdBindDescriptorSets偏移UBO绘图

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

constexpr uint32_t VERTEX_COUNT=3;

constexpr float vertex_data[VERTEX_COUNT][2]=
{
    {SCREEN_WIDTH*0.5,   SCREEN_HEIGHT*0.25},
    {SCREEN_WIDTH*0.75,  SCREEN_HEIGHT*0.75},
    {SCREEN_WIDTH*0.25,  SCREEN_HEIGHT*0.75}
};

constexpr float color_data[VERTEX_COUNT][4]=
{   {1,0,0,1},
    {0,1,0,1},
    {0,0,1,1}
};

class TestApp:public VulkanApplicationFramework
{
private:

    Camera cam;

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;

    MaterialInstance *  material_instance   =nullptr;
    RenderableInstance *render_instance     =nullptr;
    GPUBuffer *         ubo_camera_info     =nullptr;

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/VertexColor2D"));

        if(!material_instance)
            return(false);
            
//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D);     //等同上一行，为Framework重载，默认使用swapchain的render target

        return pipeline;
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        cam.width=extent.width;
        cam.height=extent.height;

        cam.Refresh();

        ubo_camera_info=db->CreateUBO(sizeof(CameraInfo),&cam.info);

        if(!ubo_camera_info)
            return(false);
            
        {
            MaterialParameters *mp_global=material_instance->GetMP(DescriptorSetType::Global);
        
            if(!mp_global)
                return(false);

            mp_global->BindUBO("g_camera",ubo_camera_info);
            mp_global->Update();
        }
        return(true);
    }

    bool InitVBO()
    {
        Renderable *render_obj=db->CreateRenderable(VERTEX_COUNT);
        if(!render_obj)return(false);

        if(!render_obj->Set(VAN::Position,  db->CreateVAB(VF_VEC2,VERTEX_COUNT,vertex_data)))return(false);
        if(!render_obj->Set(VAN::Color,     db->CreateVAB(VF_VEC4,VERTEX_COUNT,color_data)))return(false);
        
        render_instance=db->CreateRenderableInstance(render_obj,material_instance,pipeline);

        render_root.CreateSubNode(scale(0.5,0.5),render_instance);

        render_root.RefreshMatrix();

        render_list->Expend(cam.info,&render_root);

        return(true);
    }

public:

    ~TestApp()
    {
        SAFE_CLEAR(render_list);
    }

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        render_list=new RenderList(device);

        if(!InitMaterial())
            return(false);

        if(!InitUBO())
            return(false);

        if(!InitVBO())
            return(false);

        BuildCommandBuffer(render_list);

        return(true);
    }

    void Resize(int w,int h)override
    {
        cam.width=w;
        cam.height=h;

        cam.Refresh();

        ubo_camera_info->Write(&cam.info);

        BuildCommandBuffer(render_list);
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
