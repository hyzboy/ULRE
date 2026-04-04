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
#include<hgl/graph/module/MaterialAssetLoader.h>
#include<hgl/color/Color.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>

// 引入几何创建器
#include<hgl/graph/geo/GeometryCreater.h>

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
    Material* material = nullptr;
    Geometry* geometry = nullptr;

    // 每个三角形的数据
    struct TriangleData
    {
        Entity* entity;
        MaterialInstance* mi;
        Primitive* primitive;
    };

    TriangleData triangles[DRAW_OBJECT_COUNT];

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
            static const mtl::MaterialAssetRecord kMergeCfg {
                .id       = "auto_merge_pure_color",
                .preset   = mtl::MaterialPreset::PureColor2D,
                .dim      = mtl::MaterialAssetRecord::Dim::D2,
                .pipeline = GraphicsPipelinePreset::Solid2D,
            };

            material = LoadMaterialFromRecord(material_manager, nullptr, nullptr, kMergeCfg);

            if (!material)
                return false;

            std::cout << "[TestApp::InitMaterial] Created material: " << (void*)material << std::endl;
            std::cout << "[TestApp::InitMaterial] Material has MI: " << material->hasMI() << std::endl;
            std::cout << "[TestApp::InitMaterial] Material MI data bytes: " << material->GetMIDataBytes() << std::endl;

            // 为每个三角形创建不同颜色的MaterialInstance
            for (uint i = 0; i < DRAW_OBJECT_COUNT; i++)
            {
                graph::MaterialInstanceSpec spec;
                spec.material = material;
                spec.preset = GraphicsPipelinePreset::Solid2D;
                triangles[i].mi = material_manager->AcquireMaterialInstance(spec);

                if (!triangles[i].mi)
                    return false;

                // 使用不同的颜色
                Color4f color = GetColor4f((COLOR)(i + int(COLOR::Blue)), 1.0f);

                std::cout << "[TestApp::InitMaterial] Triangle[" << i << "] color: "
                          << "R=" << color.r << ", G=" << color.g << ", B=" << color.b << ", A=" << color.a << std::endl;

                triangles[i].mi->WriteMIData(color);       //设置MaterialInstance的数据
            }
        }

        {
            for (uint i = 0; i < DRAW_OBJECT_COUNT; i++)
            {
                Color4f *mi_color=(Color4f *)triangles[i].mi->GetMIData();

                std::cout<<"[TestApp::InitMaterial] Triangle["<<i<<"] MI Data Address: "<<(void*)mi_color
                    <<", Color: R="<<mi_color->r<<", G="<<mi_color->g<<", B="<<mi_color->b<<", A="<<mi_color->a<<std::endl;
            }
        }

        return true;
    }

    bool InitGeometry()
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
        if (!device || !buffer_manager || !geometry_manager)
            return false;

        GeometryCreater pc(device, material->GetDefaultVIL(), buffer_manager);
        pc.Init("Triangle", VERTEX_COUNT);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data))
            return false;

        geometry = pc.Create();

        if (!geometry)
        {
            std::cout << "[TestApp::InitGeometry] ERROR: Failed to create geometry!" << std::endl;
            return false;
        }

        std::cout << "[TestApp::InitGeometry] Created geometry: " << (void*)geometry << std::endl;

        geometry_manager->Add(geometry);

        return true;
    }

    bool InitECS()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        // === 步骤1: 获取ECS世界 ===
        ecs_world = GetECSContext();
        if (!ecs_world)
        {
            std::cout << "[TestApp::InitECS] ERROR: Failed to get ECS context!" << std::endl;
            return false;
        }

        std::cout << "[TestApp::InitECS] Got ECS context: " << (void*)ecs_world << std::endl;

        // === 步骤2: 创建12个三角形实体，每个使用不同的MaterialInstance ===
        for (uint i = 0; i < DRAW_OBJECT_COUNT; i++)
        {
            // 为每个三角形创建Primitive（共享Geometry，但使用不同的MaterialInstance）
            auto* primitive_manager = graphics_context->GetPrimitiveManager();
            if (!primitive_manager)
                return false;

            triangles[i].primitive = primitive_manager->CreatePrimitive(geometry, triangles[i].mi);

            if (!triangles[i].primitive)
            {
                std::cout << "[TestApp::InitECS] ERROR: Failed to create primitive " << i << std::endl;
                return false;
            }

            std::cout << "[TestApp::InitECS] Created primitive[" << i << "]: " << (void*)triangles[i].primitive
                      << ", MI: " << (void*)triangles[i].mi << std::endl;

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
    return RunFramework<TestApp>(OS_TEXT("Auto Merge Material Instance (ECS Version)"), argc, argv, 1024, 1024);
}

