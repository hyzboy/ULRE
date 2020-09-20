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
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::Buffer *            ubo_world_matrix    =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;

    vulkan::VAB *               vertex_buffer       =nullptr;
    vulkan::VAB *               color_buffer        =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(color_buffer);
        SAFE_CLEAR(vertex_buffer);
    }

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
        render_obj  =db->CreateRenderable(material_instance,VERTEX_COUNT);
        if(!render_obj)return(false);

        vertex_buffer   =device->CreateVAB(FMT_RG32F,  VERTEX_COUNT,vertex_data);
        color_buffer    =device->CreateVAB(FMT_RGB32F, VERTEX_COUNT,color_data);

        if(!render_obj->Set(VAN::Position,  vertex_buffer))return(false);
        if(!render_obj->Set(VAN::Color,     color_buffer))return(false);

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

        BuildCommandBuffer(pipeline,material_instance,render_obj);

        return(true);
    }

    void Resize(int w,int h)override
    {
        cam.width=w;
        cam.height=h;

        cam.Refresh();

        ubo_world_matrix->Write(&cam.matrix);

        BuildCommandBuffer(pipeline,material_instance,render_obj);
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
