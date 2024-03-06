﻿// BlinnPhong direction light

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

static float lumiance_data[2]={1,1};

static Color4f white_color(1,1,1,1);

class TestApp:public SceneAppFramework
{
private:    //plane grid

    Material *          mtl_vertex_lum      =nullptr;
    MaterialInstance *  mi_plane_grid       =nullptr;
    Pipeline *          p_line              =nullptr;
    Primitive *         prim_plane_grid     =nullptr;

private:    //sphere

    Material *          mtl_sun_light       =nullptr;
    MaterialInstance *  mi_sphere           =nullptr;
    Pipeline *          p_sphere            =nullptr;
    Primitive *         prim_sphere         =nullptr;

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

    bool InitBlinnPhongSunLightMP()
    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"BlinnPhong3D",Prim::Triangles);

        cfg.local_to_world=true;

        mtl_sun_light=db->LoadMaterial("Std3D/BlinnPhong/SunLightPureColor",&cfg);
        if(!mtl_sun_light)return(false);

        mtl_sun_light->BindUBO(DescriptorSetType::Global,"sun",sun_data,sizeof(sun_data));      //恢复SUN.UBO或是使用内嵌SUN定义也行，
                                                                                                //恢复SUN.UBO的意义是让引擎支持读取外部UBO配置文件

        mi_sphere=db->CreateMaterialInstance(mtl_sun_light);
        if(!mi_sphere)return(false);

        p_sphere=CreatePipeline(mtl_sun_light,InlinePipeline::Solid3D,Prim::Triangles);

        if(!p_sphere)
            return(false);

        return(true);        
    }
    
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

        //Plane Grid
        {
            struct PlaneGridCreateInfo pgci;

            pgci.grid_size.Set(32,32);
            pgci.sub_count.Set(8,8);

            pgci.lum=0.5;
            pgci.sub_lum=0.75;

            prim_plane_grid=CreatePlaneGrid(db,mtl_vertex_lum->GetDefaultVIL(),&pgci);
        }

        //Sphere
        {
            prim_sphere=CreateSphere(db,mi_sphere->GetVIL(),16);
        }

        return(true);
    }

    bool InitScene()
    {
        Add(prim_plane_grid,mi_plane_grid,p_line);
        Add(prim_sphere,mi_sphere,p_sphere);

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

        if(!InitBlinnPhongSunLightMP())
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