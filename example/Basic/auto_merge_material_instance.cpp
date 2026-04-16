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
// 5. InstanceDataDomain + IDDManager 直接管理实例数据的生命周期
//
// 架构要点：
// - Init 阶段只声明"语义材质需求"(RegisterSemanticMaterial)，不创建具体 MaterialTemplate
// - Primitive 以 domain-direct 方式创建 (CreatePrimitive(geometry, semantic_id, handle, slot, mgr))
// - MaterialTemplate / VIL / pipeline preset 在渲染时由 RenderPrimitiveCollectSystem
//   根据 Geometry 的实际 VAB 布局 + 当前帧运行时状态延迟解析
// - MI 颜色数据由 IDDManager 的 Domain/Slot 机制直接管理

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/module/IDDManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/IDDHandle.h>
#include<hgl/mtl/InstanceDataLayout.h>
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
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

constexpr uint32_t VERTEX_COUNT=3;

constexpr float position_data[VERTEX_COUNT*2]=
{
     0.0,  0.0,
     0.1,  0.9,
    -0.1,  0.9
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
        int slot_id = -1;                       ///< Domain slot index (颜色数据)
    };

    TriangleData triangles[DRAW_OBJECT_COUNT];

    IDDHandle   idd_handle;                     ///< 共享的 InstanceDataDomain 句柄
    IDDManager* idd_manager = nullptr;

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

        // ── 获取 IDDManager 并创建共享 Domain ─────────────────────────────
        idd_manager = GetMaterialManager()->GetIDDManager();
        if (!idd_manager)
        {
            std::cout << "[TestApp::InitECS] ERROR: Failed to get IDDManager!" << std::endl;
            return false;
        }

        idd_handle = idd_manager->Create(mtl::InstanceDataLayout::Color4f, DRAW_OBJECT_COUNT);
        if (!idd_handle.IsValid())
        {
            std::cout << "[TestApp::InitECS] ERROR: Failed to create InstanceDataDomain!" << std::endl;
            return false;
        }

        std::cout << "[TestApp::InitECS] Created shared IDD (Color4f, max=" << DRAW_OBJECT_COUNT << ")" << std::endl;

        for (uint i = 0; i < DRAW_OBJECT_COUNT; i++)
        {
            // ── 分配 Slot 并写入颜色数据 ─────────────────────────────────────
            const Color4f color = GetColor4f((COLOR)(i + int(COLOR::Blue)), 1.0f);

            triangles[i].slot_id = idd_manager->AllocSlot(idd_handle, &color, sizeof(color));
            if (triangles[i].slot_id < 0)
            {
                std::cout << "[TestApp::InitECS] ERROR: Failed to alloc slot " << i << std::endl;
                return false;
            }

            // ── 创建 domain-direct Primitive ─────────────────────────────────
            // 携带预分配的 domain handle + slot_id，material/VIL 在渲染时延迟解析
            triangles[i].primitive = primitive_manager->CreatePrimitive(
                geometry, semantic_id, idd_handle, triangles[i].slot_id, idd_manager);

            if (!triangles[i].primitive)
            {
                std::cout << "[TestApp::InitECS] ERROR: Failed to create primitive " << i << std::endl;
                return false;
            }

            std::cout << "[TestApp::InitECS] Created domain-direct primitive[" << i << "]: "
                      << (void*)triangles[i].primitive
                      << " slot_id=" << triangles[i].slot_id << std::endl;

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
        std::cout << "[TestApp::InitECS] Created " << DRAW_OBJECT_COUNT << " domain-direct primitives" << std::endl;
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

        // Enable domain-direct MI SSBO path so the batch pipeline reads per-domain
        // slot IDs instead of falling back to the legacy BindMaterialSlot shim.
        if (auto rcs = GetECSContext()->GetSystem<RenderPrimitiveCollectSystem>())
            rcs->SetDomainDirectMISsboEnabled(true);

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


