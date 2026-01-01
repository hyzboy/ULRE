// 该范例演示使用简化的ECS框架进行渲染，移除了World和SceneRenderer依赖
// This example demonstrates rendering using the simplified ECS framework, with World and SceneRenderer removed
//
// 本范例展示了：
// 1. 使用SimpleECSFramework替代RenderFramework
// 2. 不依赖World和SceneRenderer
// 3. 使用ECS RenderCollector进行批次渲染
// 4. 简化的资源管理和渲染循环

#include"SimpleECSFramework.h"
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>

// 引入ECS相关头文件
#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/RenderCollector.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

// 顶点数据
constexpr uint32_t VERTEX_COUNT = 3;

static float position_data_float[VERTEX_COUNT][2] = {
    {0.5f,   0.25f},
    {0.75f,  0.75f},
    {0.25f,  0.75f}
};

static int16 position_data[VERTEX_COUNT][2] = {};

constexpr uint8 color_data[VERTEX_COUNT * 4] = {
    255, 0, 0, 255,
    0, 255, 0, 255,
    0, 0, 255, 255
};

constexpr VAType   POSITION_SHADER_FORMAT = VAT_IVEC2;
constexpr VkFormat POSITION_DATA_FORMAT = VF_V2I16;
constexpr VkFormat COLOR_DATA_FORMAT = VF_V4UN8;

/**
 * 简化框架版本的ECS三角形应用
 * ECS Triangle Application with Simplified Framework
 */
class SimpleECSTriangleApp : public SimpleECSWorkObject
{
private:
    // 渲染资源
    MaterialInstance* material_instance = nullptr;
    Primitive* prim_triangle = nullptr;
    Pipeline* pipeline = nullptr;
    
    // ECS组件
    std::shared_ptr<ECSContext> ecs_world = nullptr;
    std::shared_ptr<Entity> triangle_entity = nullptr;
    RenderCollector* render_collector = nullptr;

private:
    bool InitMaterial()
    {
        mtl::Material2DCreateConfig cfg(
            PrimitiveType::Triangles,
            CoordinateSystem2D::Ortho,
            mtl::WithLocalToWorld::With
        );
        
        VILConfig vil_config;
        cfg.position_format = POSITION_SHADER_FORMAT;
        vil_config.Add(VAN::Position, POSITION_DATA_FORMAT);
        vil_config.Add(VAN::Color, COLOR_DATA_FORMAT);
        
        material_instance = CreateMaterialInstance(mtl::inline_material::VertexColor2D, &cfg, &vil_config);
        if (!material_instance)
            return false;
        
        pipeline = CreatePipeline(material_instance, InlinePipeline::Solid2D);
        return pipeline != nullptr;
    }
    
    bool InitGeometry()
    {
        const auto ext = GetExtent();
        if (!ext)
            return false;
        
        // 转换位置数据到屏幕坐标
        for (uint i = 0; i < VERTEX_COUNT; i++)
        {
            position_data[i][0] = static_cast<int16>(position_data_float[i][0] * ext->width);
            position_data[i][1] = static_cast<int16>(position_data_float[i][1] * ext->height);
        }
        
        prim_triangle = CreatePrimitive(
            "Triangle", VERTEX_COUNT, material_instance, pipeline,
            {
                {VAN::Position, POSITION_DATA_FORMAT, position_data},
                {VAN::Color, COLOR_DATA_FORMAT, color_data}
            }
        );
        
        return prim_triangle != nullptr;
    }
    
    bool InitECS()
    {
        // 创建ECS世界
        ecs_world = std::make_shared<ECSContext>("SimpleTriangleWorld");
        ecs_world->Initialize();
        
        // 注册RenderCollector系统
        render_collector = ecs_world->RegisterSystem<RenderCollector>();
        render_collector->SetWorld(ecs_world);
        render_collector->SetDevice(GetDevice());
        render_collector->Initialize();
        
        // 配置相机
        const auto ext = GetExtent();
        if (!ext)
            return false;
        
        CameraInfo camera;
        camera.position = glm::vec3(0.0f, 0.0f, 10.0f);
        camera.forward = glm::vec3(0.0f, 0.0f, -1.0f);
        camera.viewMatrix = glm::mat4(1.0f);
        camera.projectionMatrix = glm::ortho(
            0.0f, static_cast<float>(ext->width),
            0.0f, static_cast<float>(ext->height),
            0.1f, 100.0f
        );
        camera.UpdateViewProjection();
        render_collector->SetCameraInfo(&camera);
        
        // 创建Entity
        triangle_entity = ecs_world->CreateEntity<Entity>("TriangleEntity");
        
        // 添加TransformComponent
        auto transform = triangle_entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);
        
        // 添加PrimitiveComponent
        auto ecs_primitive = triangle_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        ecs_primitive->SetPrimitive(prim_triangle);
        ecs_primitive->SetVisible(true);
        
        return true;
    }

public:
    using SimpleECSWorkObject::SimpleECSWorkObject;
    
    bool Init() override
    {
        if (!InitMaterial())
            return false;
        
        if (!InitGeometry())
            return false;
        
        if (!InitECS())
            return false;
        
        return true;
    }
    
    void Tick(double delta_time) override
    {
        // 更新ECS世界
        if (ecs_world)
        {
            ecs_world->Update(delta_time);
        }
    }
    
    void Draw(RenderCmdBuffer* cmd_buf) override
    {
        // 使用ECS RenderCollector进行渲染
        if (render_collector && cmd_buf)
        {
            render_collector->CollectRenderables();
            render_collector->Render(cmd_buf);
        }
    }
    
    ~SimpleECSTriangleApp()
    {
        // 关闭ECS世界
        if (ecs_world)
        {
            ecs_world->Shutdown();
        }
    }
};

int os_main(int, os_char**)
{
    return RunSimpleECSFramework<SimpleECSTriangleApp>(OS_TEXT("Simple ECS Triangle"), 1024, 1024);
}
