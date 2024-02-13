// BlinnPhong direction light

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

class TestApp:public SceneAppFramework
{
    Color4f color;

    DeviceBuffer *ubo_color=nullptr;

private:

    Material *          mtl_vertex_lum      =nullptr;
    MaterialInstance *  mi_plane_grid       =nullptr;

    Pipeline *          p_line              =nullptr;

    Primitive *         ro_plane_grid       =nullptr;

    VBO *               vbo_pos             =nullptr;

private:

    bool InitVertexLumMP()
    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"VertexLuminance3D",Prim::Lines);

        cfg.local_to_world=true;

        mtl_vertex_lum=db->LoadMaterial("Std3D/VertexLum3D",&cfg);
        if(!mtl_vertex_lum)return(false);

        mi_plane_grid=db->CreateMaterialInstance(mtl_vertex_lum,nullptr,&white_color);
        if(!mi_plane_grid)return(false);

        p_line=CreatePipeline(mtl_vertex_lum,InlinePipeline::Solid3D,Prim::Lines);

        if(!p_line)
            return(false);

        return(true);
    }
    
    Renderable *Add(Primitive *r,MaterialInstance *mi)
    {
        Renderable *ri=db->CreateRenderable(r,mi,p_line);

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

            pgci.grid_size.Set(32,32);
            pgci.sub_count.Set(8,8);

            pgci.lum=0.5;
            pgci.sub_lum=0.75;

            ro_plane_grid=CreatePlaneGrid(db,mtl_vertex_lum->GetDefaultVIL(),&pgci);
        }

        return(true);
    }

    bool InitScene()
    {
        Add(ro_plane_grid,mi_plane_grid);

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

        if(!InitVertexLumMP())
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
