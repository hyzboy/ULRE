#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

struct SkyColorConfig
{
    Color4f Color;
    Color4f LowColor;
};

class TestApp:public CameraAppFramework
{
    SkyColorConfig scc;

    GPUBuffer *ubo_color=nullptr;

private:

    SceneNode   render_root;
    RenderList *render_list=nullptr;

    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    Pipeline *          pipeline            =nullptr;

    GPUBuffer *         ubo_sky_color       =nullptr;

    Primitive        * ro_skyphere         =nullptr;

private:

    bool InitMDP()
    {
        material=db->CreateMaterial(OS_TEXT("res/material/SkyColor"));
        if(!material)return(false);

        material_instance=db->CreateMaterialInstance(material);
        if(!material_instance)return(false);
        
        pipeline=CreatePipeline(material_instance,InlinePipeline::Sky,Prim::Triangles);
        if(!pipeline)
            return(false);

        return(true);
    }

    bool InitUBO()
    {
        scc.Color.Set(
        //.1, .3, .6      //blue
        //.18,.19,.224    //overcast
        //.1, .15,.4      //dusk
        .03,.2, .9      //tropical blue
        //.4, .06,.01     //orange-red
        //.1, .2, .01     //green
        );

        scc.LowColor=GetColor4f(COLOR::DarkMidnightBlue,1.0f);

        ubo_sky_color=db->CreateUBO(sizeof(SkyColorConfig),&scc);

        if(!ubo_sky_color)
            return(false);

        if(!material_instance->BindUBO(DescriptorSetsType::Value,"sky_color",ubo_sky_color))
            return(false);

        return(true);
    }
    
    Renderable *Add(Primitive *r,const Matrix4f &mat)
    {
        Renderable *ri=db->CreateRenderable(r,material_instance,pipeline);

        render_root.CreateSubNode(mat,ri);

        return ri;
    }

    void CreateRenderObject()
    {
        const VIL *vil=material_instance->GetVIL();

        ro_skyphere=inline_geometry::CreateDome(db,vil,64);
    }

    bool InitScene()
    {
        camera->pos=Vector3f(0,0,0);
        camera_control->SetTarget(Vector3f(200,200,200));
        camera_control->Refresh();

        Add(ro_skyphere,scale(200));

        render_root.RefreshMatrix();
        render_list->Expend(camera->info,&render_root);

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

        if(!InitMDP())
            return(false);

        if(!InitUBO())
            return(false);

        CreateRenderObject();

        if(!InitScene())
            return(false);

        return(true);
    }

    void BuildCommandBuffer(uint32 index)
    {
        render_root.RefreshMatrix();
        render_list->Expend(GetCameraInfo(),&render_root);

        VulkanApplicationFramework::BuildCommandBuffer(index,render_list);
    }

    void Resize(int w,int h)override
    {
        CameraAppFramework::Resize(w,h);
        
        VulkanApplicationFramework::BuildCommandBuffer(render_list);
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
