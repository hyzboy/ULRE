// 4.Geometry3D

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/SceneDB.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

class TestApp:public CameraAppFramework
{
    Color4f color;

    vulkan::Buffer *ubo_color=nullptr;

private:

    SceneNode   render_root;
    RenderList  render_list;

    struct MDP
    {
        vulkan::Material *          material            =nullptr;
        vulkan::MaterialInstance *  material_instance   =nullptr;
        vulkan::Pipeline *          pipeline            =nullptr;
    }m3d,m2d;

    vulkan::Renderable          *ro_plane_grid[3],
                                *ro_round_rectangle =nullptr;

private:

    bool InitMaterial(MDP *mdp,const OSString &vs,const OSString &fs)
    {
        mdp->material=shader_manage->CreateMaterial(vs,fs);
        
        if(!mdp->material)
            return(false);

        mdp->material_instance=mdp->material->CreateInstance();

        db->Add(mdp->material);
        db->Add(mdp->material_instance);
        return(true);
    }

    bool InitUBO(MDP *mdp)
    {
        if(!mdp->material_instance->BindUBO("world",GetCameraMatrixBuffer()))
            return(false);

        mdp->material_instance->Update();
        return(true);
    }

    bool InitPipeline(MDP *mdp,const VkPrimitiveTopology primitive)
    {
        AutoDelete<vulkan::PipelineCreater> 
        pipeline_creater=new vulkan::PipelineCreater(device,mdp->material,sc_render_target);
        pipeline_creater->CloseCullFace();
        pipeline_creater->Set(primitive);

        mdp->pipeline=pipeline_creater->Create();

        if(!mdp->pipeline)
            return(false);

        db->Add(mdp->pipeline);
        return(true);
    }

    bool InitMDP(MDP *mdp,const VkPrimitiveTopology primitive,const OSString &vs,const OSString &fs)
    {
        if(!InitMaterial(mdp,vs,fs))
            return(false);

        if(!InitUBO(mdp))
            return(false);

        if(!InitPipeline(mdp,primitive))
            return(false);

        return(true);
    }

    bool InitScene()
    {
        render_root.Add(db->CreateRenderableInstance(m2d.pipeline,m2d.material_instance,ro_round_rectangle));

        render_root.Add(db->CreateRenderableInstance(m3d.pipeline,m3d.material_instance,ro_plane_grid[0]));
        render_root.Add(db->CreateRenderableInstance(m3d.pipeline,m3d.material_instance,ro_plane_grid[1]),rotate(HGL_RAD_90,0,1,0));
        render_root.Add(db->CreateRenderableInstance(m3d.pipeline,m3d.material_instance,ro_plane_grid[2]),rotate(HGL_RAD_90,1,0,0));

        render_root.RefreshMatrix();
        render_root.ExpendToList(&render_list);

        return(true);
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

        pgci.color.Set(0.75,0,0,1);
        pgci.side_color.Set(1,0,0,1);

        ro_plane_grid[0]=CreateRenderablePlaneGrid(db,m3d.material,&pgci);

        pgci.color.Set(0,0.75,0,1);
        pgci.side_color.Set(0,1,0,1);

        ro_plane_grid[1]=CreateRenderablePlaneGrid(db,m3d.material,&pgci);

        pgci.color.Set(0,0,0.75,1);
        pgci.side_color.Set(0,0,1,1);
        ro_plane_grid[2]=CreateRenderablePlaneGrid(db,m3d.material,&pgci);

        {
            struct RoundRectangleCreateInfo rrci;

            rrci.scope.Set(SCREEN_WIDTH-30,10,20,20);
            rrci.radius=5;
            rrci.round_per=5;

            ro_round_rectangle=CreateRenderableRoundRectangle(db,m2d.material,&rrci);
        }

        camera.eye.Set(200,200,200,1.0);
    }

public:

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitMDP(&m3d,PRIM_LINES,OS_TEXT("res/shader/PositionColor3D.vert"),
                                    OS_TEXT("res/shader/VertexColor.frag")))
            return(false);

        if(!InitMDP(&m2d,PRIM_TRIANGLE_FAN, OS_TEXT("res/shader/OnlyPosition.vert"),
                                            OS_TEXT("res/shader/FlatColor.frag")))
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
