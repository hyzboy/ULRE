// 7.InlineGeometryScene
//  全内置几何体场景

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderableInstance.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

class TestApp:public CameraAppFramework
{
    Color4f color;

private:

    SceneNode   render_root;
    RenderList  render_list;
    
    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;

    Material *          axis_material       =nullptr;
    MaterialInstance *  axis_mi             =nullptr;

    PipelineData *      pipeline_data       =nullptr;
    Pipeline *          axis_pipeline       =nullptr;
    Pipeline *          pipeline_solid      =nullptr;

    GPUBuffer *         ubo_color           =nullptr;

    Renderable          *ro_axis,
                        *ro_cube,
                        *ro_sphere,
                        *ro_dome,
                        *ro_torus,
                        *ro_cylinder,
                        *ro_cone;

private:

    bool InitMaterial()
    {
        axis_material=db->CreateMaterial(OS_TEXT("res/material/VertexColor3D"));
        if(!axis_material)return(false);

        axis_mi=db->CreateMaterialInstance(axis_material);
        if(!axis_mi)return(false);

        axis_pipeline=CreatePipeline(axis_material,InlinePipeline::Solid3D,Prim::Lines);
        if(!axis_pipeline)return(false);

        material=db->CreateMaterial(OS_TEXT("res/material/DebugVertexNormal"));
        if(!material)return(false);

        material_instance=db->CreateMaterialInstance(material);
        if(!material_instance)return(false);

        pipeline_data=GetPipelineData(InlinePipeline::Solid3D);
        if(!pipeline_data)return(false);


        pipeline_solid=CreatePipeline(material,pipeline_data,Prim::Triangles);
        if(!pipeline_solid)return(false);

        return(true);
    }

    void CreateRenderObject()
    {
        {
            struct AxisCreateInfo aci;

            aci.size=200;

            ro_axis=CreateRenderableAxis(db,axis_material,&aci);
        }
        
        {
            struct CubeCreateInfo cci;
            cci.has_color=true;
            cci.color.Set(1,1,1,1);
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
        color.Set(1,1,1,1);
        
        ubo_color=device->CreateUBO(sizeof(Vector4f),&color);

        db->Add(ubo_color);

        material_instance->BindUBO("color_material",ubo_color);

        if(!material_instance->BindUBO("world",GetCameraMatrixBuffer()))
            return(false);

        material_instance->Update();

        if(!axis_mi->BindUBO("world",GetCameraMatrixBuffer()))
            return(false);

        axis_mi->Update();
        return(true);
    }
    
    void Add(Renderable *r,Pipeline *pl)
    {
        auto ri=db->CreateRenderableInstance(r,material_instance,pl);

        render_root.Add(ri);
    }

    void Add(Renderable *r,Pipeline *pl,const Matrix4f &mat)
    {
        auto ri=db->CreateRenderableInstance(r,material_instance,pl);

        render_root.Add(ri,mat);
    }

    bool InitScene()
    {
        render_root.Add(db->CreateRenderableInstance(ro_axis,axis_mi,axis_pipeline));
//        Add(ro_dome,pipeline_solid);
        Add(ro_torus    ,pipeline_solid);
        Add(ro_cube     ,pipeline_solid,translate(-10,  0, 5)*scale(10,10,10));
        Add(ro_sphere   ,pipeline_solid,translate( 10,  0, 5)*scale(10,10,10));
        Add(ro_cylinder ,pipeline_solid,translate(  0, 16, 0));
        Add(ro_cone     ,pipeline_solid,translate(  0,-16, 0));

        render_root.RefreshMatrix();
        render_root.ExpendToList(&render_list);

        return(true);
    }

public:

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);
            
        if(!InitMaterial())
            return(false);

        if(!InitUBO())
            return(false);

        CreateRenderObject();

        if(!InitScene())
            return(false);

        return(true);
    }
    
    void BuildCommandBuffer(uint32_t index) override
    {
        render_root.RefreshMatrix();
        render_list.Clear();
        render_root.ExpendToList(&render_list);
        
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
