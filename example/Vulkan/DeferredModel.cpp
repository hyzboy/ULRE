// 9.延迟渲染
//  简单的延迟渲染测试，仅一个太阳光

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderableInstance.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKImageView.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/Time.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_BEGIN
Texture2D *CreateTextureFromFile(GPUDevice *device,const OSString &filename);
VK_NAMESPACE_END

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=SCREEN_WIDTH/16*9;

using Texture2DPointer=Texture2D *;

enum class GBufferAttachment
{
    Position=0,
    Color,
    Normal,

    ENUM_CLASS_RANGE(Position,Normal)
};//

constexpr VkFormat gbuffer_color_format[size_t(GBufferAttachment::RANGE_SIZE)]={UFMT_RGBA32F,UFMT_RGBA32F,UFMT_RGBA32F};
constexpr VkFormat gbuffer_depth_format=FMT_D32F;

struct alignas(16) PhongPointLight
{
    Vector3f color;
    Vector4f position;
    float radius;
};//

class TestApp:public CameraAppFramework
{
private:

    SceneNode   render_root;
    RenderList  render_list;

    RenderTarget *gbuffer_rt;

    PhongPointLight lights;

    GPUBuffer *ubo_lights;

    struct SubpassParam
    {
        Material *          material;
        MaterialInstance *  material_instance;
        Pipeline *          pipeline_fan;
        Pipeline *          pipeline_triangles;
    };//

    SubpassParam            sp_gbuffer;
    SubpassParam            sp_composition;

    Renderable              *ro_plane,
                            *ro_cube,
                            *ro_sphere,
                            *ro_torus,
                            *ro_cylinder,
                            *ro_cone,
                                
                            *ro_gbc_plane;
    RenderableInstance      *ro_gbc_plane_ri;

    Sampler *               sampler=nullptr;

    struct
    {
        Texture2DPointer    color=nullptr;
        Texture2DPointer    normal=nullptr;
    }texture;

public:

    ~TestApp()
    {
        delete gbuffer_rt;
    }

private:

    bool InitGBuffer()
    {
        List<VkFormat> gbuffer_color_format_list(gbuffer_color_format,size_t(GBufferAttachment::RANGE_SIZE));

        gbuffer_rt=device->CreateRenderTarget(SCREEN_WIDTH,SCREEN_HEIGHT,gbuffer_color_format_list,gbuffer_depth_format);

        return(true);
    }

    bool InitSubpass(SubpassParam *sp,const OSString &material_filename)
    {
        sp->material=db->CreateMaterial(material_filename);

        if(!sp->material)
            return(false);

        sp->material_instance=db->CreateMaterialInstance(sp->material);
        return(true);
    }

    bool InitGBufferPipeline(SubpassParam *sp)
    {
        sp->pipeline_triangles  =db->CreatePipeline(sp->material,gbuffer_rt,InlinePipeline::Solid3D,Prim::Triangles);
        if(!sp->pipeline_triangles)
            return(false);

        sp->pipeline_fan        =db->CreatePipeline(sp->material,gbuffer_rt,InlinePipeline::Solid3D,Prim::Fan);

        return sp->pipeline_fan;
    }

    bool InitCompositionPipeline(SubpassParam *sp)
    {
        sp->pipeline_fan=db->CreatePipeline(sp->material,sc_render_target,InlinePipeline::Solid2D,Prim::Fan);

        return sp->pipeline_fan;
    }

    bool InitLightsUBO()
    {
        ubo_lights=db->CreateUBO(sizeof(lights),&lights);
        return ubo_lights;
    }

    bool InitMaterial()
    {
        if(!InitLightsUBO())return(false);

        if(!InitSubpass(&sp_gbuffer,    OS_TEXT("res/material/opaque")))return(false);
        if(!InitSubpass(&sp_composition,OS_TEXT("res/material/composition")))return(false);

        if(!InitGBufferPipeline(&sp_gbuffer))return(false);
        if(!InitCompositionPipeline(&sp_composition))return(false);

        texture.color   =db->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"));
        texture.normal  =db->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"));
        
        VkSamplerCreateInfo sampler_create_info=
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
            VK_TRUE,
            device->GetGPUPhysicalDevice()->GetMaxSamplerAnisotropy(),
            false,
            VK_COMPARE_OP_NEVER,
            0.0f,
            static_cast<float>(texture.color->GetMipLevel()),
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            false
        };

        sampler=db->CreateSampler(&sampler_create_info);

        sp_gbuffer.material_instance->BindUBO("world",GetCameraMatrixBuffer());
        sp_gbuffer.material_instance->BindSampler("TexColor"    ,texture.color,    sampler);
        sp_gbuffer.material_instance->BindSampler("TexNormal"   ,texture.normal,   sampler);
        sp_gbuffer.material_instance->Update();

        sp_composition.material_instance->BindUBO("world",GetCameraMatrixBuffer());
        sp_composition.material_instance->BindUBO("lights",ubo_lights);
        sp_composition.material_instance->BindSampler("GB_Position" ,gbuffer_rt->GetColorTexture((uint)GBufferAttachment::Position),sampler);
        sp_composition.material_instance->BindSampler("GB_Normal"   ,gbuffer_rt->GetColorTexture((uint)GBufferAttachment::Normal),sampler);
        sp_composition.material_instance->BindSampler("GB_Color"    ,gbuffer_rt->GetColorTexture((uint)GBufferAttachment::Color),sampler);
        sp_composition.material_instance->Update();

        return(true);
    }

    void CreateRenderObject(Material *mtl)
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

            tci.numberSlices=128;
            tci.numberStacks=64;

            tci.uv_scale.x=4;
            tci.uv_scale.y=1;

            ro_torus=CreateRenderableTorus(db,mtl,&tci);
        }

        {
            CylinderCreateInfo cci;

            cci.halfExtend=10;
            cci.radius=10;
            cci.numberSlices=32;

            ro_cylinder=CreateRenderableCylinder(db,mtl,&cci);
        }

        {
            ConeCreateInfo cci;

            cci.halfExtend=10;
            cci.radius=10;
            cci.numberSlices=128;
            cci.numberStacks=32;

            ro_cone=CreateRenderableCone(db,mtl,&cci);
        }
    }

    bool InitCompositionRenderable()
    {
        ro_gbc_plane=CreateRenderableGBufferComposition(db,sp_composition.material);
        if(!ro_gbc_plane)return(false);

        ro_gbc_plane_ri=db->CreateRenderableInstance(ro_gbc_plane,sp_composition.material_instance,sp_composition.pipeline_fan);
        if(!ro_gbc_plane_ri)return(false);

        return(true);
    }

    bool InitScene(SubpassParam *sp)
    {
        CreateRenderObject(sp->material);
        render_root.Add(db->CreateRenderableInstance(ro_plane      ,sp->material_instance,sp->pipeline_fan      ),scale(100,100,1));
        render_root.Add(db->CreateRenderableInstance(ro_torus      ,sp->material_instance,sp->pipeline_triangles),translate(0,0,0));
        render_root.Add(db->CreateRenderableInstance(ro_sphere     ,sp->material_instance,sp->pipeline_triangles),scale(20,20,20));
        render_root.Add(db->CreateRenderableInstance(ro_cube       ,sp->material_instance,sp->pipeline_triangles),translate(-30,  0,10)*scale(10,10,10));        
        render_root.Add(db->CreateRenderableInstance(ro_cylinder   ,sp->material_instance,sp->pipeline_triangles),translate( 30, 30,10)*scale(1,1,2));
        render_root.Add(db->CreateRenderableInstance(ro_cone       ,sp->material_instance,sp->pipeline_triangles),translate(  0,-30, 0)*scale(1,1,2));

        render_root.RefreshMatrix();
        render_root.ExpendToList(&render_list);

        return(true);
    }

    bool InitGBufferCommandBuffer()
    {
        GPUCmdBuffer *gbuffer_cmd=gbuffer_rt->GetCommandBuffer();

        if(!gbuffer_cmd)
            return(false);

        gbuffer_cmd->Begin();
            if(!gbuffer_cmd->BindFramebuffer(gbuffer_rt))
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

    void UpdateLights()
    {
        const double timer=GetDoubleTime();
        
		// White
		lights.position = Vector4f(0.0f, 0.0f, 50.0f, 0.0f);
		lights.color = Vector3f(15.0f);
		lights.radius = 150.0f;

		lights.position.x = sin(hgl_rad2ang(timer/100)) * 100.0f;
		lights.position.y = cos(hgl_rad2ang(timer/100)) * 100.0f;

        ubo_lights->Write(&lights);
    }
    
    virtual void SubmitDraw(int index) override
    {   
        gbuffer_rt->Submit(sc_render_target->GetPresentCompleteSemaphore());

        VkCommandBuffer cb=*cmd_buf[index];
        
        sc_render_target->Submit(cb,gbuffer_rt->GetRenderCompleteSemaphore());
        sc_render_target->PresentBackbuffer();
        sc_render_target->WaitQueue();
        sc_render_target->WaitFence();

        gbuffer_rt->WaitQueue();
        gbuffer_rt->WaitFence();
    }
    
    void BuildCommandBuffer(uint32_t index) override
    {    
        VulkanApplicationFramework::BuildCommandBuffer(index,ro_gbc_plane_ri);
    }
    
    void Draw()override
    {
        UpdateLights();

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
