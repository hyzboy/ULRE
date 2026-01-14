// RayPicking (ECS Version)
// 该范例主要演示使用ECS架构实现射线拾取功能
// This example demonstrates ray picking using ECS architecture
// 
// 本范例展示了：
// 1. 使用ECS架构创建场景对象（平面网格和射线）
// 2. 使用TransformComponent管理空间变换
// 3. 使用PrimitiveComponent管理渲染图元
// 4. 动态更新顶点数据以显示实时射线

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

// 引入ECS相关头文件
#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

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
private:

    // ECS组件
    ECSContext* ecs_world = nullptr;   // 由 RenderFramework 统一维护
    std::shared_ptr<Entity> plane_grid_entity = nullptr;
    std::shared_ptr<Entity> ray_line_entity = nullptr;

    // 传统渲染资源
    Material *          mtl_plane_grid      =nullptr;
    MaterialInstance *  mi_plane_grid       =nullptr;
    Pipeline *          pipeline_plane_grid =nullptr;
    Geometry *          geom_plane_grid     =nullptr;

    Material *          mtl_line            =nullptr;
    MaterialInstance *  mi_line             =nullptr;
    Pipeline *          pipeline_line       =nullptr;
    Geometry *          geom_line           =nullptr;
    Primitive *         prim_line           =nullptr;
    VABMap *            prim_line_vab_map   =nullptr;

    math::Ray           ray;

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

    bool CreateGeometry()
    {
        using namespace inline_geometry;
        
        // === 创建平面网格几何体 ===
        {
            auto pc=GetGeometryCreater(mi_plane_grid);

            struct PlaneGridCreateInfo pgci;

            pgci.grid_size.Set(32,32);
            pgci.sub_count.Set(8,8);

            pgci.lum=128;
            pgci.sub_lum=196;

            geom_plane_grid=CreatePlaneGrid2D(pc,&pgci);
            
            if(!geom_plane_grid)
                return(false);
        }

        // === 创建射线线段几何体 ===
        {
            geom_line=WorkObject::CreateGeometry("RayLine",2,mi_line->GetVIL(),
                                    {
                                        {VAN::Position, VF_V3F,position_data},
                                        {VAN::Luminance,VF_V1UN8,lumiance_data}
                                    });

            if(!geom_line)
                return(false);

            Add(geom_line);
        }

        return(true);
    }

    bool InitECS()
    {
        // === 步骤1: 获取ECS世界 ===
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        // === 步骤2: 创建平面网格实体 ===
        {
            plane_grid_entity = ecs_world->CreateEntity<Entity>("PlaneGrid");

            // 创建Primitive
            Primitive* prim_plane = CreatePrimitive(geom_plane_grid, mi_plane_grid, pipeline_plane_grid);
            if(!prim_plane)
                return false;

            // 添加TransformComponent
            auto transform = plane_grid_entity->AddComponent<TransformComponent>();
            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);  // 平面网格是静态的

            // 添加PrimitiveComponent
            auto primitive_comp = plane_grid_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            primitive_comp->SetPrimitive(prim_plane);
            primitive_comp->SetVisible(true);
        }

        // === 步骤3: 创建射线线段实体 ===
        {
            ray_line_entity = ecs_world->CreateEntity<Entity>("RayLine");

            // 创建Primitive
            prim_line = CreatePrimitive(geom_line, mi_line, pipeline_line);
            if(!prim_line)
                return false;

            // 获取VABMap用于后续动态更新顶点数据
            prim_line_vab_map = prim_line->GetVABMap(VAN::Position);

            // 添加TransformComponent
            auto transform = ray_line_entity->AddComponent<TransformComponent>();
            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(true);  // 线段会动态更新

            // 添加PrimitiveComponent
            auto primitive_comp = ray_line_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            primitive_comp->SetPrimitive(prim_line);
            primitive_comp->SetVisible(true);
        }

        return true;
    }

    bool InitScene()
    {
        // === 设置相机位置 ===
        CameraControl *camera_control=GetCameraControl();

        if(camera_control)
        {
            camera_control->SetPosition(math::Vector3f(32,32,32));
            camera_control->SetTarget(math::Vector3f(0,0,0));
        }

        return(true);
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(geom_plane_grid);
        SAFE_CLEAR(geom_line);
    }

    bool Init() override
    {
        if(!InitMaterialAndPipeline())
            return(false);

        if(!CreateGeometry())
            return(false);

        if(!InitECS())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }

    void Tick(double delta) override
    {
        WorkObject::Tick(delta);

        // === 射线拾取逻辑 ===
        // 根据鼠标位置计算射线，并找到与原点最近的点
        const math::Vector2i *mouse_position_ptr=GetMouseCoord();
        if(!mouse_position_ptr)
            return;

        const math::Vector2i &mouse_position=*mouse_position_ptr;

        CameraControl *camera_control=GetCameraControl();

        const CameraInfo *ci=camera_control->GetCameraInfo();
        const ViewportInfo *vi=camera_control->GetViewportInfo();

        // 设置射线查询的屏幕坐标点
        ray.SetFromViewportPoint(mouse_position,ci,vi->GetViewport());

        // 求射线上与点(0,0,0)最近的点的坐标
        const math::Vector3f pos=ray.ClosestPoint(math::Vector3f(0,0,0));

        // 更新VAB上这个点的位置（动态更新顶点数据）
        if(prim_line_vab_map)
        {
            prim_line_vab_map->Write(&pos, 1);  // 1代表数据数量,不是字节数
        }
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("RayPicking (ECS Version)"),1280,720);
}
