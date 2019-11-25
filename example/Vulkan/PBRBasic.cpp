// 10.PBR Basic

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/SceneDB.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

class TestApp:public CameraAppFramework
{
private:

    SceneNode   render_root;
    RenderList  render_list;

    struct MDP
    {
        vulkan::Material *      material        =nullptr;
        vulkan::DescriptorSets *descriptor_sets =nullptr;
        vulkan::Pipeline *      pipeline        =nullptr;
    }mdp_line;

    vulkan::Renderable          *ro_plane_grid;

private:

    bool InitMaterial(MDP *mdp,const OSString &vs,const OSString &fs)
    {
        mdp->material=shader_manage->CreateMaterial(vs,fs);
        
        if(!mdp->material)
            return(false);

        mdp->descriptor_sets=mdp->material->CreateDescriptorSets();

        db->Add(mdp->material);
        db->Add(mdp->descriptor_sets);
        return(true);
    }

    bool InitUBO(MDP *mdp)
    {
        if(!InitCameraUBO(mdp->descriptor_sets,mdp->material->GetUBO("world")))
            return(false);

        mdp->descriptor_sets->Update();
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
        render_root.Add(db->CreateRenderableInstance(mdp_line.pipeline,mdp_line.descriptor_sets,ro_plane_grid));

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

        pgci.color.Set(0.5,0.5,0.5,1);
        pgci.side_color.Set(1,0,0,1);

        ro_plane_grid=CreateRenderablePlaneGrid(db,mdp_line.material,&pgci);
    }

public:

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitMDP(&mdp_line,
                    PRIM_LINES,
                    OS_TEXT("res/shader/PositionColor3D.vert.spv"),
                    OS_TEXT("res/shader/FlatColor.frag.spv")))
            return(false);

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
