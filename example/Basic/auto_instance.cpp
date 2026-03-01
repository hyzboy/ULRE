// 该范例主要演示使用ECS架构绘制多个三角形，并利用RenderCollector进行排序以及自动合并进行Instance渲染
// This example demonstrates drawing multiple triangles using ECS architecture with automatic instancing
//
// 本范例展示了：
// 1. 使用ECS架构创建多个实体
// 2. 使用TransformComponent管理不同的空间变换
// 3. 使用PrimitiveComponent共享同一个渲染图元
// 4. RenderCollector自动合并相同材质和管线的对象进行Instance渲染
// 5. ECS与渲染系统的集成

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>

// 引入ECS相关头文件
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

constexpr uint32_t VERTEX_COUNT=3;

constexpr uint32_t TRIANGLE_NUMBER=12;

constexpr float position_data[VERTEX_COUNT*2]=
{
     0.0,  0.0,
    -0.1,  0.9,
     0.1,  0.9
};

constexpr uint8 color_data[VERTEX_COUNT][4]=
{
    {255,0,0,255},
    {0,255,0,255},
    {0,0,255,255}
};

class TestApp:public WorkObject
{
private:

    // ECS组件
    ECSContext *  ecs_world      =nullptr;   // 由默认 ECSContext 统一维护

    // 传统渲染资源（共享）
    MaterialInstance *  material_instance   =nullptr;
    Primitive *         prim_triangle       =nullptr;
    Pipeline *          pipeline            =nullptr;

    // 存储所有创建的实体
    std::vector<Entity*> triangle_entities;

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

        {
            mtl::Material2DCreateConfig cfg(PrimitiveType::Triangles,
                                            CoordinateSystem2D::NDC,
                                            mtl::WithLocalToWorld::With);

            VILConfig vil_config;

            vil_config.Add(VAN::Color,VF_V4UN8);

            material_instance=material_manager->CreateMaterialInstance(mtl::InlineMaterial::VertexColor2D,&cfg,&vil_config);
        }

        if(!material_instance)
            return(false);

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material_instance, InlinePipeline::Solid2D) : nullptr;

        return pipeline;
    }

    bool InitVBO()
    {
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
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::Color, VF_V4UN8, color_data))
            return false;

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);

        prim_triangle = primitive_manager->CreatePrimitive(geometry, material_instance, pipeline);

        if(!prim_triangle)
            return(false);

        return(true);
    }

    bool InitECS()
    {
        // === 步骤1: 获取ECS世界 ===
        // ECSContext由框架维护，通过GetECSContext()获取
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        // === 步骤2: 创建多个三角形实体 ===
        // 每个实体都有自己的Transform，但共享同一个Primitive
        // RenderCollector会自动识别并进行Instance渲染

        double rad;

        for(uint i=0;i<TRIANGLE_NUMBER;i++)
        {
            // 创建实体
            auto entity = ecs_world->CreateEntity<Entity>("Triangle_" + std::to_string(i));

            // === 步骤3: 添加TransformComponent ===
            // 每个三角形有不同的旋转变换
            auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);

            // 计算旋转角度
            rad = deg2rad((360.0/double(TRIANGLE_NUMBER))*i);

            // 使用四元数设置旋转（绕Z轴）
            // 注意：glm::angleAxis参数是(角度, 轴向量)
            glm::quat rotation = glm::angleAxis((float)rad, glm::vec3(0.0f, 0.0f, 1.0f));

            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(rotation);
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));

            // 设置为静态对象 - 因为三角形不会移动
            // 这样系统会缓存世界矩阵，提高性能
            transform->SetMovable(false);

            // === 步骤4: 添加PrimitiveComponent ===
            // 所有实体共享同一个Primitive
            // RenderCollector会检测到这一点并自动使用Instance渲染
            auto primitive_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            primitive_comp->SetPrimitive(prim_triangle);
            primitive_comp->SetVisible(true);

            // 保存实体引用
            triangle_entities.push_back(entity);
        }

        return true;
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.2f,0.2f,0.2f,1.0f));

        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);

        if(!InitECS())
            return(false);

        // 已在框架层设置默认 ECSContext
        // RenderCollector会自动收集所有PrimitiveComponent并进行批处理

        return(true);
    }

    void Tick(double delta_time) override
    {
        // ECS世界的更新由框架层 Tick 自动调用
        // 这里可以添加游戏逻辑更新

        WorkObject::Tick(delta_time);
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("AutoInstance (ECS Version)"),1024,1024);
}

