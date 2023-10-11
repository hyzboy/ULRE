// 18.RayPicking

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/Ray.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>

using namespace hgl;
using namespace hgl::graph;

static float position_data[2][3]=
{
    {100,100,100},
    {0,0,0}
};

static float lumiance_data[2]={1,1};

static Color4f white_color(1,1,1,1);
static Color4f yellow_color(1,1,0,1);

class TestApp:public SceneAppFramework
{
    Color4f color;

    DeviceBuffer *ubo_color=nullptr;

private:

    Material *          material            =nullptr;
    MaterialInstance *  mi_plane_grid       =nullptr;
    MaterialInstance *  mi_line             =nullptr;

    Pipeline *          pipeline            =nullptr;

    Primitive *         ro_plane_grid       =nullptr;

    Primitive *         ro_line             =nullptr;

    VBO *               vbo_pos             =nullptr;

    Ray                 ray;

private:

    bool InitMaterialAndPipeline()
    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"VertexLuminance3D",Prim::Lines);

        cfg.local_to_world=true;

        material=db->LoadMaterial("Std3D/VertexLum3D",&cfg);
        if(!material)return(false);

        mi_plane_grid=db->CreateMaterialInstance(material,nullptr,&white_color);
        if(!mi_plane_grid)return(false);

        mi_line=db->CreateMaterialInstance(material,nullptr,&yellow_color);
        if(!mi_line)return(false);

        pipeline=CreatePipeline(material,InlinePipeline::Solid3D,Prim::Lines);

        if(!pipeline)
            return(false);

        return(true);
    }
    
    Renderable *Add(Primitive *r,MaterialInstance *mi)
    {
        Renderable *ri=db->CreateRenderable(r,mi,pipeline);

        if(!ri)
        {
            LOG_ERROR(OS_TEXT("Create Renderable failed."));
            return(nullptr);
        }

        render_root.CreateSubNode(ri);

        return ri;
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        {
            struct PlaneGridCreateInfo pgci;

            pgci.grid_size.width =32;
            pgci.grid_size.height=32;

            pgci.sub_count.width =8;
            pgci.sub_count.height=8;

            pgci.lum=0.5;
            pgci.sub_lum=0.75;

            ro_plane_grid=CreatePlaneGrid(db,material->GetDefaultVIL(),&pgci);
        }

        {
            ro_line=db->CreatePrimitive("Line",2);
            if(!ro_line)return(false);
            
            if(!ro_line->Set(VAN::Position,  vbo_pos=   db->CreateVBO(VF_V3F,2,position_data    )))return(false);
            if(!ro_line->Set(VAN::Luminance,            db->CreateVBO(VF_V1F,2,lumiance_data    )))return(false);
        }

        return(true);
    }

    bool InitScene()
    {
        Add(ro_plane_grid,mi_plane_grid);
        Add(ro_line,mi_line);

        camera->pos=Vector3f(32,32,32);
        camera_control->SetTarget(Vector3f(0,0,0));
        camera_control->Refresh();

        render_root.RefreshMatrix();
        render_list->Expend(&render_root);

        return(true);
    }

public:

    bool Init(uint w,uint h)
    {        
        if(!SceneAppFramework::Init(w,h))
            return(false);

        if(!InitMaterialAndPipeline())
            return(false);

        if(!CreateRenderObject())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }

    void BuildCommandBuffer(uint32 index) override
    {
        const CameraInfo &ci=GetCameraInfo();
        const ViewportInfo &vi=GetViewportInfo();

        ray.Set(GetMouseCoord(),&ci,&vi);                       //设置射线查询的屏幕坐标点

        const Vector3f pos=ray.ClosestPoint(Vector3f(0,0,0));   //求射线上与点(0,0,0)最近的点的坐标

        vbo_pos->Write(&pos,3*sizeof(float));                   //更新VBO上这个点的位置

        SceneAppFramework::BuildCommandBuffer(index);
    }
};//class TestApp:public CameraAppFramework

int main(int,char **)
{
    return RunApp<TestApp>(1280,720);
}
