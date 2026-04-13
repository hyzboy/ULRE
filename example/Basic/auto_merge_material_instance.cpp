// 该范例主要演示使用ECS架构，在一个材质下使用不同材质实例传递颜色参数绘制三角形
// 该范例主要演示使用ECS架构，在一个材质下使用不同材质实例传递颜色参数绘制三角形
// 并依赖RenderCollector中的自动合并功能，让同一材质下所有不同材质实例的对象一次渲染完成
// This example demonstrates using different material instances under one material with ECS architecture
//
// 本范例展示了：
// 1. 使用ECS架构创建多个实体
// 2. 每个实体使用不同的MaterialInstance（不同颜色）
// 3. 所有实体共享同一个Geometry（顶点数据）
// 4. RenderCollector自动合并相同Material的不同MaterialInstance进行批量渲染
// 5. MaterialInstanceAssignmentBuffer自动去重和索引管理
//
// 架构要点：
// - Init 阶段只声明"语义材质需求"(RegisterSemanticMaterial)，不创建具体 MaterialTemplate
// - Primitive 以延迟方式创建 (CreatePrimitive(geometry, semantic_id))
// - MaterialTemplate / VIL / pipeline preset 在渲染时由 RenderPrimitiveCollectSystem
//   根据 Geometry 的实际 VAB 布局 + 当前帧运行时状态延迟解析
// - MI 数据（颜色）的生命周期由 MaterialInstanceHandle 独立管理

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/color/Color.h>
#include<hgl/graph/module/PrimitiveManager.h>

// 引入几何创建器
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>

// 引入ECS相关头文件
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

constexpr uint32_t VERTEX_COUNT=3;

constexpr float position_data[VERTEX_COUNT*2]=
{
     0.0,  0.0,
    -0.1,  0.9,
     0.1,  0.9
};

constexpr uint DRAW_OBJECT_COUNT=12;
constexpr double TRI_ROTATE_ANGLE=360.0/DRAW_OBJECT_COUNT;

// 材质语义描述：只声明"需要什么样的材质"，不在 init 时创建具体的 MaterialTemplate。
// MaterialTemplate = f(语义需求, Geometry VAB, 运行时渲染状态)，三者只有在渲染时全部已知。
static const mtl::MaterialAssetRecord kMergeCfg {
    .id       = "auto_merge_pure_color",
    .preset   = mtl::MaterialPreset::PureColor2D,
    .dim      = mtl::MaterialAssetRecord::Dim::D2,
    .pipeline = GraphicsPipelinePreset::Solid2D,
};

class TestApp:public WorkObject
{
private:

    ECSContext* ecs_world = nullptr;

    SemanticMaterialId semantic_id = 0;     ///< 稳定的语义材质标识（不含 pipeline 等运行时字段）
    Geometry* geometry = nullptr;

    struct TriangleData
    {
        Entity* entity = nullptr;
        Primitive* primitive = nullptr;
        MaterialInstanceHandle mi_handle = InvalidMaterialInstanceHandle;   ///< MI 数据生命周期句柄
    };

    TriangleData triangles[DRAW_OBJECT_COUNT];

private:

    bool InitGeometry()
    {
        // Geometry 的创建完全独立于 Material，只关心顶点数据本身
        geometry = CreateGeometry(
            "Triangle",
            VERTEX_COUNT,
            {{VAN::Position, VF_V2F, position_data}});

        if (!geometry)
        {
            std::cout << "[TestApp::InitGeometry] ERROR: Failed to create geometry!" << std::endl;
            return false;
        }

        std::cout << "[TestApp::InitGeometry] Created geometry: " << (void*)geometry << std::endl;
        return true;
    }

    bool InitSemanticMaterial()
    {
        // 只注册语义材质——不创建 MaterialTemplate、不解析 VIL、不分配 Domain。
        // semantic_id 是一个稳定的哈希标识，不包含 pipeline/domain_id 等运行时策略字段。
        semantic_id = RegisterSemanticMaterial(kMergeCfg);
        if (semantic_id == 0)
        {
            std::cout << "[TestApp::InitSemanticMaterial] ERROR: Failed to register semantic material!" << std::endl;
            return false;
        }

        std::cout << "[TestApp::InitSemanticMaterial] Registered semantic_id: " << semantic_id << std::endl;
        return true;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world)
        {
            std::cout << "[TestApp::InitECS] ERROR: Failed to get ECS context!" << std::endl;
            return false;
        }

        auto* primitive_manager = GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        for (uint i = 0; i < DRAW_OBJECT_COUNT; i++)
        {
            // ── 创建延迟绑定的 Primitive ──────────────────────────────────────
            // 此时只绑定 Geometry + SemanticMaterialId。
            // MaterialTemplate / VIL / Domain 在渲染时由 RenderPrimitiveCollectSystem
            // 根据 Geometry 的实际 VAB 布局 + 运行时状态延迟解析。
            triangles[i].primitive = primitive_manager->CreatePrimitive(geometry, semantic_id);
            if (!triangles[i].primitive)
            {
                std::cout << "[TestApp::InitECS] ERROR: Failed to create primitive " << i << std::endl;
                return false;
            }

            // ── 通过 Handle 预分配 MI 数据 ────────────────────────────────────
            // MI 数据（颜色）的生命周期由 MaterialInstanceHandle 独立管理，
            // 与 Primitive 解耦。渲染时系统会将 Handle 对应的 domain/mi_id
            // 绑定到 Primitive 上。
            const Color4f color = GetColor4f((COLOR)(i + int(COLOR::Blue)), 1.0f);
            triangles[i].mi_handle = AllocateMaterialHandle(kMergeCfg, &color, sizeof(color));

            if (triangles[i].mi_handle == InvalidMaterialInstanceHandle)
            {
                std::cout << "[TestApp::InitECS] ERROR: Failed to allocate MI handle " << i << std::endl;
                return false;
            }

            std::cout << "[TestApp::InitECS] Created deferred primitive[" << i << "]: "
                      << (void*)triangles[i].primitive
                      << " mi_handle=" << triangles[i].mi_handle << std::endl;

            // ── 创建 Entity + Components ──────────────────────────────────────
            triangles[i].entity = ecs_world->CreateEntity<Entity>("ColoredTriangle_" + std::to_string(i));

            auto transform = triangles[i].entity->AddComponent<TransformComponent>(Mobility::Static);

            const double rad = deg2rad(TRI_ROTATE_ANGLE * i);
            const glm::quat rotation = glm::angleAxis((float)rad, glm::vec3(0.0f, 0.0f, 1.0f));

            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(rotation);
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            auto primitive_comp = triangles[i].entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            primitive_comp->SetPrimitive(triangles[i].primitive);
            primitive_comp->SetVisible(true);

            std::cout << "[TestApp::InitECS] Entity[" << i << "] rotation angle: "
                      << (TRI_ROTATE_ANGLE * i) << " degrees — setup complete" << std::endl;
        }

        std::cout << "[TestApp::InitECS] === ECS Setup Complete ===" << std::endl;
        std::cout << "[TestApp::InitECS] Created " << DRAW_OBJECT_COUNT << " deferred primitives" << std::endl;
        std::cout << "[TestApp::InitECS] MaterialTemplate/VIL will be resolved at render time" << std::endl;

        return true;
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        std::cout << "[TestApp::Init] === Initializing Application ===" << std::endl;

        if (!InitGeometry())
        {
            std::cout << "[TestApp::Init] ERROR: InitGeometry failed!" << std::endl;
            return false;
        }

        if (!InitSemanticMaterial())
        {
            std::cout << "[TestApp::Init] ERROR: InitSemanticMaterial failed!" << std::endl;
            return false;
        }

        if (!InitECS())
        {
            std::cout << "[TestApp::Init] ERROR: InitECS failed!" << std::endl;
            return false;
        }

        std::cout << "[TestApp::Init] === Initialization Complete ===" << std::endl;

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
    }
};//class TestApp:public WorkObject

int os_main(int argc, os_char** argv)
{
    return RunFramework<TestApp>(OS_TEXT("Auto Merge MaterialTemplate Instance (ECS Version)"), argc, argv, 1024, 1024);
}


