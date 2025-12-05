// RayPicking

#include<hgl/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/RenderCollector.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/math/geometry/Ray.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/component/PrimitiveComponent.h>

using namespace hgl;
using namespace hgl::graph;

static float position_data[2][3]=
{
    {100,100,100},
    {0,0,0}
};

static uint8 lumiance_data[2]={255,255};

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
    Geometry *         geom_plane_grid     =nullptr;

    Material *          mtl_line            =nullptr;
    MaterialInstance *  mi_line             =nullptr;
    Pipeline *          pipeline_line       =nullptr;
    Geometry *         prim_line           =nullptr;
    VABMap *            prim_line_vab_map   =nullptr;

    Ray                 ray;

private:

    bool InitMaterialAndPipeline()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);

        cfg.local_to_world=true;

        VILConfig vil_config;

        vil_config.Add(VAN::Luminance,VF_V1UN8);

        {
            cfg.position_format=VAT_VEC2;

            mtl_plane_grid=LoadMaterial("Std3D/VertexLum3D",&cfg);
            if(!mtl_plane_grid)return(false);

            mi_plane_grid=CreateMaterialInstance(mtl_plane_grid,&vil_config,&white_color);
            if(!mi_plane_grid)return(false);

            pipeline_plane_grid=CreatePipeline(mi_plane_grid,InlinePipeline::Solid3D);
            if(!pipeline_plane_grid)return(false);
        }

        {
            cfg.position_format=VAT_VEC3;

            mtl_line=LoadMaterial("Std3D/VertexLum3D",&cfg);
            if(!mtl_line)return(false);

            mi_line=CreateMaterialInstance(mtl_line,&vil_config,&yellow_color);
            if(!mi_line)return(false);

            pipeline_line=CreatePipeline(mi_line,InlinePipeline::Solid3D);

            if(!pipeline_line)
                return(false);
        }

        return(true);
    }
    
    Primitive *Add(SceneNode *parent_node,Geometry *r,MaterialInstance *mi,Pipeline *p)
    {
        Primitive *ri=CreatePrimitive(r,mi,p);

        if(!ri)
        {
            LogError(OS_TEXT("Create Primitive failed."));
            return(nullptr);
        }

        CreateComponentInfo cci(parent_node);

        CreateComponent<PrimitiveComponent>(&cci,ri);

        return ri;
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;
        
        {
            auto pc=GetGeometryCreater(mi_plane_grid);

            struct PlaneGridCreateInfo pgci;

            pgci.grid_size.Set(32,32);
            pgci.sub_count.Set(8,8);

            pgci.lum=128;
            pgci.sub_lum=196;

            geom_plane_grid=CreatePlaneGrid2D(pc,&pgci);
        }

        {
            prim_line=CreateGeometry("RayLine",2,mi_line->GetVIL(),
                                    {
                                        {VAN::Position, VF_V3F,position_data},
                                        {VAN::Luminance,VF_V1UN8,lumiance_data}
                                    });

            prim_line_vab_map=prim_line->GetVABMap(VAN::Position);
        }

        return(true);
    }

    bool InitScene()
    {
        SceneNode *scene_root=GetWorldRootNode();       //取得缺省场景根节点

        Add(scene_root,geom_plane_grid,mi_plane_grid,pipeline_plane_grid);
        Add(scene_root,prim_line,mi_line,pipeline_line);

        CameraControl *camera_control=GetCameraControl();

        if(camera_control)
        {
            camera_control->SetPosition(Vector3f(32,32,32));
            camera_control->SetTarget(Vector3f(0,0,0));
        }

        return(true);
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(geom_plane_grid);
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

    void Tick(double delta) override
    {
        WorkObject::Tick(delta);

        const Vector2i mouse_position=GetMouseCoord();

        CameraControl *camera_control=GetCameraControl();

        const CameraInfo *ci=camera_control->GetCameraInfo();
        const ViewportInfo *vi=camera_control->GetViewportInfo();

        ray.SetFromViewportPoint(mouse_position,ci,vi->GetViewport());                       //设置射线查询的屏幕坐标点

        const Vector3f pos=ray.ClosestPoint(Vector3f(0,0,0));   //求射线上与点(0,0,0)最近的点的坐标

        prim_line_vab_map->Write(&pos,          //更新VAB上这个点的位置
                                 1);            //这里的1代表的数据数量,不是字节数
    }
};//class TestApp:public CameraAppFramework

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("RayPicking"),1280,720);
}
