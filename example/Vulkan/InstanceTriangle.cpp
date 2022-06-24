// Instance Triangle

// 基本的Instance绘制测试用例,不使用场景树

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

class TestApp:public VulkanApplicationFramework
{
private:

    double      start_time;

    MaterialInstance *  material_instance   =nullptr;
    Renderable *        renderable_object   =nullptr;
    RenderableInstance *render_instance     =nullptr;

    Pipeline *          pipeline            =nullptr;
    
public:

    TestApp()
    {
        start_time=GetDoubleTime();
    }
    
    ~TestApp()=default;

private:

    bool InitMaterial()
    {
        material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/Vertex2DColor"));

        if(!material_instance)
            return(false);
           
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D,Prim::Triangles);

        return pipeline;
    }
    
    bool InitUBO()
    {
        color_material.diffuse.Set(1,1,1);
        color_material.abiment.Set(0.25,0.25,0.25);

        ubo_color=db->CreateUBO(sizeof(color_material),&color_material);
        if(!ubo_color)return(false);

        sun_direction=normalized(Vector3f(rand(),rand(),rand()));

        ubo_sun=db->CreateUBO(sizeof(sun_direction),&sun_direction);
        if(!ubo_sun)return(false);

        {
            MaterialParameters *mp=material_instance->GetMP(DescriptorSetsType::Value);

            if(!mp)return(false);

            mp->BindUBO("material",ubo_color);
            mp->BindUBO("sun",ubo_sun);

            mp->Update();
        }

        BindCameraUBO(material_instance);
        return(true);
    }

    bool InitRenderable()
    {
        CubeCreateInfo cci;

        cci.tangent=false;
        cci.tex_coord=false;

        renderable_object=CreateRenderableCube(db,material_instance->GetVAB(),&cci);

        if(!renderable_object)
            return(false);

        render_instance=db->CreateRenderableInstance(renderable_object,material_instance,pipeline);

        if(!render_instance)
            return(false);

        return(true);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitMaterial())
            return(false);

        if(!InitUBO())
            return(false);

        if(!InitRenderable())
            return(false);

        if(!VulkanApplicationFramework::BuildCommandBuffer(render_instance))
            return(false);

        return(true);
    }

    void Resize(int w,int h)override
    {
        VulkanApplicationFramework::Resize(w,h);

        VulkanApplicationFramework::BuildCommandBuffer(render_instance);
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
