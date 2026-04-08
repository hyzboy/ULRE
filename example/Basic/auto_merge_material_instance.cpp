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

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/color/Color.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
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

//#define USE_MATERIAL_FILE   true        //是否使用材质文件

class TestApp:public WorkObject
{
private:

    // ECS组件
    ECSContext* ecs_world = nullptr;   // 由默认 ECSContext 统一维护

    // 传统渲染资源
    MaterialTemplate* material = nullptr;
    Geometry* geometry = nullptr;

    // 每个三角形的数据
    struct TriangleData
    {
        Entity* entity;
        Primitive* primitive;  ///< 包含所有渲染绑定态：MaterialTemplate、domain、mi_id、VIL、MIT
    };

    TriangleData triangles[DRAW_OBJECT_COUNT];

private:

    bool InitMaterial()
    {
        // 只需要获取 MaterialTemplate，实际 MI 分配在 InitECS 时进行
        static const mtl::MaterialAssetRecord kMergeCfg {
            .id       = "auto_merge_pure_color",
            .preset   = mtl::MaterialPreset::PureColor2D,
            .dim      = mtl::MaterialAssetRecord::Dim::D2,
            .pipeline = GraphicsPipelinePreset::Solid2D,
        };

        auto registry = GetMaterialAssetRegistry();
        auto handle = registry->Acquire(kMergeCfg);
        if (!handle.IsValid())
            return false;

        const VIL *resolved_vil = registry->ResolveVIL(handle.material, kMergeCfg);
        if (!resolved_vil)
            return false;

        material = handle.material;
        std::cout << "[TestApp::InitMaterial] Created material: " << (void*)material << std::endl;
        std::cout << "[TestApp::InitMaterial] MaterialTemplate has MI: " << material->hasMI() << std::endl;
        std::cout << "[TestApp::InitMaterial] MaterialTemplate MI data bytes: " << material->GetMIDataBytes() << std::endl;

        return true;
    }

    bool InitGeometry()
    {
        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = GetMaterialManager();
        if (!material_manager)
            return false;

        // 获取第一个三角形的 slot（仅用于获取 VIL）
        static const mtl::MaterialAssetRecord kMergeCfg {
            .id       = "auto_merge_pure_color",
            .preset   = mtl::MaterialPreset::PureColor2D,
            .dim      = mtl::MaterialAssetRecord::Dim::D2,
            .pipeline = GraphicsPipelinePreset::Solid2D,
        };

        auto registry = GetMaterialAssetRegistry();
        auto handle = registry->Acquire(kMergeCfg);
        if (!handle.IsValid())
            return false;

        const VIL *resolved_vil = registry->ResolveVIL(handle.material, kMergeCfg);
        if (!resolved_vil)
            return false;

        // 创建共用的 Geometry（所有三角形共享顶点数据）
        GraphicsGeometryFactory factory(graphics_context);
        auto creater = factory.CreateCreater(resolved_vil);
        if (!creater)
            return false;

        if (!creater->Init("Triangle", VERTEX_COUNT))
            return false;

        if (!creater->WriteVAB(VAN::Position, VF_V2F, position_data))
            return false;

        geometry = creater->Create();

        if (!geometry)
        {
            std::cout << "[TestApp::InitGeometry] ERROR: Failed to create geometry!" << std::endl;
            return false;
        }

        std::cout << "[TestApp::InitGeometry] Created geometry: " << (void*)geometry << std::endl;

        return true;
    }

    bool InitECS()
    {

        // === 步骤1: 获取ECS世界 ===
        ecs_world = GetECSContext();
        if (!ecs_world)
        {
            std::cout << "[TestApp::InitECS] ERROR: Failed to get ECS context!" << std::endl;
            return false;
        }

        std::cout << "[TestApp::InitECS] Got ECS context: " << (void*)ecs_world << std::endl;

        // === 步骤2: 创建12个三角形实体，每个使用不同的MaterialInstance ===
        auto* material_manager = GetMaterialManager();
        if (!material_manager)
            return false;

        auto* primitive_manager = GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        auto* registry = GetMaterialAssetRegistry();
        if (!registry)
            return false;

        static const mtl::MaterialAssetRecord kMergeCfg {
            .id       = "auto_merge_pure_color",
            .preset   = mtl::MaterialPreset::PureColor2D,
            .dim      = mtl::MaterialAssetRecord::Dim::D2,
            .pipeline = GraphicsPipelinePreset::Solid2D,
        };

        auto handle = registry->Acquire(kMergeCfg);
        if (!handle.IsValid())
            return false;

        const VIL *resolved_vil = registry->ResolveVIL(handle.material, kMergeCfg);
        if (!resolved_vil)
            return false;

        for (uint i = 0; i < DRAW_OBJECT_COUNT; i++)
        {
            // 为每个三角形分配独立的 MI 槽位
            Color4f color = GetColor4f((COLOR)(i + int(COLOR::Blue)), 1.0f);

            auto slot = material_manager->AllocMaterialInstanceSlot(
                handle.domain,
                handle.material,
                resolved_vil,
                kMergeCfg.pipeline,
                &color,
                sizeof(color));

            if (!slot.IsValid())
                return false;

            // 为每个三角形创建Primitive（共享Geometry，但使用不同的MaterialInstance）
            triangles[i].primitive = primitive_manager->CreatePrimitive(geometry, slot);

            if (!triangles[i].primitive)
            {
                std::cout << "[TestApp::InitECS] ERROR: Failed to create primitive " << i << std::endl;
                return false;
            }

            std::cout << "[TestApp::InitECS] Created primitive[" << i << "]: " << (void*)triangles[i].primitive << std::endl;

            // 验证 MI 数据（通过 primitive 访问）
            Color4f *mi_color = (Color4f *)triangles[i].primitive->GetMIData();
            if (mi_color)
            {
                std::cout << "[TestApp::InitECS] Triangle[" << i << "] MI Data: "
                          << "R=" << mi_color->r << ", G=" << mi_color->g 
                          << ", B=" << mi_color->b << ", A=" << mi_color->a << std::endl;
            }

            // 创建实体
            triangles[i].entity = ecs_world->CreateEntity<Entity>("ColoredTriangle_" + std::to_string(i));

            // === 步骤3: 添加TransformComponent ===
            // 每个三角形有不同的旋转角度
            auto transform = triangles[i].entity->AddComponent<TransformComponent>(Mobility::Static);

            // 计算旋转角度
            double rad = deg2rad(TRI_ROTATE_ANGLE * i);

            // 使用四元数设置旋转（绕Z轴）
            glm::quat rotation = glm::angleAxis((float)rad, glm::vec3(0.0f, 0.0f, 1.0f));

            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(rotation);
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));

            // 设置为静态对象
            transform->SetMovable(false);

            std::cout << "[TestApp::InitECS] Entity[" << i << "] rotation angle: " << (TRI_ROTATE_ANGLE * i) << " degrees" << std::endl;

            // === 步骤4: 添加PrimitiveComponent ===
            // 每个实体使用不同的Primitive（不同的MaterialInstance）
            auto primitive_comp = triangles[i].entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            primitive_comp->SetPrimitive(triangles[i].primitive);
            primitive_comp->SetVisible(true);

            std::cout << "[TestApp::InitECS] Entity[" << i << "] setup complete" << std::endl;
        }

        // 再次验证 MI 数据是否正常
        std::cout << "\n=== Verifying Material Instance Data ===" << std::endl;
        for (uint i = 0; i < DRAW_OBJECT_COUNT; i++)
        {
            Color4f *mi_color = (Color4f *)triangles[i].primitive->GetMIData();
            if (mi_color)
            {
                std::cout << "Triangle[" << i << "] MI Data Address: " << (void*)mi_color
                          << ", Color: R=" << mi_color->r << ", G=" << mi_color->g
                          << ", B=" << mi_color->b << ", A=" << mi_color->a << std::endl;
            }
            else
            {
                std::cout << "Triangle[" << i << "] WARNING: Failed to get MI data!" << std::endl;
            }
        }
        std::cout << "=== Verification Complete ===\n" << std::endl;

        std::cout << "[TestApp::InitECS] === ECS Setup Complete ===" << std::endl;
        std::cout << "[TestApp::InitECS] Created " << DRAW_OBJECT_COUNT << " entities" << std::endl;
        std::cout << "[TestApp::InitECS] Each entity uses a different MaterialInstanceData (different color)" << std::endl;
        std::cout << "[TestApp::InitECS] RenderCollector will automatically merge them into batches" << std::endl;
        std::cout << "[TestApp::InitECS] MaterialInstanceAssignmentBuffer will deduplicate MIs" << std::endl;

        return true;
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        std::cout << "[TestApp::Init] === Initializing Application ===" << std::endl;

        if (!InitMaterial())
        {
            std::cout << "[TestApp::Init] ERROR: InitMaterial failed!" << std::endl;
            return false;
        }

        if (!InitGeometry())
        {
            std::cout << "[TestApp::Init] ERROR: InitGeometry failed!" << std::endl;
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
        // ECS世界的更新由框架层 Tick 自动调用

        WorkObject::Tick(delta_time);
    }
};//class TestApp:public WorkObject

int os_main(int argc, os_char** argv)
{
    return RunFramework<TestApp>(OS_TEXT("Auto Merge MaterialTemplate Instance (ECS Version)"), argc, argv, 1024, 1024);
}


