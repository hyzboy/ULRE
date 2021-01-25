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
    RenderList  render_list;

    struct MDP
    {
        Material *          material            =nullptr;
        MaterialInstance *  material_instance   =nullptr;
        Pipeline *          pipeline            =nullptr;
    }m3d,m2d;

    Renderable              *ro_plane_grid[3],
                            *ro_round_rectangle =nullptr;

private:

    bool InitMDP(MDP *mdp,const Prim primitive,const OSString &mtl_name)
    {
        mdp->material=db->CreateMaterial(mtl_name);
        if(!mdp->material)return(false);

        mdp->material_instance=db->CreateMaterialInstance(mdp->material);
        if(!mdp->material_instance)return(false);

        mdp->pipeline=CreatePipeline(mdp->material_instance,InlinePipeline::Solid3D,primitive);

        if(!mdp->material_instance->BindUBO("camera",GetCameraMatrixBuffer()))
            return(false);

        mdp->material_instance->Update();
        return(true);
    }
    
    void Add(Renderable *r,MDP &mdp)
    {
        auto ri=db->CreateRenderableInstance(r,mdp.material_instance,mdp.pipeline);

        render_root.Add(ri);
    }

    void Add(Renderable *r,MDP &mdp,const Matrix4f &mat)
    {
        auto ri=db->CreateRenderableInstance(r,mdp.material_instance,mdp.pipeline);

        render_root.Add(ri,mat);
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

        ro_plane_grid[0]=CreateRenderablePlaneGrid(db,m3d.material,&pgci);

        pgci.color.Set(0,0.5,0,1);
        pgci.side_color.Set(0,1,0,1);

        ro_plane_grid[1]=CreateRenderablePlaneGrid(db,m3d.material,&pgci);

        pgci.color.Set(0,0,0.5,1);
        pgci.side_color.Set(0,0,1,1);
        ro_plane_grid[2]=CreateRenderablePlaneGrid(db,m3d.material,&pgci);

        {
            struct RoundRectangleCreateInfo rrci;

            rrci.scope.Set(SCREEN_WIDTH-30,10,20,20);
            rrci.radius=5;
            rrci.round_per=5;

            ro_round_rectangle=CreateRenderableRoundRectangle(db,m2d.material,&rrci);
        }

        camera->pos.Set(200,200,200,1.0);
    }

    bool InitScene()
    {
        Add(ro_round_rectangle,m2d);
        Add(ro_plane_grid[0],m3d);
        Add(ro_plane_grid[1],m3d,rotate(HGL_RAD_90,0,1,0));
        Add(ro_plane_grid[2],m3d,rotate(HGL_RAD_90,1,0,0));

        render_root.RefreshMatrix();
        render_root.ExpendToList(&render_list);

        return(true);
    }

public:

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitMDP(&m3d,Prim::Lines,OS_TEXT("res/material/VertexColor3D")))
            return(false);

        if(!InitMDP(&m2d,Prim::Fan, OS_TEXT("res/material/PureColor2D")))
            return(false);

        {
            color.Set(1,1,0,1);
            ubo_color=device->CreateUBO(sizeof(Vector4f),&color);

            m2d.material_instance->BindUBO("color_material",ubo_color);
            m2d.material_instance->Update();

            db->Add(ubo_color);
        }

        CreateRenderObject();

        if(!InitScene())
            return(false);

        return(true);
    }

    void BuildCommandBuffer(uint32 index)
    {
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
