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
#include<hgl/graph/vulkan/VKFramebuffer.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *LoadTGATexture(const OSString &filename,Device *device);
VK_NAMESPACE_END

constexpr uint32_t GBUFFER_WIDTH=1024;
constexpr uint32_t GBUFFER_HEIGHT=1024;

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

using Texture2DPointer=vulkan::Texture2D *;

constexpr VkFormat position_candidate_format[]={FMT_RGBA32F,FMT_RGBA16F};
constexpr VkFormat color_candidate_format   []={FMT_RGBA32F,FMT_RGBA16F,FMT_RGB16UN,FMT_RGB16SN,FMT_RGBA8UN,FMT_RGBA8SN,FMT_RGBA8U,FMT_RGB565,FMT_BGR565};
constexpr VkFormat normal_candidate_format  []={FMT_RGBA32F,FMT_RGBA16F};
constexpr VkFormat depth_candidate_format   []={FMT_D32F,FMT_D32F_S8U,FMT_X8_D24,FMT_D24UN_S8U,FMT_D16UN,FMT_D16UN_S8U};

class TestApp:public CameraAppFramework
{
private:

    SceneNode   render_root;
    RenderList  render_list;

    struct DeferredGBuffer
    {
        uint32_t width,height;
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
        vulkan::Pipeline *      pipeline;
    };//

    SubpassParam                sp_gbuffer;
    SubpassParam                sp_composition;

    vulkan::Renderable *        ro_cube;

    vulkan::Sampler *           sampler;

    struct
    {
        vulkan::Texture2D       *color,*normal,*specular;
    }texture;

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
        gbuffer.width   =GBUFFER_WIDTH;
        gbuffer.height  =GBUFFER_HEIGHT;

        //根据候选格式表选择格式
        const VkFormat position_format  =GetCandidateFormat(position_candidate_format,  sizeof(position_candidate_format));
        const VkFormat color_format     =GetCandidateFormat(color_candidate_format,     sizeof(color_candidate_format));
        const VkFormat normal_format    =GetCandidateFormat(normal_candidate_format,    sizeof(normal_candidate_format));
        const VkFormat depth_format     =GetDepthCandidateFormat();

        if(position_format  ==FMT_UNDEFINED
         ||color_format     ==FMT_UNDEFINED
         ||normal_format    ==FMT_UNDEFINED
         ||depth_format     ==FMT_UNDEFINED)
            return(false);

        gbuffer.position=device->CreateTexture2DColor(position_format,  gbuffer.width,gbuffer.height);
        gbuffer.color   =device->CreateTexture2DColor(color_format,     gbuffer.width,gbuffer.height);
        gbuffer.normal  =device->CreateTexture2DColor(normal_format,    gbuffer.width,gbuffer.height);
        gbuffer.depth   =device->CreateTexture2DDepth(depth_format,     gbuffer.width,gbuffer.height);

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
        AutoDelete<vulkan::PipelineCreater> pipeline_creater=new vulkan::PipelineCreater(device,sp->material,gbuffer.renderpass,device->GetExtent());
        pipeline_creater->SetDepthTest(true);
        pipeline_creater->SetDepthWrite(true);
        pipeline_creater->SetCullMode(VK_CULL_MODE_BACK_BIT);
        pipeline_creater->Set(PRIM_TRIANGLES);

        sp->pipeline=pipeline_creater->Create();
        
        if(!sp->pipeline)
            return(false);

        db->Add(sp->pipeline);
        return(true);
    }

    bool InitCompositionPipeline(SubpassParam *sp)
    {
        AutoDelete<vulkan::PipelineCreater> pipeline_creater=new vulkan::PipelineCreater(device,sp->material,device->GetMainRenderPass(),device->GetExtent());
        pipeline_creater->SetDepthTest(false);
        pipeline_creater->SetDepthWrite(false);
        pipeline_creater->SetCullMode(VK_CULL_MODE_NONE);
        pipeline_creater->Set(PRIM_TRIANGLES);
        sp->pipeline=pipeline_creater->Create();

        if(!sp->pipeline)
            return(false);

        db->Add(sp->pipeline);
        return(true);
    }

    bool InitMaterial()
    {
        if(!InitSubpass(&sp_gbuffer,    OS_TEXT("gbuffer_opaque.vert.spv"),OS_TEXT("gbuffer_opaque.frag.spv")))return(false);
        if(!InitSubpass(&sp_composition,OS_TEXT("ds_composition.vert.spv"),OS_TEXT("ds_composition.frag.spv")))return(false);

        if(!InitGBufferPipeline(&sp_gbuffer))return(false);
        if(!InitCompositionPipeline(&sp_composition))return(false);

        texture.color   =vulkan::LoadTGATexture(OS_TEXT("cardboardPlainStain.tga"),device);
        texture.normal  =vulkan::LoadTGATexture(OS_TEXT("APOCWALL029_NRM.tga"),device);
        texture.specular=vulkan::LoadTGATexture(OS_TEXT("APOCWALL029_SPEC.tga"),device);

        return(true);
    }

    void CreateRenderObject(vulkan::Material *mtl)
    {
        struct CubeCreateInfo cci;

        ro_cube=CreateRenderableCube(db,mtl,&cci);
    }
    
    bool InitUBO(SubpassParam *sp)
    {
        if(!InitCameraUBO(sp->desc_sets,sp->material->GetUBO("world")))
            return(false);

        sp->desc_sets->Update();
        return(true);
    }

    bool InitScene(SubpassParam *sp)
    {
        if(!InitUBO(sp))
            return(false);

        CreateRenderObject(sp->material);

        render_root.Add(db->CreateRenderableInstance(sp->pipeline,sp->desc_sets,ro_cube),scale(1000));

        render_root.RefreshMatrix();
        render_root.ExpendToList(&render_list);

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

        return(true);
    }
    
    void BuildCommandBuffer(uint32_t index) override
    {
        render_root.RefreshMatrix();
        render_list.Clear();
        render_root.ExpendToList(&render_list);
        
        VulkanApplicationFramework::BuildCommandBuffer(index,&render_list);
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
