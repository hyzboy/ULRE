// 3.HQFilterTexture
// 测试高质量纹理过滤函数

#include"VulkanAppFramework.h"
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

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

class TestApp:public CameraAppFramework
{
private:

        PipelineData *      pipeline_data       =nullptr;
        Renderable *        render_obj          =nullptr;
        Sampler *           sampler_linear      =nullptr;
        Sampler *           sampler_nearest     =nullptr;

    struct MP
    {
        Material *          material            =nullptr;
        Pipeline *          pipeline            =nullptr;
    }mp_normal,mp_hq;

    struct MIR
    {
        MaterialInstance *  material_instance   =nullptr;
        RenderableInstance *renderable_instance =nullptr;
    }mir_nearest,mir_linear,mir_nearest_hq,mir_linear_hq;
    
    Texture2D *         texture             =nullptr;

    VAB *               vertex_buffer       =nullptr;
    VAB *               tex_coord_buffer    =nullptr;
    IndexBuffer *       index_buffer        =nullptr;
    
    SceneNode           render_root;
    RenderList          *render_list        =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(render_list);
    }

private:

    bool InitVBO()
    {
        vertex_buffer   =db->CreateVAB(VF_VEC2,VERTEX_COUNT,vertex_data);
        if(!vertex_buffer)return(false);

        tex_coord_buffer=db->CreateVAB(VF_VEC2,VERTEX_COUNT,tex_coord_data);
        if(!tex_coord_buffer)return(false);

        index_buffer    =db->CreateIBO16(INDEX_COUNT,index_data);
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

        render_list=new RenderList(device);

        return(true);
    }

    Sampler *InitSampler(VkFilter filter)
    {
        SamplerCreateInfo sampler_create_info;

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

        return db->CreateSampler(&sampler_create_info);
    }

    bool InitTexture()
    {
        texture=db->LoadTexture2D(OS_TEXT("res/image/heightmap.Tex2D"));
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
        pipeline_data=GetPipelineData(InlinePipeline::Solid2D);
        if(!pipeline_data)return(false);

        if(!InitMaterial(&mp_normal,OS_TEXT("res/material/Texture2D")))return(false);
        if(!InitMaterial(&mp_hq,    OS_TEXT("res/material/Texture2DHQ")))return(false);

        return(true);
    }

    bool InitMIR(struct MIR *mir,Sampler *sampler,MP *mp)
    {
        mir->material_instance=db->CreateMaterialInstance(mp->material);
        if(!mir->material_instance)return(false);
        
        {
            MaterialParameters *mp_global=mir->material_instance->GetMP(DescriptorSetsType::Global);
        
            if(!mp_global)
                return(false);

            if(!mp_global->BindUBO("g_camera",GetCameraInfoBuffer()))return(false);

            mp_global->Update();
        }

        {
            MaterialParameters *mp_texture=mir->material_instance->GetMP(DescriptorSetsType::Value);
        
            if(!mp_texture)
                return(false);
            
            if(!mp_texture->BindSampler("tex",texture,sampler))return(false);

            mp_texture->Update();
        }

        mir->renderable_instance=db->CreateRenderableInstance(render_obj,mir->material_instance,mp->pipeline);

        if(!mir->renderable_instance)
            return(false);

        return(true);
    }

    bool Add(struct MIR *mir,const Matrix4f &offset)
    {
        render_root.CreateSubNode(offset,mir->renderable_instance);

        return(true);
    }

    bool InitScene()
    {
        Add(&mir_nearest,   translate(-1,-1,0));
        Add(&mir_linear,    translate( 0,-1,0));
        Add(&mir_nearest_hq,translate(-1, 0,0));
        Add(&mir_linear_hq, translate( 0, 0,0));
        
        return(true);
    }

public:

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_SIZE,SCREEN_SIZE))
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
        VulkanApplicationFramework::BuildCommandBuffer(render_list);
    }

    void BuildCommandBuffer(uint32_t index) override
    {
        render_root.RefreshMatrix();
        render_list->Expend(camera->info,&render_root);
        
        VulkanApplicationFramework::BuildCommandBuffer(index,render_list);
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
#ifdef _DEBUG
    if(!CheckStrideBytesByFormat())
        return 0xff;
#endif//

    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
