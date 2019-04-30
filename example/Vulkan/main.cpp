#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

struct WorldConfig
{
    Matrix4f mvp;    
}world;

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
private:    //需释放数据

    vulkan::Material *          material            =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;    
    vulkan::Buffer *            ubo_mvp             =nullptr;

    vulkan::PipelineCreater *   pipeline_creater    =nullptr;
    vulkan::Pipeline *          pipeline            =nullptr;
    vulkan::CommandBuffer *     cmd_buf             =nullptr;

    vulkan::VertexBuffer *      vertex_buffer       =nullptr;
    vulkan::VertexBuffer *      color_buffer        =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(color_buffer);
        SAFE_CLEAR(vertex_buffer);
        SAFE_CLEAR(cmd_buf);
        SAFE_CLEAR(pipeline);
        SAFE_CLEAR(pipeline_creater);
        SAFE_CLEAR(ubo_mvp);
        SAFE_CLEAR(render_obj);
        SAFE_CLEAR(material);
    }

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("FlatColor.vert.spv"),
                                               OS_TEXT("FlatColor.frag.spv"));
        if(!material)
            return(false);

        render_obj=material->CreateRenderable();
        return(true);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=device->GetExtent();

        world.mvp=ortho(extent.width,extent.height);

        ubo_mvp=device->CreateUBO(sizeof(WorldConfig),&world);

        if(!ubo_mvp)
            return(false);

        return material->UpdateUBO("world",*ubo_mvp);
    }

    void InitVBO()
    {    
        vertex_buffer   =device->CreateVBO(FMT_RG32F,  VERTEX_COUNT,vertex_data);
        color_buffer    =device->CreateVBO(FMT_RGB32F, VERTEX_COUNT,color_data);

        render_obj->Set("Vertex",   vertex_buffer);
        render_obj->Set("Color",    color_buffer);
    }

    bool InitPipeline()
    {
        pipeline_creater=new vulkan::PipelineCreater(device,material);
        pipeline_creater->SetDepthTest(false);
        pipeline_creater->SetDepthWrite(false);
        pipeline_creater->CloseCullFace();
        pipeline_creater->Set(PRIM_TRIANGLES);

        pipeline=pipeline_creater->Create();

        return pipeline;
    }

    bool InitCommandBuffer()
    {
        cmd_buf=device->CreateCommandBuffer();

        if(!cmd_buf)
            return(false);

        cmd_buf->Begin();
            cmd_buf->BeginRenderPass(device->GetRenderPass(),device->GetFramebuffer(0));
                cmd_buf->Bind(pipeline);
                cmd_buf->Bind(material);
                cmd_buf->Bind(render_obj);
                cmd_buf->Draw(VERTEX_COUNT);
            cmd_buf->EndRenderPass();
        cmd_buf->End();

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

        InitVBO();

        if(!InitPipeline())
            return(false);

        if(!InitCommandBuffer())
            return(false);

        return(true);
    }

    void Draw()
    {
        Submit(cmd_buf);
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    #ifdef _DEBUG
    if(!vulkan::CheckStrideBytesByFormat())
        return 0xff;
    #endif//

    TestApp app;

    if(!app.Init())
        return(-1);

    app.AcquireNextFrame();
    app.Draw();

    while(app.Run());
    
    return 0;
}
