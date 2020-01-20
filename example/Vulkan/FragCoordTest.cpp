// 该范例主要用于测试gl_FragCoord值

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;
using namespace hgl::graph;

bool SaveToFile(const OSString &filename,VK_NAMESPACE::PipelineCreater *pc);
bool LoadFromFile(const OSString &filename,VK_NAMESPACE::PipelineCreater *pc);

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

constexpr uint32_t VERTEX_COUNT=4;

constexpr float vertex_data[VERTEX_COUNT][2]=
{
    {0,0},
    {SCREEN_WIDTH,0},
    {0,SCREEN_HEIGHT},
    {SCREEN_WIDTH,SCREEN_HEIGHT}
};

class TestApp:public VulkanApplicationFramework
{
private:

    Camera cam;

    vulkan::Material *          material            =nullptr;
    vulkan::DescriptorSets *    descriptor_sets     =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::Buffer *            ubo_mvp             =nullptr;
    vulkan::Buffer *            ubo_mvp_fs          =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;

    vulkan::VertexBuffer *      vertex_buffer       =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(vertex_buffer);
        SAFE_CLEAR(pipeline);
        SAFE_CLEAR(ubo_mvp_fs);
        SAFE_CLEAR(ubo_mvp);
        SAFE_CLEAR(render_obj);
        SAFE_CLEAR(descriptor_sets);
        SAFE_CLEAR(material);
    }

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("res/shader/OnlyPosition.vert.spv"),
                                               OS_TEXT("res/shader/FragCoord.frag.spv"));
        if(!material)
            return(false);

        render_obj=material->CreateRenderable(VERTEX_COUNT);
        descriptor_sets=material->CreateDescriptorSets();
        return(true);
    }

    vulkan::Buffer *CreateUBO(const UTF8String &name,const VkDeviceSize size,void *data)
    {
        vulkan::Buffer *ubo=device->CreateUBO(size,data);

        if(!ubo)
            return(nullptr);

        const int index=material->GetUBO(name);

        if(index<0)
        {
            SAFE_CLEAR(ubo);
            return(nullptr);
        }

        if(!descriptor_sets->BindUBO(index,ubo))
        {
            SAFE_CLEAR(ubo);
            return(nullptr);
        }

        return ubo;
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        cam.width=extent.width;
        cam.height=extent.height;

        cam.Refresh();
        
        ubo_mvp     =CreateUBO("world",         sizeof(WorldMatrix),&cam.matrix);
        ubo_mvp_fs  =CreateUBO("fragment_world",sizeof(WorldMatrix),&cam.matrix);

        descriptor_sets->Update();
        return(true);
    }
    
    void InitVBO()
    {
        vertex_buffer   =device->CreateVBO(FMT_RG32F,  VERTEX_COUNT,vertex_data);

        render_obj->Set("Vertex",   vertex_buffer);
    }

    bool InitPipeline()
    {
        AutoDelete<vulkan::PipelineCreater> 
        pipeline_creater=new vulkan::PipelineCreater(device,material,sc_render_target);
        pipeline_creater->CloseCullFace();
        pipeline_creater->Set(PRIM_TRIANGLE_STRIP);

        pipeline=pipeline_creater->Create();

        return pipeline;
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

        BuildCommandBuffer(pipeline,descriptor_sets,render_obj);

        return(true);
    }

    void Resize(int w,int h)override
    {
        cam.width=w;
        cam.height=h;

        cam.Refresh();

        ubo_mvp->Write(&cam.matrix);
        ubo_mvp_fs->Write(&cam.matrix);

        BuildCommandBuffer(pipeline,descriptor_sets,render_obj);
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
