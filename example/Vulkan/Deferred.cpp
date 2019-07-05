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

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

struct AtomsphereData
{
    alignas(16) Vector3f position;
    float intensity;
    float scattering_direction;
};//

using Texture2DPointer=vulkan::Texture2D *;

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

    vulkan::Renderable          *ro_sphere;

private:

    bool InitGBuffer()
    {
        gbuffer.width=power_to_2(SCREEN_WIDTH);
        gbuffer.height=power_to_2(SCREEN_HEIGHT);

        gbuffer.position=device->CreateTexture2DColor(FMT_RGB32F,   gbuffer.width,gbuffer.height);
        gbuffer.color   =device->CreateTexture2DColor(FMT_RGB32F,   gbuffer.width,gbuffer.height);
        gbuffer.normal  =device->CreateTexture2DColor(FMT_RGB32F,   gbuffer.width,gbuffer.height);
        gbuffer.depth   =device->CreateTexture2DDepth(FMT_D32F,     gbuffer.width,gbuffer.height);

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

        device->CreateSubpassDependency(gbuffer.subpass.dependency,2);

        gbuffer.renderpass=device->CreateRenderPass(gbuffer.attachment.desc_list,
                                                    gbuffer.subpass.desc,
                                                    gbuffer.subpass.dependency,
                                                    gbuffer.gbuffer_format_list,
                                                    gbuffer.depth->GetFormat());

        if(!gbuffer.renderpass)
            return(false);

        gbuffer.framebuffer=vulkan::CreateFramebuffer(device,gbuffer.renderpass,gbuffer.image_view_list);

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
        SharedPtr<vulkan::PipelineCreater> pipeline_creater=new vulkan::PipelineCreater(device,sp->material,gbuffer.renderpass,device->GetExtent());
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
        SharedPtr<vulkan::PipelineCreater> pipeline_creater=new vulkan::PipelineCreater(device,sp->material,device->GetMainRenderPass(),device->GetExtent());
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
        if(!InitSubpass(&sp_gbuffer,        OS_TEXT("gbuffer_opaque.vert.spv"),OS_TEXT("gbuffer_opaque.frag.spv")))return(false);
        if(!InitSubpass(&sp_composition,    OS_TEXT("ds_composition.vert.spv"),OS_TEXT("ds_composition.frag.spv")))return(false);

        if(!InitGBufferPipeline(&sp_gbuffer))return(false);
        if(!InitCompositionPipeline(&sp_composition))return(false);

        return(true);
    }

    void CreateRenderObject(vulkan::Material *mtl)
    {
        ro_sphere=CreateRenderableSphere(db,mtl,128);
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

        render_root.Add(db->CreateRenderableInstance(sp->pipeline,sp->desc_sets,ro_sphere),scale(1000));

        render_root.RefreshMatrix();
        render_root.ExpendToList(&render_list);

        return(true);
    }

public:

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
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
