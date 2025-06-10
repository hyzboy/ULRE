// RayPicking

#include<hgl/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/Ray.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/VKVertexInputConfig.h>

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

class TestApp:public WorkObject
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
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);

        cfg.local_to_world=true;

        {
            cfg.position_format=VAT_VEC2;

            mtl_plane_grid=db->LoadMaterial("Std3D/VertexLum3D",&cfg);
            if(!mtl_plane_grid)return(false);
       
            VILConfig vil_config;

            vil_config.Add(VAN::Luminance,VF_V1UN8);

            mi_plane_grid=db->CreateMaterialInstance(mtl_plane_grid,&vil_config,&white_color);
            if(!mi_plane_grid)return(false);

            pipeline_plane_grid=CreatePipeline(mi_plane_grid,InlinePipeline::Solid3D,PrimitiveType::Lines);
            if(!pipeline_plane_grid)return(false);
        }

        {
            cfg.position_format=VAT_VEC3;

            mtl_line=db->LoadMaterial("Std3D/VertexLum3D",&cfg);
            if(!mtl_line)return(false);

            mi_line=db->CreateMaterialInstance(mtl_line,nullptr,&yellow_color);
            if(!mi_line)return(false);

            pipeline_line=CreatePipeline(mtl_line,InlinePipeline::Solid3D,PrimitiveType::Lines);

            if(!pipeline_line)
                return(false);
        }

        return(true);
    }
    
    Mesh *Add(SceneNode *parent_node,Primitive *r,MaterialInstance *mi,Pipeline *p)
    {
        Mesh *ri=db->CreateMesh(r,mi,p);

        if(!ri)
        {
            LOG_ERROR(OS_TEXT("Create Mesh failed."));
            return(nullptr);
        }

        parent_node->Add(new SceneNode(ri));

        return ri;
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;
        
        {
            auto pc=GetPrimitiveCreater(mi_plane_grid);

            struct PlaneGridCreateInfo pgci;

            pgci.grid_size.Set(32,32);
            pgci.sub_count.Set(8,8);

            pgci.lum=128;
            pgci.sub_lum=196;

            prim_plane_grid=CreatePlaneGrid2D(pc,&pgci);
        }

        {
            prim_line=CreatePrimitive("RayLine",2,mtl_line->GetDefaultVIL(),
                                    {
                                        {VAN::Position, VF_V3F,position_data},
                                        {VAN::Luminance,VF_V1F,lumiance_data}
                                    });

            prim_line_vab_map=prim_line->GetVABMap(VAN::Position);
        }

        return(true);
    }

    bool InitScene()
    {
        SceneNode *scene_root=GetSceneRoot();       //取得缺省场景根节点

        Add(scene_root,prim_plane_grid,mi_plane_grid,pipeline_plane_grid);
        Add(scene_root,prim_line,mi_line,pipeline_line);

        CameraControl *camera_control=GetCameraControl();

        camera_control->SetPosition(Vector3f(32,32,32));
        camera_control->SetTarget(Vector3f(0,0,0));

        return(true);
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(prim_plane_grid);
    }

    bool Init() override
    {
        if(!InitMaterialAndPipeline())
            return(false);

        if(!CreateRenderObject())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }

    void Tick(double) override
    {
        Vector2i mouse_position;

        if(!GetMouseCoord(&mouse_position))
            return;

        CameraControl *camera_control=GetCameraControl();

        const CameraInfo *ci=camera_control->GetCameraInfo();
        const ViewportInfo *vi=camera_control->GetViewportInfo();

        ray.Set(mouse_position,ci,vi);                       //设置射线查询的屏幕坐标点

        const Vector3f pos=ray.ClosestPoint(Vector3f(0,0,0));   //求射线上与点(0,0,0)最近的点的坐标

        prim_line_vab_map->Write(&pos,          //更新VAB上这个点的位置
                                 1);            //这里的1代表的数据数量,不是字节数
    }
};//class TestApp:public CameraAppFramework

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("RayPicking"),1280,720);
}
