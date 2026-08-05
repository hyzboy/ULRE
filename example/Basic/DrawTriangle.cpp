// 该范例主要演示使用新的ECS架构管理和绘制一个渐变色的三角形，参考draw_triangle_use_UBO.cpp
// This example demonstrates managing and drawing a gradient colored triangle using the new ECS architecture
//
// 本范例展示了：
// 1. 创建ECS World和Entity
// 2. 使用TransformComponent管理空间变换
// 3. 使用PrimitiveComponent管理渲染图元
// 4. ECS与传统渲染系统的集成

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/mtl/MaterialLibrary.h>

 // 引入ECS相关头文件
 #include<hgl/ecs/core/Context.h>
 #include<hgl/ecs/core/Entity.h>
 #include<hgl/ecs/components/TransformComponent.h>
 #include<hgl/ecs/components/PrimitiveComponent.h>
 #include<hgl/object/ObjectTracker.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateDrawTriangleGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V2I},
            {VertexSemantic::Color,    VF_V4F},
        };
        return gvf;
    }
}

constexpr uint32_t VERTEX_COUNT=3;

static float position_data_float[VERTEX_COUNT][2]=
{
    {0.5,   0.25},
    {0.75,  0.75},
    {0.25,  0.75}
};

static int32 position_data[VERTEX_COUNT][2]={};

constexpr float color_data[VERTEX_COUNT][4]=
{
    {1.0f,0.0f,0.0f,1.0f},
    {0.0f,1.0f,0.0f,1.0f},
    {0.0f,0.0f,1.0f,1.0f}
};

constexpr VkFormat POSITION_DATA_FORMAT     =VF_V2I;

constexpr VkFormat COLOR_DATA_FORMAT        =VF_V4F;

class TestApp:public WorkObject
{
private:

    // ECS组件
    ECSContext *  ecs_world      =nullptr;   // 由默认 ECSContext 统一维护
    Entity* triangle_entity     =nullptr;
    uint64_t entity_id          =0;          // 对象追踪ID

    // 传统渲染资源
    Geometry *          geom_triangle       =nullptr;
    graph::mtl::MaterialRecipe triangle_recipe{};
    PrimitiveAsset             triangle_asset{};

private:

    bool CreateRenderObject()
    {
        const auto ext=GetExtent();

        for(uint i=0;i<VERTEX_COUNT;i++)
        {
            position_data[i][0]=position_data_float[i][0]*ext->width;
            position_data[i][1]=position_data_float[i][1]*ext->height;
        }

        auto* device = GetDevice();
        auto* buffer_manager = GetManager<BufferManager>();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !buffer_manager || !geometry_manager)
            return false;

        GeometryCreater pc(device, CreateDrawTriangleGeometryVertexFormat(), buffer_manager);
        if (!pc.Init("Triangle", VERTEX_COUNT))
            return false;
        if (!pc.WriteVAB(VAN::Position, POSITION_DATA_FORMAT, position_data) ||
            !pc.WriteVAB(VAN::Color, COLOR_DATA_FORMAT, color_data))
            return false;

        geom_triangle = pc.Create();
        if (!geom_triangle)
            return false;
        geometry_manager->Add(geom_triangle);

        return true;
    }

    bool InitECS()
    {
        HGL_CAPTURE_SCOPE();  // 记录此函数调用的栈信息

        // === 步骤1: 创建ECS世界 ===
        // World是ECS架构的顶层容器，管理所有Entity和System
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        // === 步骤2: 创建Entity ===
        // Entity是游戏对象的容器，本身不包含数据，只是Component的集合
        triangle_entity = ecs_world->CreateEntity<Entity>("TriangleEntity");
        entity_id = HGL_TRACK_ALLOCATION("TriangleEntity", hgl::core::ObjectTypeTag::RenderSystem);

        // === 步骤3: 添加TransformComponent ===
        // TransformComponent管理空间变换（位置、旋转、缩放）
        // 内部使用SOA（Structure of Arrays）存储以提高缓存性能
        HGL_TRACK_ALLOCATION("TriangleTransform", hgl::core::ObjectTypeTag::FrameResource);
        auto transform = triangle_entity->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));

        // 设置为静态对象 - 系统会缓存世界矩阵，提高性能
        transform->SetMovable(false);

        // === 步骤4: 添加ECS PrimitiveComponent ===
        // 新的ECS PrimitiveComponent用于管理渲染图元
        // 注意：需要明确使用hgl::ecs命名空间，因为有两个PrimitiveComponent
        HGL_TRACK_ALLOCATION("TrianglePrimitive", hgl::core::ObjectTypeTag::FrameResource);
        auto ecs_primitive = triangle_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        triangle_recipe.recipe_name = "DrawTriangle.VertexColor";
        triangle_recipe.mtl_def_id = "VertexColor";
        triangle_recipe.domain = "DrawTriangle";
        triangle_recipe.vertex_node_config = graph::mtl::Make2DNodeConfigOrtho(false);
        triangle_recipe.pipeline_preset = PipelinePreset::Solid2D;
        triangle_asset = PrimitiveAsset(geom_triangle, &triangle_recipe, PrimitiveType::Triangles);
        ecs_primitive->SetPrimitiveAsset(&triangle_asset);
        ecs_primitive->SetVisible(true);

        return true;
    }

public:

    bool Init() override
    {
        HGL_CAPTURE_SCOPE();  // 记录应用初始化的调用栈

        if(!CreateRenderObject())
            return(false);

        if(!InitECS())
            return(false);

        // 初始化时已设置默认 ECSContext

         return(true);
     }
 };//class TestApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Draw triangle use ECS"),argc,argv);
}
