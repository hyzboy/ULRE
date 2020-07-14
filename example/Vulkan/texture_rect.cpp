// 2.texture_rect
// 该示例是1.indices_rect的进化，演示在矩形上贴上贴图

#include"VulkanAppFramework.h"
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTextureFromFile(Device *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

constexpr uint32_t VERTEX_COUNT=4;

constexpr float vertex_data[VERTEX_COUNT][2]=
{
    {0,             0},
    {SCREEN_WIDTH,  0},
    {0,             SCREEN_HEIGHT},
    {SCREEN_WIDTH,  SCREEN_HEIGHT}
};

constexpr float tex_coord_data[VERTEX_COUNT][2]=
{
    {0,0},
    {1,0},
    {0,1},
    {1,1}
};

constexpr uint32_t INDEX_COUNT=6;

constexpr uint16 index_data[INDEX_COUNT]=
{
    0,1,3,
    0,3,2
};

class TestApp:public VulkanApplicationFramework
{
    Camera cam;

private:

    vulkan::Material *          material            =nullptr;
    vulkan::Texture2D *         texture             =nullptr;
    vulkan::Sampler *           sampler             =nullptr;
    vulkan::MaterialInstance *  material_instance   =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::Buffer *            ubo_mvp             =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;

    vulkan::VertexAttribBuffer *      vertex_buffer       =nullptr;
    vulkan::VertexAttribBuffer *      tex_coord_buffer    =nullptr;
    vulkan::IndexBuffer *       index_buffer        =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(index_buffer);
        SAFE_CLEAR(tex_coord_buffer);
        SAFE_CLEAR(vertex_buffer);
        SAFE_CLEAR(pipeline);
        SAFE_CLEAR(ubo_mvp);
        SAFE_CLEAR(render_obj);
        SAFE_CLEAR(material_instance);
        SAFE_CLEAR(sampler);
        SAFE_CLEAR(texture);
        SAFE_CLEAR(material);
    }

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("res/shader/FlatTexture.vert"),
                                               OS_TEXT("res/shader/FlatTexture.frag"));
        if(!material)
            return(false);

        render_obj=material->CreateRenderable(VERTEX_COUNT);
        material_instance=material->CreateInstance();

        texture=vulkan::CreateTextureFromFile(device,OS_TEXT("res/image/lena.Tex2D"));

        sampler=device->CreateSampler();

        material_instance->BindSampler("tex",texture,sampler);
        material_instance->BindUBO("world",ubo_mvp);
        material_instance->Update();

        return(true);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=sc_render_target->GetExtent();

        cam.width=extent.width;
        cam.height=extent.height;

        cam.Refresh();

        ubo_mvp=device->CreateUBO(sizeof(WorldMatrix),&cam.matrix);

        if(!ubo_mvp)
            return(false);

        return(true);
    }

    void InitVBO()
    {
        vertex_buffer   =device->CreateVBO(FMT_RG32F,VERTEX_COUNT,vertex_data);
        tex_coord_buffer=device->CreateVBO(FMT_RG32F,VERTEX_COUNT,tex_coord_data);
        index_buffer    =device->CreateIBO16(INDEX_COUNT,index_data);

        render_obj->Set("Vertex",vertex_buffer);
        render_obj->Set("TexCoord",tex_coord_buffer);
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

        if(!InitUBO())
            return(false);

        if(!InitMaterial())
            return(false);

        InitVBO();

        if(!InitPipeline())
            return(false);
            
        BuildCommandBuffer(pipeline,material_instance,render_obj);

        return(true);
    }

    void Resize(int w,int h)override
    {
        cam.width=w;
        cam.height=h;

        cam.Refresh();

        ubo_mvp->Write(&cam.matrix);

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
