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

struct AtmosphereData
{
    Vector3f position;
    float intensity;
    float scattering_direction;

public:

    AtmosphereData()
    {    
        position.Set(0,0.1f,-1.0f);
        intensity=22.0f;
        scattering_direction=0.758f;
    }
};//struct AtmosphereData

class TestApp:public CameraAppFramework
{
private:

    SceneNode   render_root;
    RenderList  render_list;

    vulkan::MaterialInstance *  material_instance   =nullptr;
    vulkan::Pipeline *          pipeline_solid      =nullptr;
    
    vulkan::Buffer *            ubo_atomsphere      =nullptr;
    AtmosphereData              atomsphere_data;

    vulkan::Renderable *        ro_sphere           =nullptr;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/Atmosphere"));       //不需要优先创建Material，也不需要写扩展名        
        if(!material_instance)return(false);
        
//        pipeline_solid=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/sky"));
        pipeline_solid=CreatePipeline(material_instance,OS_TEXT("res/pipeline/sky"));       //等同上一行，为Framework重载，默认使用swapchain的render target
        if(!pipeline_solid)return(false);

        return(true);
    }

    bool InitUBO()
    {
        if(!material_instance->BindUBO("world",GetCameraMatrixBuffer()))
            return(false);

        ubo_atomsphere=db->CreateUBO(sizeof(AtmosphereData),&atomsphere_data);

        if(!ubo_atomsphere)
            return(false);

        if(!material_instance->BindUBO("sun",ubo_atomsphere))
            return(false);

        material_instance->Update();
        return(true);
    }

    bool InitScene()
    {
        ro_sphere=CreateRenderableSphere(db,material_instance->GetMaterial(),128);

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

        if(!InitUBO())
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
