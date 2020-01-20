// indices_rect
// 该示例演示使用索引数据画一个矩形，并使用了颜色材质

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

constexpr uint32_t VERTEX_COUNT=4;

static Vector4f color(1,1,0,1);

constexpr float SSP=0.25;
constexpr float SSN=1-SSP;

constexpr float vertex_data[VERTEX_COUNT][2]=
{
    {SCREEN_WIDTH*SSP,  SCREEN_HEIGHT*SSP},
    {SCREEN_WIDTH*SSN,  SCREEN_HEIGHT*SSP},
    {SCREEN_WIDTH*SSP,  SCREEN_HEIGHT*SSN},
    {SCREEN_WIDTH*SSN,  SCREEN_HEIGHT*SSN}
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
    vulkan::MaterialInstance *  material_instance   =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::Buffer *            ubo_mvp             =nullptr;
    vulkan::Buffer *            ubo_color_material  =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;

    vulkan::VertexBuffer *      vertex_buffer       =nullptr;
    vulkan::IndexBuffer *       index_buffer        =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(index_buffer);
        SAFE_CLEAR(vertex_buffer);
        SAFE_CLEAR(pipeline);
        SAFE_CLEAR(ubo_color_material);
        SAFE_CLEAR(ubo_mvp);
        SAFE_CLEAR(render_obj);
        SAFE_CLEAR(material_instance);
        SAFE_CLEAR(material);
    }

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("res/shader/OnlyPosition.vert.spv"),
                                               OS_TEXT("res/shader/FlatColor.frag.spv"));
        if(!material)
            return(false);

        render_obj=material->CreateRenderable(VERTEX_COUNT);
        material_instance=material->CreateInstance();
        return(true);
    }

    vulkan::Buffer *CreateUBO(const UTF8String &name,const VkDeviceSize size,void *data)
    {
        vulkan::Buffer *ubo=device->CreateUBO(size,data);

        if(!ubo)
            return(nullptr);

        if(!material_instance->BindUBO(name,ubo))
        {
            SAFE_CLEAR(ubo);
            return(nullptr);
        }

        return ubo;
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        wm.ortho=ortho(extent.width,extent.height);

        ubo_mvp             =CreateUBO("world",         sizeof(WorldMatrix),&wm);
        ubo_color_material  =CreateUBO("color_material",sizeof(Vector4f),&color);
        
        material_instance->Update();
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
            
        BuildCommandBuffer(pipeline,material_instance,render_obj);

        return(true);
    }

    void Resize(int,int)override
    {
        BuildCommandBuffer(pipeline,material_instance,render_obj);
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
