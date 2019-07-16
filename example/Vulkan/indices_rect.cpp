// 1.indices_rect
// 该示例是0.triangle的进化，演示使用索引数据画一个矩形

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

constexpr uint32_t VERTEX_COUNT=4;

constexpr float vertex_data[VERTEX_COUNT][2]=
{
    {SCREEN_WIDTH*0,  SCREEN_HEIGHT*0},
    {SCREEN_WIDTH*1,  SCREEN_HEIGHT*0},
    {SCREEN_WIDTH*0,  SCREEN_HEIGHT*1},
    {SCREEN_WIDTH*1,  SCREEN_HEIGHT*1}
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

    WorldMatrix                 wm;

    vulkan::Material *          material            =nullptr;
    vulkan::DescriptorSets *    descriptor_sets     =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::Buffer *            ubo_mvp             =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;

    vulkan::VertexBuffer *      vertex_buffer       =nullptr;
    vulkan::IndexBuffer *       index_buffer        =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(index_buffer);
        SAFE_CLEAR(vertex_buffer);
        SAFE_CLEAR(pipeline);
        SAFE_CLEAR(ubo_mvp);
        SAFE_CLEAR(render_obj);
        SAFE_CLEAR(descriptor_sets);
        SAFE_CLEAR(material);
    }

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("res/shader/OnlyPosition.vert.spv"),
                                               OS_TEXT("res/shader/glFragCoord.frag.spv"));
        if(!material)
            return(false);

        render_obj=material->CreateRenderable(VERTEX_COUNT);
        descriptor_sets=material->CreateDescriptorSets();
        return(true);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        wm.vp_size.x=extent.width;
        wm.vp_size.y=extent.height;

        wm.ortho=ortho(extent.width,extent.height);

        ubo_mvp=device->CreateUBO(sizeof(WorldMatrix),&wm);

        if(!ubo_mvp)
            return(false);

        if(!descriptor_sets->BindUBO(material->GetUBO("world"),ubo_mvp))
            return(false);

        descriptor_sets->Update();
        return(true);
    }

    void InitVBO()
    {
        vertex_buffer   =device->CreateVBO(FMT_RG32F,VERTEX_COUNT,vertex_data);
        index_buffer    =device->CreateIBO16(INDEX_COUNT,index_data);

        render_obj->Set("Vertex",vertex_buffer);
        render_obj->Set(index_buffer);
    }

    bool InitPipeline()
    {
        AutoDelete<vulkan::PipelineCreater> 
        pipeline_creater=new vulkan::PipelineCreater(device,material,sc_render_target);
        pipeline_creater->CloseCullFace();
        pipeline_creater->Set(PRIM_TRIANGLES);

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

    void Resize(int,int)override
    {
        BuildCommandBuffer(pipeline,descriptor_sets,render_obj);
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

    while(app.Run());

    return 0;
}
