// 2.RectanglePrimivate
// 该示例是texture_rect的进化，演示使用GeometryShader画矩形

#include"VulkanAppFramework.h"
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTextureFromFile(Device *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_SIZE=512;

constexpr uint32_t VERTEX_COUNT=1;

constexpr float BORDER=0.1f;

constexpr float vertex_data[4]=
{
    SCREEN_SIZE*BORDER,         SCREEN_SIZE*BORDER,
    SCREEN_SIZE*(1.0-BORDER),   SCREEN_SIZE*(1.0-BORDER)
};

constexpr float tex_coord_data[4]=
{
    0,0,1,1
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
    vulkan::Buffer *            ubo_world_matrix             =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;

    vulkan::VertexAttribBuffer *      vertex_buffer       =nullptr;
    vulkan::VertexAttribBuffer *      tex_coord_buffer    =nullptr;

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial( OS_TEXT("res/shader/DrawRect2D.vert"),
                                                OS_TEXT("res/shader/DrawRect2D.geom"),
                                                OS_TEXT("res/shader/FlatTexture.frag"));
        if(!material)
            return(false);

        render_obj=material->CreateRenderable(VERTEX_COUNT);
        material_instance=material->CreateInstance();

        texture=vulkan::CreateTextureFromFile(device,OS_TEXT("res/image/lena.Tex2D"));

        sampler=db->CreateSampler();

        material_instance->BindSampler("tex",texture,sampler);
        material_instance->BindUBO("world",ubo_world_matrix);
        material_instance->Update();

        db->Add(material);
        db->Add(material_instance);
        db->Add(texture);
        db->Add(render_obj);
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

        return(true);
    }

    void InitVBO()
    {
        vertex_buffer   =db->CreateVBO(FMT_RGBA32F,VERTEX_COUNT,vertex_data);
        tex_coord_buffer=db->CreateVBO(FMT_RGBA32F,VERTEX_COUNT,tex_coord_data);

        render_obj->Set("Vertex",vertex_buffer);
        render_obj->Set("TexCoord",tex_coord_buffer);
    }

    bool InitPipeline()
    {
        AutoDelete<vulkan::PipelineCreater>
        pipeline_creater=new vulkan::PipelineCreater(device,material,sc_render_target);
        pipeline_creater->CloseCullFace();
        pipeline_creater->Set(PRIM_RECTANGLES);

        pipeline=pipeline_creater->Create();

        db->Add(pipeline);
        return pipeline;
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_SIZE,SCREEN_SIZE))
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
