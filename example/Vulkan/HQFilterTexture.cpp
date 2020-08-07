// 3.HQFilterTexture
// 测试高质量纹理过滤函数

#include"VulkanAppFramework.h"
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTextureFromFile(Device *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_WIDTH=512;
constexpr uint32_t SCREEN_HEIGHT=512;

constexpr uint32_t VERTEX_COUNT=4;

constexpr float vertex_data[VERTEX_COUNT][2]=
{
    {0,0},
    {1,0},
    {0,1},
    {1,1}
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

    struct MPD
    {
        vulkan::Material *          material            =nullptr;
        vulkan::Pipeline *          pipeline            =nullptr;
        vulkan::MaterialInstance *  material_instance   =nullptr;
        vulkan::Renderable *        render_obj          =nullptr;

    public:

        ~MPD()
        {
            delete material;
            delete render_obj;
            delete material_instance;
            delete pipeline;
        }
    }nearest,linear,nearest_hq,linear_hq;
    
    vulkan::Texture2D *         texture             =nullptr;

    vulkan::Sampler *           sampler_linear      =nullptr;
    vulkan::Sampler *           sampler_nearest     =nullptr;

    vulkan::VAB *      vertex_buffer       =nullptr;
    vulkan::VAB *      tex_coord_buffer    =nullptr;
    vulkan::IndexBuffer *       index_buffer        =nullptr;
    
            SceneNode           render_root;
            RenderList          render_list;
public:

    ~TestApp()
    {
        SAFE_CLEAR(index_buffer);
        SAFE_CLEAR(tex_coord_buffer);
        SAFE_CLEAR(vertex_buffer);
        SAFE_CLEAR(sampler_nearest);
        SAFE_CLEAR(sampler_linear);
        SAFE_CLEAR(texture);
    }

private:

    bool InitVBO()
    {
        vertex_buffer   =device->CreateVAB(FMT_RG32F,VERTEX_COUNT,vertex_data);
        if(!vertex_buffer)return(false);

        tex_coord_buffer=device->CreateVAB(FMT_RG32F,VERTEX_COUNT,tex_coord_data);
        if(!tex_coord_buffer)return(false);

        index_buffer    =device->CreateIBO16(INDEX_COUNT,index_data);
        if(!index_buffer)return(false);

        return(true);
    }

    vulkan::Sampler *InitSampler(VkFilter filter)
    {
        VkSamplerCreateInfo sampler_create_info;

        sampler_create_info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_create_info.pNext                   = nullptr;
        sampler_create_info.flags                   = 0;
        sampler_create_info.magFilter               = filter;
        sampler_create_info.minFilter               = filter;
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

        return device->CreateSampler(&sampler_create_info);
    }

    bool InitTexture()
    {
        texture=vulkan::CreateTextureFromFile(device,OS_TEXT("res/image/heightmap.Tex2D"));
        return texture;
    }

    bool InitMaterial(struct MPD *mpd,vulkan::Sampler *sampler,const OSString &fragment_shader)
    {
        mpd->material=shader_manage->CreateMaterial(OS_TEXT("res/shader/Texture2D.vert"),
                                                    OS_TEXT("res/shader/")+fragment_shader+OS_TEXT(".frag"));
        if(!mpd->material)
            return(false);

        mpd->material_instance=mpd->material->CreateInstance();

        mpd->material_instance->BindSampler("tex",texture,sampler);
        mpd->material_instance->Update();
        
        mpd->render_obj=mpd->material->CreateRenderable(VERTEX_COUNT);
        mpd->render_obj->Set("Vertex",vertex_buffer);
        mpd->render_obj->Set("TexCoord",tex_coord_buffer);
        mpd->render_obj->Set(index_buffer);
        
        AutoDelete<vulkan::PipelineCreater>
        pipeline_creater=new vulkan::PipelineCreater(device,mpd->material,sc_render_target);
        pipeline_creater->CloseCullFace();
        pipeline_creater->Set(PRIM_TRIANGLES);

        mpd->pipeline=pipeline_creater->Create();

        return mpd->pipeline;
    }

    bool Add(struct MPD *mpd,const Matrix4f &offset)
    {
        RenderableInstance *ri=db->CreateRenderableInstance(mpd->pipeline,mpd->material_instance,mpd->render_obj);

        if(!ri)return(false);

        render_root.Add(ri,offset);

        return(true);
    }

    bool InitScene()
    {
        Add(&nearest,   translate(-1,-1,0));
        Add(&linear,    translate( 0,-1,0));
        Add(&nearest_hq,translate(-1, 0,0));
        Add(&linear_hq, translate( 0, 0,0));
        
        render_root.RefreshMatrix();
        render_root.ExpendToList(&render_list);
        BuildCommandBuffer(&render_list);
        return(true);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitVBO())
            return(false);

        sampler_nearest=InitSampler(VK_FILTER_NEAREST);
        sampler_linear=InitSampler(VK_FILTER_LINEAR);

        if(!InitTexture())
            return(false);

        if(!InitMaterial(&nearest,      sampler_nearest ,OS_TEXT("FlatTexture")))return(false);
        if(!InitMaterial(&linear,       sampler_linear  ,OS_TEXT("FlatTexture")))return(false);
        if(!InitMaterial(&nearest_hq,   sampler_nearest ,OS_TEXT("hqfilter")))return(false);
        if(!InitMaterial(&linear_hq,    sampler_linear  ,OS_TEXT("hqfilter")))return(false);
        
        if(!InitScene())
            return(false);

        return(true);
    }

    void Resize(int w,int h) override
    {
        BuildCommandBuffer(&render_list);
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
