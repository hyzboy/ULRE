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

class TestApp:public CameraAppFramework
{
    Color3f color;

    GPUBuffer *ubo_color=nullptr;

private:

    SceneNode   render_root;
    RenderList *render_list=nullptr;

    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    Pipeline *          pipeline            =nullptr;

    GPUBuffer *         ubo_sky_color       =nullptr;

    Renderable        * ro_skyphere         =nullptr;

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
        color.Set(
        //.1, .3, .6      //blue
        //.18,.19,.224    //overcast
        //.1, .15,.4      //dusk
        .03,.2, .9      //tropical blue
        //.4, .06,.01     //orange-red
        //.1, .2, .01     //green
        );

        ubo_sky_color=db->CreateUBO(sizeof(Color3f),&color);

        if(!ubo_sky_color)
            return(false);

        {
            MaterialParameters *mp=material_instance->GetMP(DescriptorSetsType::Value);

            if(!mp)return(false);

            if(!mp->BindUBO("sky_color",ubo_sky_color))
                return(false);

            mp->Update();
        }
    }
    
    RenderableInstance *Add(Renderable *r,const Matrix4f &mat)
    {
        RenderableInstance *ri=db->CreateRenderableInstance(r,material_instance,pipeline);

        render_root.CreateSubNode(mat,ri);

        return ri;
    }

    void CreateRenderObject()
    {
        const VAB *vab=material_instance->GetVAB();

        ro_skyphere=CreateRenderableDome(db,vab,32);
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
