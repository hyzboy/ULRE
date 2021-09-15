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
    RenderableInstance *ri_plane_grid[3];

private:

    bool RecreatePipeline()
    {
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid3D,Prim::Lines);

        return pipeline;
    }

    bool InitMDP()
    {
        material=db->CreateMaterial(OS_TEXT("res/material/VertexColor3D"));
        if(!material)return(false);

        material_instance=db->CreateMaterialInstance(material);
        if(!material_instance)return(false);

        if(!RecreatePipeline())
            return(false);

        {
            MaterialParameters *mp_global=material_instance->GetMP(DescriptorSetType::Global);
        
            if(!mp_global)
                return(false);

            if(!mp_global->BindUBO("g_camera",GetCameraInfoBuffer()))return(false);

            mp_global->Update();
        }

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

        pgci.coord[0].Set(-100,-100,0);
        pgci.coord[1].Set( 100,-100,0);
        pgci.coord[2].Set( 100, 100,0);
        pgci.coord[3].Set(-100, 100,0);

        pgci.step.u=20;
        pgci.step.v=20;

        pgci.side_step.u=10;
        pgci.side_step.v=10;

        pgci.color.Set(0.5,0,0,1);
        pgci.side_color.Set(1,0,0,1);

        ro_plane_grid[0]=CreateRenderablePlaneGrid(db,material,&pgci);

        pgci.color.Set(0,0.5,0,1);
        pgci.side_color.Set(0,1,0,1);

        ro_plane_grid[1]=CreateRenderablePlaneGrid(db,material,&pgci);

        pgci.color.Set(0,0,0.5,1);
        pgci.side_color.Set(0,0,1,1);
        ro_plane_grid[2]=CreateRenderablePlaneGrid(db,material,&pgci);

        camera->pos.Set(200,200,200,1.0);
    }

    bool InitScene()
    {
        ri_plane_grid[0]=Add(ro_plane_grid[0],Matrix4f::identity);
        ri_plane_grid[1]=Add(ro_plane_grid[1],rotate(HGL_RAD_90,0,1,0));
        ri_plane_grid[2]=Add(ro_plane_grid[2],rotate(HGL_RAD_90,1,0,0));

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
        VulkanApplicationFramework::BuildCommandBuffer(index,render_list);
    }

    void Resize(int w,int h)override
    {
        CameraAppFramework::Resize(w,h);
        
        RecreatePipeline();

        for(int i=0;i<3;i++)
            ri_plane_grid[i]->UpdatePipeline(pipeline);
        
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
