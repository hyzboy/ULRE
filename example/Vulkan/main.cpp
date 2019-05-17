// 0.triangle
// 该范例主要演示直接绘制一个渐变色的三角形

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;
using namespace hgl::graph;

bool SaveToFile(const OSString &filename,VK_NAMESPACE::PipelineCreater *pc);
bool LoadFromFile(const OSString &filename,VK_NAMESPACE::PipelineCreater *pc);

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
private:

    uint swap_chain_count=0;

    vulkan::Material *          material            =nullptr;
    vulkan::DescriptorSets *    desciptor_sets      =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::Buffer *            ubo_mvp             =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;
    vulkan::CommandBuffer **    cmd_buf             =nullptr;

    vulkan::VertexBuffer *      vertex_buffer       =nullptr;
    vulkan::VertexBuffer *      color_buffer        =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(color_buffer);
        SAFE_CLEAR(vertex_buffer);
        SAFE_CLEAR_OBJECT_ARRAY(cmd_buf,swap_chain_count);
        SAFE_CLEAR(pipeline);
        SAFE_CLEAR(ubo_mvp);
        SAFE_CLEAR(render_obj);
        SAFE_CLEAR(desciptor_sets);
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
        desciptor_sets=material->CreateDescriptorSets();
        return(true);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=device->GetExtent();

        world.mvp=ortho(extent.width,extent.height);

        ubo_mvp=device->CreateUBO(sizeof(WorldConfig),&world);

        if(!ubo_mvp)
            return(false);

        return desciptor_sets->UpdateUBO(material->GetUBO("world"),*ubo_mvp);       //material中这里可以改成不区分类型，返回的值包含类型和ID,这样descriptor_sets->Update也不再需要类型
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
        constexpr os_char PIPELINE_FILENAME[]=OS_TEXT("2DSolid.pipeline");

        {
            vulkan::PipelineCreater *pipeline_creater=new vulkan::PipelineCreater(device,material,device->GetRenderPass(),device->GetExtent());
            pipeline_creater->SetDepthTest(false);
            pipeline_creater->SetDepthWrite(false);
            pipeline_creater->CloseCullFace();
            pipeline_creater->Set(PRIM_TRIANGLES);

            SaveToFile(PIPELINE_FILENAME,pipeline_creater);

            delete pipeline_creater;
        }

        {
            void *data;
            uint size=filesystem::LoadFileToMemory(PIPELINE_FILENAME,(void **)&data);

            vulkan::PipelineCreater *pipeline_creater=new vulkan::PipelineCreater(device,material,device->GetRenderPass(),device->GetExtent(),(uchar *)data,size);

            pipeline=pipeline_creater->Create();

            delete pipeline_creater;
        }

        return pipeline;
    }

    bool InitCommandBuffer()
    {
        cmd_buf=hgl_zero_new<vulkan::CommandBuffer *>(swap_chain_count);

        for(uint i=0;i<swap_chain_count;i++)
        {
            cmd_buf[i]=device->CreateCommandBuffer();

            if(!cmd_buf[i])
                return(false);

            cmd_buf[i]->Begin();
                cmd_buf[i]->BeginRenderPass(device->GetRenderPass(),device->GetFramebuffer(i));
                    cmd_buf[i]->Bind(pipeline);
                    cmd_buf[i]->Bind(desciptor_sets);
                    cmd_buf[i]->Bind(render_obj);
                    cmd_buf[i]->Draw(VERTEX_COUNT);
                cmd_buf[i]->EndRenderPass();
            cmd_buf[i]->End();
        }

        return(true);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        swap_chain_count=device->GetSwapChainImageCount();

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

    void Draw() override
    {
        const uint32_t frame_index=device->GetCurrentFrameIndices();

        const vulkan::CommandBuffer *cb=cmd_buf[frame_index];

        Submit(*cb);
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
