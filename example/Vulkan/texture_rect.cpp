// 2.texture_rect
// 该示例是1.indices_rect的进化，演示在矩形上贴上贴图

#include"VulkanAppFramework.h"
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *LoadTGATexture(const OSString &filename,Device *device);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

struct WorldConfig
{
    Matrix4f mvp;
}world;

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
private:

    vulkan::Material *          material            =nullptr;
    vulkan::Texture2D *         texture             =nullptr;
    vulkan::Sampler *           sampler             =nullptr;
    vulkan::DescriptorSets *    descriptor_sets     =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::Buffer *            ubo_mvp             =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;

    vulkan::VertexBuffer *      vertex_buffer       =nullptr;
    vulkan::VertexBuffer *      tex_coord_buffer    =nullptr;
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
        SAFE_CLEAR(descriptor_sets);
        SAFE_CLEAR(sampler);
        SAFE_CLEAR(texture);
        SAFE_CLEAR(material);
    }

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("FlatTexture.vert.spv"),
                                               OS_TEXT("FlatTexture.frag.spv"));
        if(!material)
            return(false);

        render_obj=material->CreateRenderable(VERTEX_COUNT);
        descriptor_sets=material->CreateDescriptorSets();

        texture=vulkan::LoadTGATexture(OS_TEXT("lena.tga"),device);

        VkSamplerCreateInfo sampler_create_info;

        sampler_create_info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_create_info.pNext                   = nullptr;
        sampler_create_info.flags                   = 0;
        sampler_create_info.magFilter               = VK_FILTER_LINEAR;
        sampler_create_info.minFilter               = VK_FILTER_LINEAR;
        sampler_create_info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_create_info.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_create_info.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_create_info.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_create_info.mipLodBias              = 0.0f;
        sampler_create_info.anisotropyEnable        = false;
        sampler_create_info.maxAnisotropy           = 0;
        sampler_create_info.compareEnable           = false;
        sampler_create_info.compareOp               = VK_COMPARE_OP_NEVER;
        sampler_create_info.minLod                  = 0.0f;
        sampler_create_info.maxLod                  = 1.0f;
        sampler_create_info.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        sampler_create_info.unnormalizedCoordinates = false;

        sampler=device->CreateSampler(&sampler_create_info);

        descriptor_sets->BindSampler(material->GetSampler("texture_lena"),texture,sampler);
        descriptor_sets->BindUBO(material->GetUBO("world"),*ubo_mvp);
        descriptor_sets->Update();

        return(true);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=device->GetExtent();

        world.mvp=ortho(extent.width,extent.height);

        ubo_mvp=device->CreateUBO(sizeof(WorldConfig),&world);

        return ubo_mvp;
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
        SharedPtr<vulkan::PipelineCreater> 
        pipeline_creater=new vulkan::PipelineCreater(device,material,device->GetMainRenderPass(),device->GetExtent());
        pipeline_creater->SetDepthTest(false);
        pipeline_creater->SetDepthWrite(false);
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
