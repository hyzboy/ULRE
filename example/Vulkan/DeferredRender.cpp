﻿// 简单传统延迟渲染，使用多个RenderPass，多个CommandBuffer

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKImageView.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/Time.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTexture2DFromFile(GPUDevice *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=SCREEN_WIDTH/16*9;

using Texture2DPointer=Texture2D *;

enum class GBufferAttachment
{
    Color=0,
    Normal,

    ENUM_CLASS_RANGE(Color,Normal)
};//

constexpr VkFormat gbuffer_color_format[size_t(GBufferAttachment::RANGE_SIZE)]={UPF_RGB565,UPF_RG8};
constexpr VkFormat gbuffer_depth_format=PF_D16UN;

struct alignas(16) PhongPointLight
{
    Vector4f color;
    Vector4f position;
    float radius;
};//

class TestApp:public CameraAppFramework
{
private:

    SceneNode   render_root;
    RenderList *render_list=nullptr;

    struct
    {
        RenderTarget *rt=nullptr;
        RenderPass *rp=nullptr;
        RenderCmdBuffer *cmd=nullptr;

        Sampler *sampler=nullptr;

    public:

        bool Submit(Semaphore *sem)
        {
            return rt->Submit(cmd,sem);
        }
    }gbuffer;

    PhongPointLight lights;

    DeviceBuffer *ubo_lights;

    struct SubpassParam
    {
        Material *          material;
        MaterialInstance *  material_instance;
        Pipeline *          pipeline_fan;
        Pipeline *          pipeline_triangles;
    };//

    SubpassParam            sp_gbuffer;
    SubpassParam            sp_composition;

    Primitive               *ro_plane,
                            *ro_cube,
                            *ro_sphere,
                            *ro_torus,
                            *ro_cylinder,
                            *ro_cone,                                
                            *ro_gbc_plane,
                            *ro_axis;

    Renderable      *ro_gbc_plane_ri;

    struct
    {
        Texture2DPointer    color=nullptr;
        Texture2DPointer    normal=nullptr;

        Sampler *           color_sampler=nullptr;
        Sampler *           normal_sampler=nullptr;
    }texture;

public:

    ~TestApp()
    {
        SAFE_CLEAR(render_list);
        SAFE_CLEAR(gbuffer.cmd);
        SAFE_CLEAR(gbuffer.rt);
    }

private:

    void CreateGBufferSampler()
    {    
        VkSamplerCreateInfo sci=
        {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            nullptr,
            0,
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_MIPMAP_MODE_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            0.0f,
            false,
            1.0f,
            false,
            VK_COMPARE_OP_NEVER,
            0.0f,
            0.0f,
            VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            false
        };

        gbuffer.sampler=db->CreateSampler(&sci);
        
    #ifdef _DEBUG
        {
            auto da=device->GetDeviceAttribute();

            if(da->debug_maker)
            {
                da->debug_maker->SetSampler(*(gbuffer.sampler), "[debug maker] GBuffer_Sampler");
            }

            if(da->debug_utils)
            {
                da->debug_utils->SetSampler(*(gbuffer.sampler), "[debug utils] GBuffer_Sampler");
            }
        }
    #endif//_DEBUG
    }

    bool InitGBuffer()
    {
        FramebufferInfo fbi(gbuffer_color_format,size_t(GBufferAttachment::RANGE_SIZE),gbuffer_depth_format);

        fbi.SetExtent(SCREEN_WIDTH,SCREEN_HEIGHT);

        gbuffer.rt=device->CreateRT(&fbi);

        if(!gbuffer.rt)return(false);

        gbuffer.cmd=device->CreateRenderCommandBuffer();

        gbuffer.rp=gbuffer.rt->GetRenderPass();

        CreateGBufferSampler();

    #ifdef _DEBUG
        {
            auto da=device->GetDeviceAttribute();

            VkQueue           q=*(gbuffer.rt->GetQueue());
            VkFramebuffer   fbo=  gbuffer.rt->GetFramebuffer()->GetFramebuffer();
            VkRenderPass     rp=  gbuffer.rp->GetVkRenderPass();
            VkSemaphore     sem=*(gbuffer.rt->GetRenderCompleteSemaphore());
            VkCommandBuffer  cb=*(gbuffer.cmd);
            VkSampler         s=*(gbuffer.sampler);

            if(da->debug_maker)
            {
                da->debug_maker->SetQueue(          q,  "[debug maker] GBufferQueue");
                da->debug_maker->SetFramebuffer(    fbo,"[debug maker] GBufferFBO");
                da->debug_maker->SetRenderPass(     rp, "[debug maker] GBufferRenderpass");
                da->debug_maker->SetCommandBuffer(  cb, "[debug maker] GBufferCommandBuffer");
                da->debug_maker->SetSampler(        s,  "[debug maker] GBufferSampler");
                da->debug_maker->SetSemaphore(      sem,"[debug maker] GBufferSemaphore");
            }

            if(da->debug_utils)
            {
                da->debug_utils->SetQueue(          q,  "[debug utils] GBufferQueue");
                da->debug_utils->SetFramebuffer(    fbo,"[debug utils] GBufferFBO");
                da->debug_utils->SetRenderPass(     rp, "[debug utils] GBufferRenderpass");
                da->debug_utils->SetCommandBuffer(  cb, "[debug utils] GBufferCommandBuffer");
                da->debug_utils->SetSampler(        s,  "[debug utils] GBufferSampler");
                da->debug_utils->SetSemaphore(      sem,"[debug utils] GBufferSemaphore");
            }
        }
    #endif//_DEBUG

        return(gbuffer.rt);
    }

    bool InitMaterial(SubpassParam *sp,const OSString &material_filename)
    {
        sp->material=db->CreateMaterial(material_filename);

        if(!sp->material)
            return(false);

        sp->material_instance=db->CreateMaterialInstance(sp->material);
        return(true);
    }

    bool InitGBufferPipeline(SubpassParam *sp)
    {
        sp->pipeline_triangles  =gbuffer.rp->CreatePipeline(sp->material_instance,InlinePipeline::Solid3D,Prim::Triangles);
        if(!sp->pipeline_triangles)
            return(false);

        sp->pipeline_fan        =gbuffer.rp->CreatePipeline(sp->material_instance,InlinePipeline::Solid3D,Prim::Fan);
        if(!sp->pipeline_fan)
            return(false);

    #ifdef _DEBUG
        {
            auto da=device->GetDeviceAttribute();

            if(da->debug_maker)
            {
                da->debug_maker->SetPipeline(*(sp->pipeline_triangles), "[debug maker] GBuffer_Pipeline_Triangles");
                da->debug_maker->SetPipeline(*(sp->pipeline_fan),       "[debug maker] GBuffer_Pipeline_Fan");
            }

            if(da->debug_utils)
            {
                da->debug_utils->SetPipeline(*(sp->pipeline_triangles), "[debug utils] GBuffer_Pipeline_Triangles");
                da->debug_utils->SetPipeline(*(sp->pipeline_fan),       "[debug utils] GBuffer_Pipeline_Fan");
            }
        }
    #endif//_DEBUG

        return(true);
    }

    bool InitCompositionPipeline(SubpassParam *sp)
    {
        sp->pipeline_fan=device_render_pass->CreatePipeline(sp->material_instance,InlinePipeline::Solid2D,Prim::Fan);

        return sp->pipeline_fan;
    }

    bool InitLightsUBO()
    {
        ubo_lights=db->CreateUBO(sizeof(lights),&lights);
        return ubo_lights;
    }

    Sampler *CreateSampler(Texture *tex)
    {
        VkSamplerCreateInfo sci=
        {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            nullptr,
            0,
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_MIPMAP_MODE_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            0.0f,
            false,
            1.0f,
            false,
            VK_COMPARE_OP_NEVER,
            0.0f,
            0.0f,
            VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            false
        };

        sci.maxLod=tex->GetMipLevel();

        return db->CreateSampler(&sci);
    }

    bool InitMaterial()
    {
        if(!InitLightsUBO())return(false);

        if(!InitMaterial(&sp_gbuffer,    OS_TEXT("res/material/opaque")))return(false);
        if(!InitMaterial(&sp_composition,OS_TEXT("res/material/composition")))return(false);

        if(!InitGBufferPipeline(&sp_gbuffer))return(false);
        if(!InitCompositionPipeline(&sp_composition))return(false);

        texture.color   =db->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"));
        if(!texture.color)return(false);
        texture.normal  =db->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"));
        if(!texture.normal)return(false);
        
        texture.color_sampler=CreateSampler(texture.color);
        texture.normal_sampler=CreateSampler(texture.normal);
      
        BindCameraUBO(sp_gbuffer.material_instance);

        {
            MaterialParameters *mp=sp_gbuffer.material_instance->GetMP(DescriptorSetType::Value);

            if(!mp)
                return(false);

            mp->BindImageSampler("TexColor"    ,texture.color,    texture.color_sampler);
            mp->BindImageSampler("TexNormal"   ,texture.normal,   texture.normal_sampler);
            mp->Update();
        }
        
        BindCameraUBO(sp_composition.material_instance);

        {            
            MaterialParameters *mp=sp_composition.material_instance->GetMP(DescriptorSetType::Value);
        
            if(!mp)
                return(false);

            mp->BindUBO("lights",ubo_lights);
            mp->BindImageSampler("GB_Color"    ,gbuffer.rt->GetColorTexture((uint)GBufferAttachment::Color),gbuffer.sampler);
            mp->BindImageSampler("GB_Normal"   ,gbuffer.rt->GetColorTexture((uint)GBufferAttachment::Normal),gbuffer.sampler);
            mp->BindImageSampler("GB_Depth"    ,gbuffer.rt->GetDepthTexture(),gbuffer.sampler);
            mp->Update();
        }

        return(true);
    }

    void CreateRenderObject(const VIL *vil)
    {
        using namespace inline_geometry;

        {
            struct PlaneCreateInfo pci;
            ro_plane=CreatePlane(db,vil,&pci);
        }

        {
            struct CubeCreateInfo cci;            
            ro_cube=CreateCube(db,vil,&cci);
        }

        {        
            ro_sphere=CreateSphere(db,vil,64);
        }

        {
            TorusCreateInfo tci;

            tci.innerRadius=50;
            tci.outerRadius=70;

            tci.numberSlices=128;
            tci.numberStacks=64;

            tci.uv_scale.x=4;
            tci.uv_scale.y=1;

            ro_torus=CreateTorus(db,vil,&tci);
        }

        {
            CylinderCreateInfo cci;

            cci.halfExtend=10;
            cci.radius=10;
            cci.numberSlices=32;

            ro_cylinder=CreateCylinder(db,vil,&cci);
        }

        {
            ConeCreateInfo cci;

            cci.halfExtend=10;
            cci.radius=10;
            cci.numberSlices=128;
            cci.numberStacks=32;

            ro_cone=CreateCone(db,vil,&cci);
        }
    }

    bool InitCompositionRenderable()
    {
        ro_gbc_plane=inline_geometry::CreateGBufferCompositionRectangle(db,sp_composition.material_instance->GetVIL());
        if(!ro_gbc_plane)return(false);

        ro_gbc_plane_ri=db->CreateRenderable(ro_gbc_plane,sp_composition.material_instance,sp_composition.pipeline_fan);
        if(!ro_gbc_plane_ri)return(false);

        return(true);
    }

    bool InitScene(SubpassParam *sp)
    {
        CreateRenderObject(sp->material_instance->GetVIL());
        render_root.CreateSubNode(                      scale(100,100,1),   db->CreateRenderable(ro_plane      ,sp->material_instance,sp->pipeline_fan      ));
        render_root.CreateSubNode(                                          db->CreateRenderable(ro_torus      ,sp->material_instance,sp->pipeline_triangles));
        render_root.CreateSubNode(                      scale(20,20,20),    db->CreateRenderable(ro_sphere     ,sp->material_instance,sp->pipeline_triangles));
        render_root.CreateSubNode(translate(-30,  0,10)*scale(10,10,10),    db->CreateRenderable(ro_cube       ,sp->material_instance,sp->pipeline_triangles));
        render_root.CreateSubNode(translate( 30, 30,10)*scale(1,1,2),       db->CreateRenderable(ro_cylinder   ,sp->material_instance,sp->pipeline_triangles));
        render_root.CreateSubNode(translate(  0,-30, 0)*scale(1,1,2),       db->CreateRenderable(ro_cone       ,sp->material_instance,sp->pipeline_triangles));

        render_root.RefreshMatrix();
        render_list->Expend(GetCameraInfo(),&render_root);

        return(true);
    }

    bool InitGBufferCommandBuffer()
    {
        if(!gbuffer.cmd)
            return(false);

        gbuffer.cmd->Begin();
            if(!gbuffer.cmd->BindFramebuffer(gbuffer.rt->GetRenderPass(),gbuffer.rt->GetFramebuffer()))
                return(false);

            gbuffer.cmd->BeginRegion("GBuffer Begin",Color4f(1,0,0,1));
            if(!gbuffer.cmd->BeginRenderPass())
                return(false);

                render_list->Render(gbuffer.cmd);

            gbuffer.cmd->EndRenderPass();
            gbuffer.cmd->EndRegion();
        gbuffer.cmd->End();

        return(true);
    }

public:

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        render_list=new RenderList(device);

        if(!InitGBuffer())
            return(false);

        if(!InitMaterial())
            return(false);

        if(!InitScene(&sp_gbuffer))
            return(false);

        if(!InitGBufferCommandBuffer())
            return(false);

        if(!InitCompositionRenderable())
            return(false);

        return(true);
    }

    void UpdateLights()
    {
        const double timer=GetDoubleTime();
        
        // White
        lights.position = Vector4f(0.0f, 0.0f, 25.0f, 0.0f);
        lights.color = Vector4f(15.0f);
        lights.radius = 155.0f;

        lights.position.x = sin(rad2deg(timer/100)) * 100.0f;
        lights.position.y = cos(rad2deg(timer/100)) * 100.0f;

        ubo_lights->Write(&lights);
    }
    
    virtual void SubmitDraw(int index) override
    {
        gbuffer.Submit(sc_render_target->GetPresentCompleteSemaphore());

        VkCommandBuffer cb=*cmd_buf[index];
        
        sc_render_target->Submit(cb,gbuffer.rt->GetRenderCompleteSemaphore());
        sc_render_target->PresentBackbuffer();
        sc_render_target->WaitQueue();
        sc_render_target->WaitFence();

        gbuffer.rt->WaitQueue();
        gbuffer.rt->WaitFence();
    }
    
    void BuildCommandBuffer(uint32_t index) override
    {
        VulkanApplicationFramework::BuildCommandBuffer(index,ro_gbc_plane_ri);
    }
    
    void Draw()override
    {
        UpdateLights();
        
        render_root.RefreshMatrix();
        render_list->Expend(GetCameraInfo(),&render_root);
        
        CameraAppFramework::Draw();
    }
};//class TestApp:public CameraAppFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
