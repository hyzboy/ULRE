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
#include<hgl/graph/mtl/BlinnPhong.h>
#include<hgl/color/Color.h>

using namespace hgl;
using namespace hgl::graph;

static float lumiance_data[2]={1,1};

static Color4f white_color(1,1,1,1);

static mtl::blinnphong::SunLight sun_light=
{
    Vector4f(1,1,1,0),      //direction
    Vector4f(1,0.95,0.9,1)  //color
};

constexpr const COLOR AxisColor[4]={COLOR::Red,COLOR::Green,COLOR::Blue,COLOR::White};

class TestApp:public SceneAppFramework
{
private:    //plane grid

    Material *          mtl_vertex_lum      =nullptr;
    MaterialInstance *  mi_plane_grid       =nullptr;
    Pipeline *          p_line              =nullptr;
    Primitive *         prim_plane_grid     =nullptr;

private:

    DeviceBuffer *      ubo_sun             =nullptr;

private:    //sphere

    Material *          mtl_blinnphong      =nullptr;
    MaterialInstance *  mi_blinnphong[4]{};
    Pipeline *          p_blinnphong        =nullptr;

    Primitive *         prim_sphere         =nullptr;
    Primitive *         prim_cone           =nullptr;
    Primitive *         prim_cylinder       =nullptr;

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

    bool CreateBlinnPhongUBO()
    {
        ubo_sun=db->CreateUBO("sun",sizeof(sun_light),&sun_light);
        if(!ubo_sun)return(false);

        return(true);
    }

    bool InitBlinnPhongSunLightMP()
    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"BlinnPhong3D",Prim::Triangles);

        cfg.local_to_world=true;

        mtl_blinnphong=db->LoadMaterial("Std3D/BlinnPhong/SunLightPureColor",&cfg);
        if(!mtl_blinnphong)return(false);

        mtl_blinnphong->BindUBO(DescriptorSetType::Global,"sun",ubo_sun);
        mtl_blinnphong->Update();

        Color4f mi_data;
        for(uint i=0;i<4;i++)
        {
            mi_data=GetColor4f(AxisColor[i],4);

            mi_blinnphong[i]=db->CreateMaterialInstance(mtl_blinnphong,nullptr,&mi_data);
            if(!mi_blinnphong[i])return(false);
        }

        p_blinnphong=CreatePipeline(mtl_blinnphong,InlinePipeline::Solid3D,Prim::Triangles);

        if(!p_blinnphong)
            return(false);

        return(true);        
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
        prim_sphere=CreateSphere(db,mi_blinnphong[0]->GetVIL(),16);

        //Cone
        {
            struct ConeCreateInfo cci;

            cci.radius      =1;         //圆锥半径
            cci.halfExtend  =1;         //圆锤一半高度
            cci.numberSlices=16;        //圆锥底部分割数
            cci.numberStacks=8;         //圆锥高度分割数

            prim_cone=CreateCone(db,mi_blinnphong[1]->GetVIL(),&cci);
        }

        //Cyliner
        {
            struct CylinderCreateInfo cci;

            cci.halfExtend  =4;         //圆柱一半高度
            cci.numberSlices=16;        //圆柱底部分割数
            cci.radius      =0.25f;     //圆柱半径

            prim_cylinder=CreateCylinder(db,mi_blinnphong[2]->GetVIL(),&cci);
        }

        return(true);
    }    
    
    Renderable *Add(Primitive *r,MaterialInstance *mi,Pipeline *p,const Matrix4f &mat=Identity4f)
    {
        Renderable *ri=db->CreateRenderable(r,mi,p);

        if(!ri)
        {
            LOG_ERROR(OS_TEXT("Create Renderable failed."));
            return(nullptr);
        }

        render_root.CreateSubNode(mat,ri);

        return ri;
    }

    bool InitScene()
    {
        Add(prim_plane_grid,mi_plane_grid,p_line,Identity4f);

        Add(prim_sphere,    mi_blinnphong[0],p_blinnphong,translate(Vector3f(0,0,2)));

        Add(prim_cone,      mi_blinnphong[1],p_blinnphong);
        Add(prim_cylinder,  mi_blinnphong[2],p_blinnphong,translate(Vector3f(0,0,-5)));

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
        
        if(!CreateBlinnPhongUBO())
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
