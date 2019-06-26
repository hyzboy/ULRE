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
        vulkan::Material *      mtl;
        vulkan::DescriptorSets *desc_sets;
        vulkan::Pipeline *      pipeline;
    };//

    SubpassParam *              mrt;
    SubpassParam *              composition;

    vulkan::Renderable          *ro_sphere;
   
    vulkan::Buffer *            ubo_atomsphere      =nullptr;
    AtomsphereData              atomsphere_data;

private:

    bool InitMaterial()
    {
        material_mrt=shader_manage->CreateMaterial( OS_TEXT("Atomsphere.vert.spv"),
                                                    OS_TEXT("Atomsphere.frag.spv"));
        if(!material_mrt)
            return(false);

        descriptor_sets=material_mrt->CreateDescriptorSets();

        db->Add(material);
        db->Add(descriptor_sets);
        return(true);
    }

    bool InitMaterial()
    {
    }

    void CreateRenderObject()
    {
        ro_sphere=CreateRenderableSphere(db,material,128);
    }
    
    bool InitAtomsphereUBO(vulkan::DescriptorSets *desc_set,uint bindpoint)
    {
        atomsphere_data.position.Set(0,0.1f,-1.0f);
        atomsphere_data.intensity=22.0f;
        atomsphere_data.scattering_direction=0.758f;

        ubo_atomsphere=db->CreateUBO(sizeof(AtomsphereData),&atomsphere_data);

        if(!ubo_atomsphere)
            return(false);

        return desc_set->BindUBO(bindpoint,*ubo_atomsphere);
    }

    bool InitUBO()
    {
        if(!InitCameraUBO(descriptor_sets,material->GetUBO("world")))
            return(false);

        if(!InitAtomsphereUBO(descriptor_sets,material->GetUBO("sun")))
            return(false);

        descriptor_sets->Update();
        return(true);
    }

    bool InitPipeline()
    {
        vulkan::PipelineCreater *pipeline_creater=new vulkan::PipelineCreater(device,material,device->GetRenderPass(),device->GetExtent());
        pipeline_creater->SetDepthTest(true);
        pipeline_creater->SetDepthWrite(true);
        pipeline_creater->SetCullMode(VK_CULL_MODE_NONE);
        pipeline_creater->Set(PRIM_TRIANGLES);
        pipeline_solid=pipeline_creater->Create();
        
        if(!pipeline_solid)
            return(false);

        db->Add(pipeline_solid);

        delete pipeline_creater;
        return(true);
    }

    bool InitScene()
    {
        render_root.Add(db->CreateRenderableInstance(pipeline_solid,descriptor_sets,ro_sphere),scale(1000));

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

        CreateRenderObject();

        if(!InitUBO())
            return(false);

        if(!InitPipeline())
            return(false);

        if(!InitScene())
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
