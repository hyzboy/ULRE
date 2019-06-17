// 7.InlineGeometryScene
//  全内置几何体场景

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

    vulkan::Material *          material            =nullptr;
    vulkan::DescriptorSets *    descriptor_sets     =nullptr;

    vulkan::Renderable          *ro_plane_grid,
                                *ro_cube,
                                *ro_sphere,
                                *ro_dome,
                                *ro_torus,
                                *ro_cylinder,
                                *ro_cone;

    vulkan::Pipeline            *pipeline_line      =nullptr,
                                *pipeline_solid     =nullptr,
                                *pipeline_twoside   =nullptr;

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("OnlyPosition3D.vert.spv"),
                                               OS_TEXT("FlatColor.frag.spv"));
        if(!material)
            return(false);

        descriptor_sets=material->CreateDescriptorSets();

        db->Add(material);
        db->Add(descriptor_sets);
        return(true);
    }

    void CreateRenderObject()
    {
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

            ro_plane_grid=CreateRenderablePlaneGrid(db,material,&pgci);
        }
        
        {
            struct CubeCreateInfo cci;            
            ro_cube=CreateRenderableCube(db,material,&cci);
        }
        
        {        
            ro_sphere=CreateRenderableSphere(db,material,16);
        }

        {
            DomeCreateInfo dci;

            dci.radius=100;
            dci.numberSlices=32;

            ro_dome=CreateRenderableDome(db,material,&dci);
        }

        {
            TorusCreateInfo tci;

            tci.innerRadius=50;
            tci.outerRadius=70;

            tci.numberSlices=32;
            tci.numberStacks=16;

            ro_torus=CreateRenderableTorus(db,material,&tci);
        }

        {
            CylinderCreateInfo cci;

            cci.halfExtend=10;
            cci.radius=10;
            cci.numberSlices=16;

            ro_cylinder=CreateRenderableCylinder(db,material,&cci);
        }

        {
            ConeCreateInfo cci;

            cci.halfExtend=10;
            cci.radius=10;
            cci.numberSlices=16;
            cci.numberStacks=1;

            ro_cone=CreateRenderableCone(db,material,&cci);
        }
    }

    bool InitUBO()
    {
        if(!InitCameraUBO(descriptor_sets,material->GetUBO("world")))
            return(false);

        descriptor_sets->Update();
        return(true);
    }

    bool InitPipeline()
    {
        vulkan::PipelineCreater *pipeline_creater=new vulkan::PipelineCreater(device,material,device->GetRenderPass(),device->GetExtent());
        pipeline_creater->SetDepthTest(true);
        pipeline_creater->SetDepthWrite(true);
        pipeline_creater->Set(PRIM_LINES);

        pipeline_line=pipeline_creater->Create();
        db->Add(pipeline_line);

        pipeline_creater->Set(PRIM_TRIANGLES);
        pipeline_creater->SetPolygonMode(VK_POLYGON_MODE_LINE);
        pipeline_solid=pipeline_creater->Create();
        db->Add(pipeline_solid);

        pipeline_creater->SetCullMode(VK_CULL_MODE_NONE);
        pipeline_twoside=pipeline_creater->Create();
        db->Add(pipeline_twoside);

        delete pipeline_creater;

        if(!pipeline_line)
            return(false);

        if(!pipeline_solid)
            return(false);

        return(true);
    }

    bool InitScene()
    {
        render_root.Add(db->CreateRenderableInstance(pipeline_line,descriptor_sets,ro_plane_grid));
        render_root.Add(db->CreateRenderableInstance(pipeline_twoside,descriptor_sets,ro_dome));
        render_root.Add(db->CreateRenderableInstance(pipeline_twoside,descriptor_sets,ro_torus));
        render_root.Add(db->CreateRenderableInstance(pipeline_solid,descriptor_sets,ro_cube     ),translate(-10,  0, 5)*scale(10,10,10));
        render_root.Add(db->CreateRenderableInstance(pipeline_solid,descriptor_sets,ro_sphere   ),translate( 10,  0, 5)*scale(10,10,10));
        render_root.Add(db->CreateRenderableInstance(pipeline_solid,descriptor_sets,ro_cylinder ),translate(  0, 16, 0));
        render_root.Add(db->CreateRenderableInstance(pipeline_solid,descriptor_sets,ro_cone     ),translate(  0,-16, 0));

        render_root.RefreshMatrix();
        render_root.ExpendToList(&render_list);

        BuildCommandBuffer(&render_list);
        return(true);
    }

public:

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitMaterial())
            return(false);

        CreateRenderObject();

        if(!InitUBO())
            return(false);

        if(!InitPipeline())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }

    void Resize(int,int)override
    {
        BuildCommandBuffer(&render_list);
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
