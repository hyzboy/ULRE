// Metric Cells Grid

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/VKRenderablePrimitiveCreater.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>

using namespace hgl;
using namespace hgl::graph;

struct MetricCellsGridData
{
    Color4f x_color;
    Color4f y_color;
    Color4f x_axis_color;
    Color4f y_axis_color;
    Color4f center_color;

    Vector2f lum;
    Vector2f cell_step;
    Vector2f big_cell_step;
    Vector2f scale;

    float axis_line_width;
    float center_radius;
};

constexpr const size_t MCG_SIZE=sizeof(MetricCellsGridData);

constexpr const float PLANE_SIZE=1024;

class TestApp:public SceneAppFramework
{
private:

    MetricCellsGridData mcg_data;

    Material *          material            =nullptr;
    Pipeline *          pipeline            =nullptr;

    Primitive *         ro_plane            =nullptr;
    MaterialInstance *  material_instance   =nullptr;

private:

    bool InitMDP()
    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"MetricCellsGrid",Prim::Fan);

        cfg.local_to_world=true;

        material=db->LoadMaterial("Std3D/MetricCellsGrid",&cfg);
        if(!material)return(false);

        {
            mcg_data.x_color=Color4f(1,1,1,1);
            mcg_data.y_color=Color4f(1,1,1,1);

            mcg_data.x_axis_color=GetColor4f(COLOR::BlenderAxisRed,     1.0);
            mcg_data.y_axis_color=GetColor4f(COLOR::BlenderAxisGreen,   1.0);

            mcg_data.center_color=Color4f(1,1,0,1);

            mcg_data.lum            =Vector2f(0.1,0.2);
            mcg_data.cell_step      =Vector2f(8,8);
            mcg_data.big_cell_step  =Vector2f(32,32);
            mcg_data.scale          =Vector2f(PLANE_SIZE,PLANE_SIZE);

            mcg_data.axis_line_width=1.0;
            mcg_data.center_radius  =4.0;
        }

        material_instance=db->CreateMaterialInstance(material,nullptr,&mcg_data);
        
        pipeline=CreatePipeline(material,InlinePipeline::Solid3D,Prim::Fan);

        return pipeline;
    }

    bool CreateRenderObject()
    {
        ro_plane=inline_geometry::CreatePlane(db,material->GetDefaultVIL());

        return ro_plane;
    }

    Renderable *Add(MaterialInstance *mi,const Matrix4f &mat)
    {
        Renderable *ri=db->CreateRenderable(ro_plane,mi,pipeline);

        if(!ri)
            return(nullptr);

        render_root.CreateSubNode(mat,ri);

        return ri;
    }

    bool InitScene()
    {
        Add(material_instance,scale(PLANE_SIZE,PLANE_SIZE,1));

        camera->pos=Vector3f(PLANE_SIZE/2,PLANE_SIZE/2,PLANE_SIZE/2);
        camera_control->SetTarget(Vector3f(0,0,0));
        camera_control->Refresh();

//        camera_control->SetReserveDirection(true,true);        //反转x,y

        render_root.RefreshMatrix();
        render_list->Expend(&render_root);

        return(true);
    }

public:

    bool Init(uint width,uint height) override
    {
        if(!SceneAppFramework::Init(width,height))
            return(false);

        if(!InitMDP())
            return(false);

        if(!CreateRenderObject())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }
};//class TestApp:public CameraAppFramework

int main(int,char **)
{
    return RunApp<TestApp>(1280,720);
}
