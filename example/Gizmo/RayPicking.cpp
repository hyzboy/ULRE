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

    Material *          mtl_plane_grid      =nullptr;
    MaterialInstance *  mi_plane_grid       =nullptr;
    Pipeline *          pipeline_plane_grid =nullptr;
    Primitive *         prim_plane_grid     =nullptr;

    Material *          mtl_line            =nullptr;
    MaterialInstance *  mi_line             =nullptr;
    Pipeline *          pipeline_line       =nullptr;
    Primitive *         prim_line           =nullptr;
    VABMap *            prim_line_vab_map   =nullptr;

    Ray                 ray;

private:

    bool InitMaterialAndPipeline()
    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"VertexLuminance2D",Prim::Lines);

        cfg.local_to_world=true;

        {            
            cfg.mtl_name="VertexLuminance2D";       //注意必须用不同名字，未来改名材质文件名+cfg hash名
            cfg.position_format=VAT_VEC2;

            mtl_plane_grid=db->LoadMaterial("Std3D/VertexLum3D",&cfg);
            if(!mtl_plane_grid)return(false);
       
            mi_plane_grid=db->CreateMaterialInstance(mtl_plane_grid,nullptr,&white_color);
            if(!mi_plane_grid)return(false);

            pipeline_plane_grid=CreatePipeline(mtl_plane_grid,InlinePipeline::Solid3D,Prim::Lines);        
            if(!pipeline_plane_grid)return(false);
        }

        {
            cfg.mtl_name="VertexLuminance3D";       //注意必须用不同名字，未来改名材质文件名+cfg hash名
            cfg.position_format=VAT_VEC3;

            mtl_line=db->LoadMaterial("Std3D/VertexLum3D",&cfg);
            if(!mtl_line)return(false);

            mi_line=db->CreateMaterialInstance(mtl_line,nullptr,&yellow_color);
            if(!mi_line)return(false);

            pipeline_line=CreatePipeline(mtl_line,InlinePipeline::Solid3D,Prim::Lines);

            if(!pipeline_line)
                return(false);
        }

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
        
        {
            PrimitiveCreater pc(device,mtl_plane_grid->GetDefaultVIL());

            struct PlaneGridCreateInfo pgci;

            pgci.grid_size.Set(32,32);
            pgci.sub_count.Set(8,8);

            pgci.lum=0.5;
            pgci.sub_lum=0.75;

            prim_plane_grid=CreatePlaneGrid(&pc,&pgci);
        }

        {
            PrimitiveCreater pc(device,mtl_line->GetDefaultVIL());

            if(!pc.Init("Line",2))
                return(false);
            
            if(!pc.WriteVAB(VAN::Position, VF_V3F,position_data))return(false);
            if(!pc.WriteVAB(VAN::Luminance,VF_V1F,lumiance_data))return(false);

            prim_line=pc.Create();

            prim_line_vab_map=prim_line->GetVABMap(VAN::Position);
        }

        return(true);
    }

    bool InitScene()
    {
        Add(prim_plane_grid,mi_plane_grid,pipeline_plane_grid);
        Add(prim_line,mi_line,pipeline_line);

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
        SAFE_CLEAR(prim_line);
    }

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

        prim_line_vab_map->Write(&pos,          //更新VAB上这个点的位置
                                 1);            //这里的1代表的数据数量,不是字节数           

        SceneAppFramework::BuildCommandBuffer(index);
    }
};//class TestApp:public CameraAppFramework

int main(int,char **)
{
    return RunApp<TestApp>(1280,720);
}
