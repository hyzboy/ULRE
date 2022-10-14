// 8.大气渲染
//  画一个球，纯粹使用shader计算出颜色

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=512;
constexpr uint32_t SCREEN_HEIGHT=512;

struct AtmosphereData
{
    Vector3f position;
    float intensity;
    float scattering_direction;

public:

    AtmosphereData()
    {    
        position=Vector3f(0,0.1f,-1.0f);
        intensity=22.0f;
        scattering_direction=0.758f;
    }
};//struct AtmosphereData

class TestApp:public CameraAppFramework
{
private:

    SceneNode           render_root;
    RenderList *        render_list=nullptr;

    MaterialInstance *  material_instance   =nullptr;
    Pipeline *          pipeline_solid      =nullptr;
    
    DeviceBuffer *      ubo_atomsphere      =nullptr;
    AtmosphereData      atomsphere_data;

    Primitive *         ro_sphere           =nullptr;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/Atmosphere"));       //不需要优先创建Material，也不需要写扩展名        
        if(!material_instance)return(false);
        
//        pipeline_solid=db->CreatePipeline(material_instance,sc_render_target,OS_TEXT("res/pipeline/sky"));
        pipeline_solid=CreatePipeline(material_instance,InlinePipeline::Sky,Prim::Triangles);       //等同上一行，为Framework重载，默认使用swapchain的render target
        if(!pipeline_solid)return(false);

        return(true);
    }

    bool InitUBO()
    {
        ubo_atomsphere=db->CreateUBO(sizeof(AtmosphereData),&atomsphere_data);

        if(!ubo_atomsphere)
            return(false);

        {
            MaterialParameters *mp=material_instance->GetMP(DescriptorSetsType::Value);

            if(!mp)return(false);

            if(!mp->BindUBO("sun",ubo_atomsphere))
                return(false);

            mp->Update();
        }
        return(true);
    }

    bool InitScene()
    {
        ro_sphere=inline_geometry::CreateSphere(db,material_instance->GetVIL(),128);
        
        render_root.CreateSubNode(scale(100),db->CreateRenderable(ro_sphere,material_instance,pipeline_solid));

        render_root.RefreshMatrix();
        render_list->Expend(GetCameraInfo(),&render_root);

        return(true);
    }

public:

    ~TestApp()
    {
        SAFE_CLEAR(render_list);
    }

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);
            
        render_list=new RenderList(device);

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
        render_list->Expend(GetCameraInfo(),&render_root);
        
        VulkanApplicationFramework::BuildCommandBuffer(index,render_list);
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
