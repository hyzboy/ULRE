// 0.triangle
// 该范例主要演示直接绘制一个渐变色的三角形

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

constexpr float color_data[VERTEX_COUNT][3]=
{   {1,0,0},
    {0,1,0},
    {0,0,1}
};

class TestApp:public VulkanApplicationFramework
{
private:

    Camera cam;

    vulkan::MaterialInstance *  material_instance   =nullptr;
    vulkan::RenderableInstance *render_instance     =nullptr;
    vulkan::Buffer *            ubo_world_matrix    =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/VertexColor2D"));

        if(!material_instance)
            return(false);
            
//        pipeline=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/solid2d"));
        pipeline=CreatePipeline(material_instance,OS_TEXT("res/pipeline/solid2d"));     //等同上一行，为Framework重载，默认使用swapchain的render target

        if(!pipeline)
            return(false);
        
        return(true);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        cam.width=extent.width;
        cam.height=extent.height;

        cam.Refresh();

        ubo_world_matrix=db->CreateUBO(sizeof(WorldMatrix),&cam.matrix);

        if(!ubo_world_matrix)
            return(false);
            
        material_instance->BindUBO("world",ubo_world_matrix);
        material_instance->Update();
        return(true);
    }
    
    bool InitVBO()
    {
        vulkan::Renderable *render_obj=db->CreateRenderable(VERTEX_COUNT);
        if(!render_obj)return(false);

        if(!render_obj->Set(VAN::Position,  db->CreateVAB(VAF_VEC2,VERTEX_COUNT,vertex_data)))return(false);
        if(!render_obj->Set(VAN::Color,     db->CreateVAB(VAF_VEC3,VERTEX_COUNT,color_data)))return(false);
        
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

        cam.Refresh();

        ubo_world_matrix->Write(&cam.matrix);

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
