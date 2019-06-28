// 9.延迟渲染
//  简单的延迟渲染测试，仅一个太阳光

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/SceneDB.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/RenderList.h>

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

class TestApp:public CameraAppFramework
{
private:

    SceneNode   render_root;
    RenderList  render_list;

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
        vulkan::PipelineCreater *pipeline_creater=new vulkan::PipelineCreater(device,sp->material,device->GetRenderPass(),device->GetExtent());
        pipeline_creater->SetDepthTest(true);
        pipeline_creater->SetDepthWrite(true);
        pipeline_creater->SetCullMode(VK_CULL_MODE_BACK_BIT);
        pipeline_creater->Set(PRIM_TRIANGLES);
        sp->pipeline=pipeline_creater->Create();
        
        if(!sp->pipeline)
            return(false);

        db->Add(sp->pipeline);

        delete pipeline_creater;
        return(true);
    }

    bool InitCompositionPipeline(SubpassParam *sp)
    {
        vulkan::PipelineCreater *pipeline_creater=new vulkan::PipelineCreater(device,sp->material,device->GetRenderPass(),device->GetExtent());
        pipeline_creater->SetDepthTest(false);
        pipeline_creater->SetDepthWrite(false);
        pipeline_creater->SetCullMode(VK_CULL_MODE_NONE);
        pipeline_creater->Set(PRIM_TRIANGLES);
        sp->pipeline=pipeline_creater->Create();
        
        if(!sp->pipeline)
            return(false);

        db->Add(sp->pipeline);

        delete pipeline_creater;
        return(true);
    }

    bool InitMaterial()
    {
        InitSubpass(&sp_gbuffer,        OS_TEXT("gbuffer_opaque.vert.spv"),OS_TEXT("gbuffer_opaque.frag.spv"));
        InitSubpass(&sp_composition,    OS_TEXT("ds_composition.vert.spv"),OS_TEXT("ds_composition.frag.spv"));

        InitGBufferPipeline(&sp_gbuffer);
        InitCompositionPipeline(&sp_composition);
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
