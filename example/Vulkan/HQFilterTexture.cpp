// 3.HQFilterTexture
// 测试高质量纹理过滤函数

#include"VulkanAppFramework.h"
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTextureFromFile(RenderDevice *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_SIZE=512;

constexpr uint32_t VERTEX_COUNT=4;

constexpr float vertex_data[VERTEX_COUNT][2]=
{
    {0,  0},
    {1,  0},
    {0,  1},
    {1,  1}
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

        vulkan::PipelineData *      pipeline_data       =nullptr;
        vulkan::Renderable *        render_obj          =nullptr;
        vulkan::Sampler *           sampler_linear      =nullptr;
        vulkan::Sampler *           sampler_nearest     =nullptr;

    struct MP
    {
        vulkan::Material *          material            =nullptr;
        vulkan::Pipeline *          pipeline            =nullptr;
    }mp_normal,mp_hq;

    struct MIR
    {
        vulkan::MaterialInstance *  material_instance   =nullptr;
        vulkan::RenderableInstance *renderable_instance =nullptr;
    }mir_nearest,mir_linear,mir_nearest_hq,mir_linear_hq;
    
    vulkan::Texture2D *         texture             =nullptr;

    vulkan::VAB *               vertex_buffer       =nullptr;
    vulkan::VAB *               tex_coord_buffer    =nullptr;
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
        vertex_buffer   =device->CreateVAB(VAF_VEC2,VERTEX_COUNT,vertex_data);
        if(!vertex_buffer)return(false);

        tex_coord_buffer=device->CreateVAB(VAF_VEC2,VERTEX_COUNT,tex_coord_data);
        if(!tex_coord_buffer)return(false);

        index_buffer    =device->CreateIBO16(INDEX_COUNT,index_data);
        if(!index_buffer)return(false);

        return(true);
    }

    bool InitRenderObject()
    {
        render_obj=db->CreateRenderable(VERTEX_COUNT);
        if(!render_obj)return(false);

        render_obj->Set(VAN::Position,vertex_buffer);
        render_obj->Set(VAN::TexCoord,tex_coord_buffer);
        render_obj->Set(index_buffer);

        return(true);
    }

    vulkan::Sampler *InitSampler(VkFilter filter)
    {
        vulkan::SamplerCreateInfo sampler_create_info;

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

    bool InitMaterial(MP *mp,const OSString &mtl_name)
    {
        mp->material=db->CreateMaterial(mtl_name);
        if(!mp->material)return(false);

        mp->pipeline=CreatePipeline(mp->material,pipeline_data);
        if(!mp->pipeline)return(false);

        return(true);
    }

    bool InitMaterial()
    {
        pipeline_data=vulkan::GetPipelineData(vulkan::InlinePipeline::Solid2D);
        if(!pipeline_data)return(false);

        if(!InitMaterial(&mp_normal,OS_TEXT("res/material/Texture2DPC")))return(false);
        if(!InitMaterial(&mp_hq,    OS_TEXT("res/material/Texture2DHQPC")))return(false);

        return(true);
    }

    bool InitMIR(struct MIR *mir,vulkan::Sampler *sampler,MP *mp)
    {
        mir->material_instance=db->CreateMaterialInstance(mp->material);
        if(!mir->material_instance)return(false);
            
        if(!mir->material_instance->BindSampler("tex",texture,sampler))return(false);
        mir->material_instance->Update();

        mir->renderable_instance=db->CreateRenderableInstance(render_obj,mir->material_instance,mp->pipeline);

        if(!mir->renderable_instance)
            return(false);

        return(true);
    }

    bool Add(struct MIR *mir,const Matrix4f &offset)
    {
        render_root.Add(mir->renderable_instance,offset);

        return(true);
    }

    bool InitScene()
    {
        Add(&mir_nearest,   translate(-1,-1,0));
        Add(&mir_linear,    translate( 0,-1,0));
        Add(&mir_nearest_hq,translate(-1, 0,0));
        Add(&mir_linear_hq, translate( 0, 0,0));
        
        render_root.RefreshMatrix();
        render_root.ExpendToList(&render_list);
        BuildCommandBuffer(&render_list);
        return(true);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_SIZE,SCREEN_SIZE))
            return(false);

        if(!InitVBO())
            return(false);

        if(!InitRenderObject())
            return(false);

        if(!InitTexture())
            return(false);

        if(!InitMaterial())
            return(false);

        sampler_nearest=InitSampler(VK_FILTER_NEAREST);
        sampler_linear=InitSampler(VK_FILTER_LINEAR);

        if(!InitMIR(&mir_nearest,      sampler_nearest ,&mp_normal  ))return(false);
        if(!InitMIR(&mir_linear,       sampler_linear  ,&mp_normal  ))return(false);
        if(!InitMIR(&mir_nearest_hq,   sampler_nearest ,&mp_hq      ))return(false);
        if(!InitMIR(&mir_linear_hq,    sampler_linear  ,&mp_hq      ))return(false);
        
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
