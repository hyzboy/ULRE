// 9.延迟渲染
//  简单的延迟渲染测试，仅一个太阳光

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/SceneDB.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKImageView.h>
#include<hgl/graph/vulkan/VKSampler.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *LoadTGATexture(const OSString &filename,Device *device);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

using Texture2DPointer=vulkan::Texture2D *;

constexpr VkFormat position_candidate_format[]={FMT_RGBA32F,FMT_RGBA16F};
constexpr VkFormat color_candidate_format   []={FMT_RGBA32F,
                                                FMT_RGBA16F,
                                                FMT_RGBA8UN,FMT_RGBA8SN,FMT_RGBA8U,
                                                FMT_BGRA8UN,FMT_BGRA8SN,FMT_BGRA8U,
                                                FMT_ABGR8UN,FMT_ABGR8SN,FMT_ABGR8U,
                                                FMT_RGB565,FMT_BGR565};
constexpr VkFormat normal_candidate_format  []={FMT_RGBA32F,
                                                FMT_RGBA16F,
                                                FMT_A2RGB10UN,FMT_A2RGB10SN,FMT_A2BGR10UN,
                                                FMT_A2BGR10SN};
constexpr VkFormat depth_candidate_format   []={FMT_D32F,FMT_D32F_S8U,FMT_X8_D24UN,FMT_D24UN_S8U,FMT_D16UN,FMT_D16UN_S8U};

class TestApp:public CameraAppFramework
{
private:

    SceneNode   render_root;
    RenderList  render_list;

    struct DeferredGBuffer
    {
        vulkan::Semaphore *render_complete_semaphore   =nullptr;

        vulkan::RenderTarget *rt;

        VkExtent2D extent;
        vulkan::Framebuffer *framebuffer;
        vulkan::RenderPass *renderpass;

        union
        {
            struct
            {
                Texture2DPointer position,normal,color,depth;
            };

            Texture2DPointer texture_list[4];
        };

        List<VkFormat> gbuffer_format_list;
        List<vulkan::ImageView *> image_view_list;

        struct
        {
            List<VkAttachmentDescription> desc_list;
            List<VkAttachmentReference> ref_list;
        }attachment;
        
        struct
        {
            List<VkSubpassDescription> desc;
            List<VkSubpassDependency> dependency;
        }subpass;
    }gbuffer;//

    struct SubpassParam
    {
        vulkan::Material *      material;
        vulkan::DescriptorSets *desc_sets;
        vulkan::Pipeline *      pipeline_fan;
        vulkan::Pipeline *      pipeline_triangles;
    };//

    SubpassParam                sp_gbuffer;
    SubpassParam                sp_composition;

    vulkan::Renderable          *ro_plane,
                                *ro_cube,
                                *ro_sphere,
                                *ro_torus,
                                *ro_cylinder,
                                *ro_cone,
                                
                                *ro_gbc_plane;

    vulkan::Sampler *           sampler=nullptr;

    struct
    {
        Texture2DPointer        color=nullptr;
        Texture2DPointer        normal=nullptr;
//        Texture2DPointer        specular=nullptr;
    }texture;

    vulkan::CommandBuffer       *gbuffer_cmd=nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(gbuffer_cmd);
        //SAFE_CLEAR(texture.specular);
        SAFE_CLEAR(texture.normal);
        SAFE_CLEAR(texture.color);
        SAFE_CLEAR(sampler);
    }
private:

    const VkFormat GetCandidateFormat(const VkFormat *fmt_list,const uint count)
    {
        auto pd=device->GetPhysicalDevice();

        for(uint i=0;i<count;i++)
            if(pd->IsColorAttachmentOptimal(fmt_list[i]))
                return fmt_list[i];

        for(uint i=0;i<count;i++)
            if(pd->IsColorAttachmentLinear(fmt_list[i]))
                return fmt_list[i];

        return FMT_UNDEFINED;
    }

    const VkFormat GetDepthCandidateFormat()
    {
        auto pd=device->GetPhysicalDevice();

        for(VkFormat fmt:depth_candidate_format)
            if(pd->IsDepthAttachmentOptimal(fmt))
                return fmt;

        for(VkFormat fmt:depth_candidate_format)
            if(pd->IsDepthAttachmentLinear(fmt))
                return fmt;

        return FMT_UNDEFINED;
    }

    bool InitGBuffer()
    {
        gbuffer.extent.width   =1024;
        gbuffer.extent.height  =1024;

        gbuffer.render_complete_semaphore   =device->CreateSem();

        //根据候选格式表选择格式
        //const VkFormat position_format  =GetCandidateFormat(position_candidate_format,  sizeof(position_candidate_format));
        //const VkFormat color_format     =GetCandidateFormat(color_candidate_format,     sizeof(color_candidate_format));
        //const VkFormat normal_format    =GetCandidateFormat(normal_candidate_format,    sizeof(normal_candidate_format));
        //const VkFormat depth_format     =GetDepthCandidateFormat();

        //if(position_format  ==FMT_UNDEFINED
        // ||color_format     ==FMT_UNDEFINED
        // ||normal_format    ==FMT_UNDEFINED
        // ||depth_format     ==FMT_UNDEFINED)
        //    return(false);

        const VkFormat position_format  =FMT_RGBA32F;
        const VkFormat color_format     =FMT_RGBA32F;
        const VkFormat normal_format    =FMT_RGBA32F;
        const VkFormat depth_format     =FMT_D32F;

        gbuffer.position=device->CreateAttachmentTextureColor(position_format,  gbuffer.extent.width,gbuffer.extent.height);
        gbuffer.color   =device->CreateAttachmentTextureColor(color_format,     gbuffer.extent.width,gbuffer.extent.height);
        gbuffer.normal  =device->CreateAttachmentTextureColor(normal_format,    gbuffer.extent.width,gbuffer.extent.height);
        gbuffer.depth   =device->CreateAttachmentTextureDepth(depth_format,     gbuffer.extent.width,gbuffer.extent.height);

        for(uint i=0;i<3;i++)
        {
            gbuffer.gbuffer_format_list.Add(gbuffer.texture_list[i]->GetFormat());
            gbuffer.image_view_list.Add(gbuffer.texture_list[i]->GetImageView());
        }

        if(!device->CreateAttachment(   gbuffer.attachment.ref_list,
                                        gbuffer.attachment.desc_list,
                                        gbuffer.gbuffer_format_list,
                                        gbuffer.depth->GetFormat()))
            return(false);

        VkSubpassDescription desc;

        device->CreateSubpassDescription(desc,gbuffer.attachment.ref_list);

        gbuffer.subpass.desc.Add(desc);

        device->CreateSubpassDependency(gbuffer.subpass.dependency,2);          //为啥要2个还不清楚

        gbuffer.renderpass=device->CreateRenderPass(gbuffer.attachment.desc_list,
                                                    gbuffer.subpass.desc,
                                                    gbuffer.subpass.dependency,
                                                    gbuffer.gbuffer_format_list,
                                                    gbuffer.depth->GetFormat());

        if(!gbuffer.renderpass)
            return(false);

        gbuffer.framebuffer=vulkan::CreateFramebuffer(device,gbuffer.renderpass,gbuffer.image_view_list,gbuffer.depth->GetImageView());

        if(!gbuffer.framebuffer)
            return(false);

        gbuffer.rt=device->CreateRenderTarget(gbuffer.framebuffer);

        return(true);
    }

    bool InitSubpass(SubpassParam *sp,const OSString &vs,const OSString &fs)
    {
        sp->material=shader_manage->CreateMaterial(vs,fs);

        if(!sp->material)
            return(false);

        sp->desc_sets=sp->material->CreateDescriptorSets();

        db->Add(sp->material);
        db->Add(sp->desc_sets);
        return(true);
    }

    bool InitGBufferPipeline(SubpassParam *sp)
    {
        AutoDelete<vulkan::PipelineCreater> pipeline_creater=new vulkan::PipelineCreater(device,sp->material,gbuffer.rt);

        {
            pipeline_creater->Set(PRIM_TRIANGLES);

            sp->pipeline_triangles=pipeline_creater->Create();
        
            if(!sp->pipeline_triangles)
                return(false);

            db->Add(sp->pipeline_triangles);
        }

        {
            pipeline_creater->Set(PRIM_TRIANGLE_FAN);

            sp->pipeline_fan=pipeline_creater->Create();
        
            if(!sp->pipeline_fan)
                return(false);

            db->Add(sp->pipeline_fan);
        }
        return(true);
    }

    bool InitCompositionPipeline(SubpassParam *sp)
    {
        AutoDelete<vulkan::PipelineCreater> pipeline_creater=new vulkan::PipelineCreater(device,sp->material,sc_render_target);
        pipeline_creater->SetDepthTest(false);
        pipeline_creater->SetDepthWrite(false);
        pipeline_creater->SetCullMode(VK_CULL_MODE_NONE);
        pipeline_creater->Set(PRIM_TRIANGLE_FAN);

        sp->pipeline_triangles=pipeline_creater->Create();

        if(!sp->pipeline_triangles)
            return(false);

        db->Add(sp->pipeline_triangles);
        return(true);
    }

    bool InitMaterial()
    {
        if(!InitSubpass(&sp_gbuffer,    OS_TEXT("res/shader/gbuffer_opaque.vert.spv"),OS_TEXT("res/shader/gbuffer_opaque.frag.spv")))return(false);
        if(!InitSubpass(&sp_composition,OS_TEXT("res/shader/gbuffer_composition.vert.spv"),OS_TEXT("res/shader/gbuffer_composition.frag.spv")))return(false);

        if(!InitGBufferPipeline(&sp_gbuffer))return(false);
        if(!InitCompositionPipeline(&sp_composition))return(false);

        texture.color   =vulkan::LoadTGATexture(OS_TEXT("res/image/cardboardPlainStain.tga"),device);
        texture.normal  =vulkan::LoadTGATexture(OS_TEXT("res/image/APOCWALL029_NRM.tga"),device);
        //texture.specular=vulkan::LoadTGATexture(OS_TEXT("res/image/APOCWALL029_SPEC.tga"),device);

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

        InitCameraUBO(sp_gbuffer.desc_sets,sp_gbuffer.material->GetUBO("world"));

        sp_gbuffer.desc_sets->BindSampler(sp_gbuffer.material->GetSampler("TextureColor"    ),texture.color,    sampler);
        sp_gbuffer.desc_sets->BindSampler(sp_gbuffer.material->GetSampler("TextureNormal"   ),texture.normal,   sampler);
        sp_gbuffer.desc_sets->Update();

        sp_composition.desc_sets->BindSampler(sp_composition.material->GetSampler("GB_Position" ),gbuffer.position, sampler);
        sp_composition.desc_sets->BindSampler(sp_composition.material->GetSampler("GB_Normal"   ),gbuffer.normal,   sampler);
        sp_composition.desc_sets->BindSampler(sp_composition.material->GetSampler("GB_Color"    ),gbuffer.color,    sampler);
        sp_composition.desc_sets->Update();

        return(true);
    }

    void CreateRenderObject(vulkan::Material *mtl)
    {
        {
            struct PlaneCreateInfo pci;

            ro_plane=CreateRenderablePlane(db,mtl,&pci);
        }
        
        {
            struct CubeCreateInfo cci;            
            ro_cube=CreateRenderableCube(db,mtl,&cci);
        }
        
        {        
            ro_sphere=CreateRenderableSphere(db,mtl,64);
        }

        {
            TorusCreateInfo tci;

            tci.innerRadius=50;
            tci.outerRadius=70;

            tci.numberSlices=32;
            tci.numberStacks=16;

            ro_torus=CreateRenderableTorus(db,mtl,&tci);
        }

        {
            CylinderCreateInfo cci;

            cci.halfExtend=10;
            cci.radius=10;
            cci.numberSlices=16;

            ro_cylinder=CreateRenderableCylinder(db,mtl,&cci);
        }

        {
            ConeCreateInfo cci;

            cci.halfExtend=10;
            cci.radius=10;
            cci.numberSlices=16;
            cci.numberStacks=1;

            ro_cone=CreateRenderableCone(db,mtl,&cci);
        }
    }

    bool InitCompositionRenderable()
    {
        ro_gbc_plane=CreateRenderableGBufferComposition(db,sp_composition.material);

        return ro_gbc_plane;
    }
    
    bool InitScene(SubpassParam *sp)
    {
        CreateRenderObject(sp->material);
        render_root.Add(db->CreateRenderableInstance(sp->pipeline_fan,sp->desc_sets,ro_plane),scale(100,100,1));
        render_root.Add(db->CreateRenderableInstance(sp->pipeline_triangles,sp->desc_sets,ro_torus    ),translate(0,0,0));
        render_root.Add(db->CreateRenderableInstance(sp->pipeline_triangles,sp->desc_sets,ro_sphere   ),scale(20,20,20));
        render_root.Add(db->CreateRenderableInstance(sp->pipeline_triangles,sp->desc_sets,ro_cube     ),translate(-30,  0,10)*scale(10,10,10));        
        render_root.Add(db->CreateRenderableInstance(sp->pipeline_triangles,sp->desc_sets,ro_cylinder ),translate( 30, 30,10)*scale(1,1,2));
        render_root.Add(db->CreateRenderableInstance(sp->pipeline_triangles,sp->desc_sets,ro_cone     ),translate(  0,-30, 0)*scale(1,1,2));

        render_root.RefreshMatrix();
        render_root.ExpendToList(&render_list);

        return(true);
    }

    bool InitGBufferCommandBuffer()
    {
        gbuffer_cmd=device->CreateCommandBuffer(gbuffer.extent,gbuffer.attachment.desc_list.GetCount());

        if(!gbuffer_cmd)
            return(false);

        gbuffer_cmd->Begin();
            if(!gbuffer_cmd->BeginRenderPass(gbuffer.rt))
                return(false);

            render_list.Render(gbuffer_cmd);

            gbuffer_cmd->EndRenderPass();
        gbuffer_cmd->End();

        return(true);
    }

public:

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

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
    
    virtual void SubmitDraw(int index) override
    {   
        gbuffer.rt->Submit(*gbuffer_cmd,present_complete_semaphore,gbuffer.render_complete_semaphore);

        VkCommandBuffer cb=*cmd_buf[index];
        
        sc_render_target->Submit(cb,gbuffer.render_complete_semaphore,render_complete_semaphore);
        sc_render_target->PresentBackbuffer(render_complete_semaphore);
        sc_render_target->Wait();
        gbuffer.rt->Wait();
    }
    
    void BuildCommandBuffer(uint32_t index) override
    {    
        VulkanApplicationFramework::BuildCommandBuffer( index,
                                                        sp_composition.pipeline_triangles,
                                                        sp_composition.desc_sets,
                                                        ro_gbc_plane);
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
