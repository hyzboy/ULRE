// 4.Geometry3D

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
    Color4f color;

    GPUBuffer *ubo_color=nullptr;

private:

    SceneNode   render_root;
    RenderList *render_list=nullptr;

    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    Pipeline *          pipeline            =nullptr;

    Renderable        * ro_plane_grid[3];

private:

    bool InitMDP()
    {
        material=db->CreateMaterial(OS_TEXT("res/material/VertexColor3D"));
        if(!material)return(false);

        material_instance=db->CreateMaterialInstance(material);
        if(!material_instance)return(false);
        
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid3D,Prim::Lines);
        if(!pipeline)
            return(false);

        return(true);
    }
    
    RenderableInstance *Add(Renderable *r,const Matrix4f &mat)
    {
        RenderableInstance *ri=db->CreateRenderableInstance(r,material_instance,pipeline);

        render_root.CreateSubNode(mat,ri);

        return ri;
    }

    void CreateRenderObject()
    {
        struct PlaneGridCreateInfo pgci;

        pgci.coord[0]=Vector3f(-100,-100,0);
        pgci.coord[1]=Vector3f( 100,-100,0);
        pgci.coord[2]=Vector3f( 100, 100,0);
        pgci.coord[3]=Vector3f(-100, 100,0);

        pgci.step.x=20;
        pgci.step.y=20;

        pgci.side_step.x=10;
        pgci.side_step.y=10;

        pgci.color.Set(0.5,0,0,1);
        pgci.side_color.Set(1,0,0,1);

        const VAB *vab=material_instance->GetVAB();

        ro_plane_grid[0]=CreateRenderablePlaneGrid(db,vab,&pgci);

        pgci.color.Set(0,0.5,0,1);
        pgci.side_color.Set(0,1,0,1);

        ro_plane_grid[1]=CreateRenderablePlaneGrid(db,vab,&pgci);

        pgci.color.Set(0,0,0.5,1);
        pgci.side_color.Set(0,0,1,1);
        ro_plane_grid[2]=CreateRenderablePlaneGrid(db,vab,&pgci);

    }

    bool InitScene()
    {
        Add(ro_plane_grid[0],Matrix4f(1.0f));
        Add(ro_plane_grid[1],rotate(HGL_RAD_90,0,1,0));
        Add(ro_plane_grid[2],rotate(HGL_RAD_90,1,0,0));

        camera->pos=Vector3f(200,200,200);
        camera->Refresh();

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
