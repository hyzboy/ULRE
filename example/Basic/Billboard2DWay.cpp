// Billboard : The 2D way

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/Ray.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VertexDataManager.h>

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

    Material *          mtl_plane_grid      =nullptr;
    MaterialInstance *  mi_plane_grid       =nullptr;
    Pipeline *          pipeline_plane_grid =nullptr;
    Primitive *         prim_plane_grid     =nullptr;

    Material *          mtl_billboard       =nullptr;
    MaterialInstance *  mi_billboard        =nullptr;
    Pipeline *          pipeline_billboard  =nullptr;
    Primitive *         prim_billboard      =nullptr;

    Ray                 ray;

private:

    bool InitPlaneGridMP()
    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"VertexLuminance3D",Prim::Lines);

        cfg.local_to_world=true;

        {
            cfg.position_format=VAT_VEC2;

            mtl_plane_grid=db->LoadMaterial("Std3D/VertexLum3D",&cfg);
            if(!mtl_plane_grid)return(false);
       
            mi_plane_grid=db->CreateMaterialInstance(mtl_plane_grid,nullptr,&white_color);
            if(!mi_plane_grid)return(false);

            pipeline_plane_grid=CreatePipeline(mtl_plane_grid,InlinePipeline::Solid3D,Prim::Lines);
            if(!pipeline_plane_grid)return(false);
        }

        return(true);
    }

    bool InitBillboardMP()
    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"Billboard2DWay",Prim::TriangleStrip);

        cfg.local_to_world=true;

        {
            mtl_billboard=db->LoadMaterial("Std3D/Billboard/2DWay",&cfg);
            if(!mtl_billboard)return(false);
       
            mi_billboard=db->CreateMaterialInstance(mtl_billboard,nullptr,&yellow_color);
            if(!mi_billboard)return(false);

            pipeline_billboard=CreatePipeline(mtl_billboard,InlinePipeline::Solid3D,Prim::Billboard2DWay);
            if(!pipeline_billboard)return(false);
        }

        return(true);
    
    Renderable *Add(Primitive *r,MaterialInstance *mi,Pipeline *p)
    {
        Renderable *ri=db->CreateRenderable(r,mi,p);

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
            PrimitiveCreater pc(device,mtl_plane_grid->GetDefaultVIL());

            struct PlaneGridCreateInfo pgci;

            pgci.grid_size.Set(32,32);
            pgci.sub_count.Set(8,8);

            pgci.lum=0.5;
            pgci.sub_lum=0.75;

            prim_plane_grid=CreatePlaneGrid(&pc,&pgci);
        }

        return(true);
    }

    bool InitScene()
    {
        Add(prim_plane_grid,mi_plane_grid,pipeline_plane_grid);

        camera->pos=Vector3f(32,32,32);
        camera_control->SetTarget(Vector3f(0,0,0));
        camera_control->Refresh();

        render_root.RefreshMatrix();
        render_list->Expend(&render_root);

        return(true);
    }

public:

    ~TestApp()
    {
        SAFE_CLEAR(prim_plane_grid);
    }

    bool Init(uint w,uint h)
    {        
        if(!SceneAppFramework::Init(w,h))
            return(false);

        if(!InitPlaneGridMP())
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

        SceneAppFramework::BuildCommandBuffer(index);
    }
};//class TestApp:public CameraAppFramework

int main(int,char **)
{
    return RunApp<TestApp>(1280,720);
}
