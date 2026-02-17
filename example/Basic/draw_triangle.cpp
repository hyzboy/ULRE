// 该范例主要演示使用新的ECS架构管理和绘制一个渐变色的三角形，参考draw_triangle_use_UBO.cpp
// This example demonstrates managing and drawing a gradient colored triangle using the new ECS architecture
//
// 本范例展示了：
// 1. 创建ECS World和Entity
// 2. 使用TransformComponent管理空间变换
// 3. 使用PrimitiveComponent管理渲染图元
// 4. ECS与传统渲染系统的集成

#include<hgl/WorkManager.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>

 // 引入ECS相关头文件
 #include<hgl/ecs/core/Context.h>
 #include<hgl/ecs/core/Entity.h>
 #include<hgl/ecs/components/TransformComponent.h>
 #include<hgl/ecs/components/PrimitiveComponent.h>
 #include<hgl/utils/ObjectTracker.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

constexpr uint32_t VERTEX_COUNT=3;

static float position_data_float[VERTEX_COUNT][2]=
{
    {0.5,   0.25},
    {0.75,  0.75},
    {0.25,  0.75}
};

static int16 position_data[VERTEX_COUNT][2]={};

constexpr uint8 color_data[VERTEX_COUNT*4]=
{
    255,0,0,255,
    0,255,0,255,
    0,0,255,255
};

constexpr VAType   POSITION_SHADER_FORMAT   =VAT_IVEC2;
constexpr VkFormat POSITION_DATA_FORMAT     =VF_V2I16;

constexpr VkFormat COLOR_DATA_FORMAT        =VF_V4UN8;

class TestApp:public WorkObject
{
private:

    // ECS组件
    ECSContext *  ecs_world      =nullptr;   // 由默认 ECSContext 统一维护
    Entity* triangle_entity     =nullptr;
    uint64_t entity_id          =0;          // 对象追踪ID

    // 传统渲染资源
    MaterialInstance *  material_instance   =nullptr;
    Primitive *         prim_triangle       =nullptr;
    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        mtl::Material2DCreateConfig cfg(PrimitiveType::Triangles,
                                        CoordinateSystem2D::Ortho,
                                        mtl::WithLocalToWorld::Without);

        VILConfig vil_config;

        cfg.position_format     =       POSITION_SHADER_FORMAT;     //这里指定shader中使用ivec2当做顶点输入格式
                                //      ^
                                //      +  这上下两种格式要配套，否则会出错
                                //      v
        vil_config.Add(VAN::Position,   POSITION_DATA_FORMAT);     //这里指定VAB中使用RG16I当做顶点数据格式

        vil_config.Add(VAN::Color,      COLOR_DATA_FORMAT);        //这里指定VAB中使用RGBA8UNorm当做颜色数据格式

        material_instance=material_manager->CreateMaterialInstance(mtl::inline_material::VertexColor2D,&cfg,&vil_config);

        if(!material_instance)
            return(false);

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material_instance, InlinePipeline::Solid2D) : nullptr;

        if (pipeline)
            std::cout << "[TestApp::InitMaterial] Created Pipeline in RenderPass (render_pass*=0x" << std::hex << (uintptr_t)render_pass 
                      << ", VkPipeline=0x" << (VkPipeline)(*pipeline) << std::dec 
                      << ", Pipeline*=0x" << (uintptr_t)pipeline << ")" << std::endl;
        else
            std::cout << "[TestApp::InitMaterial] FAILED to create Pipeline (render_target=0x" << std::hex << (uintptr_t)render_target 
                      << ", render_pass=0x" << (uintptr_t)render_pass << std::dec << ")" << std::endl;

        return pipeline != nullptr;
    }

    bool InitVBO()
    {
        const auto ext=GetExtent();

        for(uint i=0;i<VERTEX_COUNT;i++)
        {
            position_data[i][0]=position_data_float[i][0]*ext->width;
            position_data[i][1]=position_data_float[i][1]*ext->height;
        }

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* device = graphics_context->GetDevice();
        auto* buffer_manager = graphics_context->GetBufferManager();
        auto* geometry_manager = graphics_context->GetGeometryManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!device || !buffer_manager || !geometry_manager || !primitive_manager)
            return false;

        GeometryCreater pc(device, material_instance->GetVIL(), buffer_manager);
        pc.Init("Triangle", VERTEX_COUNT);
        if (!pc.WriteVAB(VAN::Position, POSITION_DATA_FORMAT, position_data) ||
            !pc.WriteVAB(VAN::Color, COLOR_DATA_FORMAT, color_data))
            return false;

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);

        prim_triangle = primitive_manager->CreatePrimitive(geometry, material_instance, pipeline);

        if(!prim_triangle)
            return(false);

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
        auto transform = triangle_entity->AddComponent<TransformComponent>();
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
        ecs_primitive->SetPrimitive(prim_triangle);
        ecs_primitive->SetVisible(true);

        return true;
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        HGL_CAPTURE_SCOPE();  // 记录应用初始化的调用栈
        
        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);

        if(!InitECS())
            return(false);

        // 初始化时已设置默认 ECSContext

         return(true);
     }

     void Tick(double delta_time) override
     {
         // 更新ECS世界 - 这会更新所有Entity和Component
        // 框架层 Tick 会调用 ECSContext::Tick

         WorkObject::Tick(delta_time);
     }

 };//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Draw triangle use ECS"));
}

