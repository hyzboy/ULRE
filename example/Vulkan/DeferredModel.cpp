// 9.延迟渲染
//  简单的延迟渲染测试，仅一个太阳光

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKDatabase.h>
#include<hgl/graph/VKRenderableInstance.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKImageView.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/VKFramebuffer.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTextureFromFile(Device *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_WIDTH=512;
constexpr uint32_t SCREEN_HEIGHT=512;

using Texture2DPointer=vulkan::Texture2D *;

class TestApp:public CameraAppFramework
{
private:

    SceneNode   render_root;
    RenderList  render_list;

    vulkan::RenderTarget *gbuffer_rt;

    struct SubpassParam
    {
        vulkan::Material *          material;
        vulkan::MaterialInstance *  material_instance;
        vulkan::Pipeline *          pipeline_fan;
        vulkan::Pipeline *          pipeline_triangles;
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
    }texture;

    vulkan::CommandBuffer       *gbuffer_cmd=nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(gbuffer_cmd);
        SAFE_CLEAR(texture.normal);
        SAFE_CLEAR(texture.color);
        SAFE_CLEAR(sampler);
    }

private:

    bool InitGBuffer()
    {
        List<VkFormat> gbuffer_color_format;

        gbuffer_color_format.Add(UFMT_RGBA32F);     //position
        gbuffer_color_format.Add(UFMT_RGBA32F);     //color
        gbuffer_color_format.Add(UFMT_RGBA32F);     //normal

        gbuffer_rt=device->CreateRenderTarget(SCREEN_WIDTH,SCREEN_HEIGHT,gbuffer_color_format,FMT_D32F);

        return(true);
    }

    bool InitSubpass(SubpassParam *sp,const OSString &vs,const OSString &fs)
    {
        sp->material=db->CreateMaterial(vs,fs);

        if(!sp->material)
            return(false);

        sp->material_instance=db->CreateMaterialInstance(sp->material);
        return(true);
    }

    bool InitGBufferPipeline(SubpassParam *sp)
    {
        sp->pipeline_triangles  =db->CreatePipeline(sp->material,gbuffer_rt,vulkan::InlinePipeline::Solid3D,Prim::Triangles);
        if(!sp->pipeline_triangles)
            return(false);

        sp->pipeline_fan        =db->CreatePipeline(sp->material,gbuffer_rt,vulkan::InlinePipeline::Solid3D,Prim::Fan);

        return sp->pipeline_fan;
    }

    bool InitCompositionPipeline(SubpassParam *sp)
    {
        sp->pipeline_fan=db->CreatePipeline(sp->material,gbuffer_rt,vulkan::InlinePipeline::Solid2D,Prim::Fan);

        return sp->pipeline_fan;
    }

    bool InitMaterial()
    {
        if(!InitSubpass(&sp_gbuffer,    OS_TEXT("res/shader/gbuffer_opaque.vert.spv"),OS_TEXT("res/shader/gbuffer_opaque.frag.spv")))return(false);
        if(!InitSubpass(&sp_composition,OS_TEXT("res/shader/gbuffer_composition.vert.spv"),OS_TEXT("res/shader/gbuffer_composition.frag.spv")))return(false);

        if(!InitGBufferPipeline(&sp_gbuffer))return(false);
        if(!InitCompositionPipeline(&sp_composition))return(false);

        texture.color   =vulkan::CreateTextureFromFile(device,OS_TEXT("res/image/Brickwall/Albedo.Tex2D"));
        texture.normal  =vulkan::CreateTextureFromFile(device,OS_TEXT("res/image/Brickwall/Normal.Tex2D"));

        sampler=device->CreateSampler();

        InitCameraUBO(sp_gbuffer.material_instance,"world");

        sp_gbuffer.material_instance->BindSampler("TextureColor"    ,texture.color,    sampler);
        sp_gbuffer.material_instance->BindSampler("TextureNormal"   ,texture.normal,   sampler);
        sp_gbuffer.material_instance->Update();

        sp_composition.material_instance->BindSampler("GB_Position" ,gbuffer_rt->GetColorTexture(0),sampler);
        sp_composition.material_instance->BindSampler("GB_Normal"   ,gbuffer_rt->GetColorTexture(1),sampler);
        sp_composition.material_instance->BindSampler("GB_Color"    ,gbuffer_rt->GetColorTexture(2),sampler);
        sp_composition.material_instance->Update();

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
        render_root.Add(db->CreateRenderableInstance(sp->pipeline_fan,      sp->material_instance,ro_plane      ),scale(100,100,1));
        render_root.Add(db->CreateRenderableInstance(sp->pipeline_triangles,sp->material_instance,ro_torus      ),translate(0,0,0));
        render_root.Add(db->CreateRenderableInstance(sp->pipeline_triangles,sp->material_instance,ro_sphere     ),scale(20,20,20));
        render_root.Add(db->CreateRenderableInstance(sp->pipeline_triangles,sp->material_instance,ro_cube       ),translate(-30,  0,10)*scale(10,10,10));        
        render_root.Add(db->CreateRenderableInstance(sp->pipeline_triangles,sp->material_instance,ro_cylinder   ),translate( 30, 30,10)*scale(1,1,2));
        render_root.Add(db->CreateRenderableInstance(sp->pipeline_triangles,sp->material_instance,ro_cone       ),translate(  0,-30, 0)*scale(1,1,2));

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
                                                        sp_composition.pipeline_fan,
                                                        sp_composition.material_instance,
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
