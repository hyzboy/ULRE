// RayPicking

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

    Material *          material            =nullptr;
    MaterialInstance *  mi_plane_grid       =nullptr;
    MaterialInstance *  mi_line             =nullptr;

    Pipeline *          pipeline            =nullptr;

    VertexDataManager * vdm                 =nullptr;
    PrimitiveCreater *  prim_creater        =nullptr;

    Primitive *         prim_plane_grid     =nullptr;

    Primitive *         prim_line           =nullptr;

    VAB *               vab_pos             =nullptr;

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
    
    bool InitVDMAndPC()
    {
        vdm=new VertexDataManager(device,material->GetDefaultVIL());
        if(!vdm->Init(  1024*1024,          //VAB最大容量
                        1024*1024,          //索引最大容量
                        IndexType::U16))    //索引类型
        {
            delete vdm;
            vdm=nullptr;

            return(false);
        }

        prim_creater=new PrimitiveCreater(vdm);

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

            pgci.grid_size.Set(32,32);
            pgci.sub_count.Set(8,8);

            pgci.lum=0.5;
            pgci.sub_lum=0.75;

            prim_plane_grid=CreatePlaneGrid(prim_creater,&pgci);
        }

        {
            if(!prim_creater->Init("Line",2))
                return(false);
            
            if(!prim_creater->WriteVAB(VAN::Position, VF_V3F,position_data))return(false);
            if(!prim_creater->WriteVAB(VAN::Luminance,VF_V1F,lumiance_data))return(false);

            prim_line=prim_creater->Create();

            prim_line->Getv
        }

        return(true);
    }

    bool InitScene()
    {
        Add(prim_plane_grid,mi_plane_grid);
        Add(prim_line,mi_line);

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
        SAFE_CLEAR(prim_creater)
        SAFE_CLEAR(vdm)
    }

    bool Init(uint w,uint h)
    {        
        if(!SceneAppFramework::Init(w,h))
            return(false);

        if(!InitMaterialAndPipeline())
            return(false);

        if(!InitVDMAndPC())
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

        vab_pos->Write(&pos,3*sizeof(float));                   //更新VAB上这个点的位置

        SceneAppFramework::BuildCommandBuffer(index);
    }
};//class TestApp:public CameraAppFramework

int main(int,char **)
{
    return RunApp<TestApp>(1280,720);
}
