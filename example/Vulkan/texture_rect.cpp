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

constexpr uint32_t SCREEN_WIDTH=256;
constexpr uint32_t SCREEN_HEIGHT=256;

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

    uint swap_chain_count=0;

    vulkan::Material *          material            =nullptr;
    vulkan::Texture2D *         texture             =nullptr;
    vulkan::Sampler *           sampler             =nullptr;
    vulkan::DescriptorSets *    desciptor_sets      =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::Buffer *            ubo_mvp             =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;
    vulkan::CommandBuffer **    cmd_buf             =nullptr;

    vulkan::VertexBuffer *      vertex_buffer       =nullptr;
    vulkan::VertexBuffer *      tex_coord_buffer    =nullptr;
    vulkan::IndexBuffer *       index_buffer        =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(index_buffer);
        SAFE_CLEAR(tex_coord_buffer);
        SAFE_CLEAR(vertex_buffer);
        SAFE_CLEAR_OBJECT_ARRAY(cmd_buf,swap_chain_count);
        SAFE_CLEAR(pipeline);
        SAFE_CLEAR(ubo_mvp);
        SAFE_CLEAR(render_obj);
        SAFE_CLEAR(desciptor_sets);
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

        render_obj=material->CreateRenderable();
        desciptor_sets=material->CreateDescriptorSets();

        texture=vulkan::LoadTGATexture(OS_TEXT("lena.tga"),device);

        VkSamplerCreateInfo sampler_create_info{};

        sampler_create_info.magFilter   = VK_FILTER_LINEAR;
        sampler_create_info.minFilter   = VK_FILTER_LINEAR;
        sampler_create_info.mipmapMode  = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_create_info.addressModeU= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_create_info.addressModeV= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_create_info.addressModeW= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_create_info.mipLodBias  = 0.0f;
        sampler_create_info.compareOp   = VK_COMPARE_OP_NEVER;
        sampler_create_info.minLod      = 0.0f;
        sampler_create_info.maxLod      = 1.0f;

        sampler=device->CreateSampler(&sampler_create_info);

        VkDescriptorImageInfo image_info;
        image_info.imageView    =*texture;
        image_info.imageLayout  =*texture;
        image_info.sampler      =*sampler;

        desciptor_sets->UpdateSampler(material->GetCombindImageSampler("texture_lena"),&image_info);
        return(true);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=device->GetExtent();

        world.mvp=ortho(extent.width,extent.height);

        ubo_mvp=device->CreateUBO(sizeof(WorldConfig),&world);

        if(!ubo_mvp)
            return(false);

        return desciptor_sets->UpdateUBO(material->GetUBO("world"),*ubo_mvp);
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
        vulkan::PipelineCreater *
            pipeline_creater=new vulkan::PipelineCreater(device,material,device->GetRenderPass(),device->GetExtent());
        pipeline_creater->SetDepthTest(false);
        pipeline_creater->SetDepthWrite(false);
        pipeline_creater->CloseCullFace();
        pipeline_creater->Set(PRIM_TRIANGLES);

        pipeline=pipeline_creater->Create();

        delete pipeline_creater;
        pipeline_creater=nullptr;

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
            cmd_buf[i]->DrawIndexed(INDEX_COUNT);
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
