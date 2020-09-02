// 8.大气渲染
//  画一个球，纯粹使用shader计算出颜色

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/vulkan/VKDatabase.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

struct AtomsphereData
{
    Vector3f position;
    float intensity;
    float scattering_direction;
};//

class TestApp:public CameraAppFramework
{
private:

    SceneNode   render_root;
    RenderList  render_list;

    vulkan::Material *          material            =nullptr;
    vulkan::MaterialInstance *  material_instance   =nullptr;

    vulkan::Renderable *        ro_sphere           =nullptr;

    vulkan::Pipeline *          pipeline_solid      =nullptr;
    
    vulkan::Buffer *            ubo_atomsphere      =nullptr;
    AtomsphereData              atomsphere_data;

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("res/shader/Atomsphere.vert"),
                                               OS_TEXT("res/shader/Atomsphere.frag"));
        if(!material)
            return(false);

        material_instance=material->CreateInstance();

        db->Add(material);
        db->Add(material_instance);
        return(true);
    }

    void CreateRenderObject()
    {
        ro_sphere=CreateRenderableSphere(db,material,128);
    }

    bool InitAtomsphereUBO(vulkan::MaterialInstance *mi,const AnsiString &sun_node_name)
    {
        atomsphere_data.position.Set(0,0.1f,-1.0f);
        atomsphere_data.intensity=22.0f;
        atomsphere_data.scattering_direction=0.758f;

        ubo_atomsphere=db->CreateUBO(sizeof(AtomsphereData),&atomsphere_data);

        if(!ubo_atomsphere)
            return(false);

        return mi->BindUBO(sun_node_name,ubo_atomsphere);
    }

    bool InitUBO()
    {
        if(!material_instance->BindUBO("world",GetCameraMatrixBuffer()))
            return(false);

        if(!InitAtomsphereUBO(material_instance,"sun"))
            return(false);

        material_instance->Update();
        return(true);
    }

    bool InitPipeline()
    {
        AutoDelete<vulkan::PipelineCreater> 
        pipeline_creater=new vulkan::PipelineCreater(device,material,sc_render_target);
        pipeline_creater->SetDepthTest(true);
        pipeline_creater->SetDepthWrite(true);
        pipeline_creater->SetCullMode(VK_CULL_MODE_NONE);
        pipeline_creater->Set(Prim::Triangles);
        pipeline_solid=pipeline_creater->Create();
        
        if(!pipeline_solid)
            return(false);

        db->Add(pipeline_solid);
        return(true);
    }

    bool InitScene()
    {
        render_root.Add(db->CreateRenderableInstance(pipeline_solid,material_instance,ro_sphere),scale(100));

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
