// 该范例主要演示使用ECS架构，在一个材质下使用不同结构体行传递颜色参数绘制三角形
// 并依赖RenderCollector中的自动合并功能，让同一材质下所有不同颜色的对象一次渲染完成
// This example demonstrates using different material rows under one material with ECS architecture
//
// 本范例展示了：
// 1. 使用ECS架构创建多个实体
// 2. 每个实体使用不同的结构体行（不同颜色）
// 3. 所有实体共享同一个Geometry（顶点数据）
// 4. RenderCollector自动合并相同Material的不同结构体行进行批量渲染
// 5. 示例自建 EmissiveSurface 结构体 SSBO 并注册进 ResourceDomainManager，RDBS 按 SSBOType+ID 严格绑定

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/color/Color.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>

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

namespace
{
    GeometryVertexFormat CreateAutoMergeGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V2F},
        };
        return gvf;
    }
}

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
    Geometry* geometry = nullptr;
    graph::mtl::MaterialRecipe triangle_recipe{};
    PrimitiveAsset triangle_asset{};

    // MI 结构体 SSBO
    graph::SSBOArrayAccessor<Color4f>* mtl_data_ssbo_accessor = nullptr;

    // 每个三角形的数据
    struct TriangleData
    {
        Entity* entity;
    };

    TriangleData triangles[DRAW_OBJECT_COUNT];

private:

    bool InitRecipe()
    {
        if (!geometry)
            return false;

        triangle_recipe.recipe_name = "AutoMergeMaterialData.PureColor";
        triangle_recipe.mtl_def_id = "builtin/pure_color";
        triangle_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid2DConfig();
        triangle_recipe.vertex_node_config = graph::mtl::Make2DNodeConfigNDC(true);
        graph::mtl::UpsertRecipeSSBOAssetBinding(triangle_recipe,
                                                 graph::mtl::DefaultMaterialDataSlotName,
                                                 mtl_data_ssbo_accessor->GetSSBOBinding());

        triangle_asset = PrimitiveAsset(geometry, &triangle_recipe, PrimitiveType::Triangles);


        return true;
    }

    bool CreateGeometry()
    {
        auto* device = GetDevice();
        auto* buffer_manager = GetManager<BufferManager>();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !buffer_manager || !geometry_manager)
            return false;

        GeometryCreater pc(device, CreateAutoMergeGeometryVertexFormat(), buffer_manager);
        pc.Init("Triangle", VERTEX_COUNT);   // 非索引几何：无 IBO（gl_VertexIndex 直通）
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
        // === 步骤1: 获取ECS世界 ===
        ecs_world = GetECSContext();
        if (!ecs_world)
        {
            std::cout << "[TestApp::InitECS] ERROR: Failed to get ECS context!" << std::endl;
            return false;
        }

        std::cout << "[TestApp::InitECS] Got ECS context: " << (void*)ecs_world << std::endl;

        // === 步骤2: 创建12个三角形实体，每个使用不同的结构体行 ===
        for (uint i = 0; i < DRAW_OBJECT_COUNT; i++)
        {
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
            // 每个实体共享同一 PrimitiveAsset，颜色来自不同结构体行
            auto primitive_comp = triangles[i].entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            primitive_comp->SetPrimitiveAsset(&triangle_asset);
            hgl::ecs::PrimitiveComponent::MaterialDataSlotAuthoringResource tri_struct{};
            tri_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
            tri_struct.ssbo_id = mtl_data_ssbo_accessor->GetSSBOId();
            tri_struct.data_index = i;
            tri_struct.use_data_index = true;
            tri_struct.shared_across_instances = false;
            primitive_comp->SetMaterialDataSlotResource(tri_struct);
            primitive_comp->SetVisible(true);

            std::cout << "[TestApp::InitECS] Entity[" << i << "] setup complete" << std::endl;
        }

        std::cout << "[TestApp::InitECS] === ECS Setup Complete ===" << std::endl;
        std::cout << "[TestApp::InitECS] Created " << DRAW_OBJECT_COUNT << " entities" << std::endl;
        std::cout << "[TestApp::InitECS] Each entity uses a different struct row (different color)" << std::endl;
        std::cout << "[TestApp::InitECS] RenderCollector will automatically merge them into batches" << std::endl;
        std::cout << "[TestApp::InitECS] ShaderProgram index tables are bound by strict SSBOType+ssbo_id routing" << std::endl;

        return true;
    }

    /**
     * 创建 EmissiveSurface 结构体 SSBO 并注册进 ResourceDomainManager。
     * 这是"新终极形态"示范：资源生产方自建 SSBO，向 RDBS 登记 layout，
     * 由 RenderDescriptorBindingSystem 按 SSBOType+ssbo_id 严格绑定，
     * PrimitiveBatchPipeline 负责按 draw order 写 DataIndex 行表。
     */
    bool InitMISSBO()
    {
        if (!ecs_world)
            ecs_world = GetECSContext();
        if (!ecs_world)
            return false;

        auto *domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        mtl_data_ssbo_accessor = domain_manager->AllocateArrayAccessor<Color4f>(
            graph::mtl::SSBOType::EmissiveSurface,
            "Example:EmissiveSurface:MaterialData",
            DRAW_OBJECT_COUNT);
        if (!mtl_data_ssbo_accessor)
            return false;

        for (uint i = 0; i < DRAW_OBJECT_COUNT; i++)
        {
            (*mtl_data_ssbo_accessor)[i] = GetColor4f((COLOR)(i + int(COLOR::Blue)), 1.0f);
            triangles[i].entity = nullptr;
        }
        mtl_data_ssbo_accessor->Commit();

        return true;
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        std::cout << "[TestApp::Init] === Initializing Application ===" << std::endl;

        if (!CreateGeometry())
        {
            std::cout << "[TestApp::Init] ERROR: InitGeometry failed!" << std::endl;
            return false;
        }

        if (!InitMISSBO())
        {
            std::cout << "[TestApp::Init] ERROR: InitMISSBO failed!" << std::endl;
            return false;
        }

        if (!InitRecipe())
        {
            std::cout << "[TestApp::Init] ERROR: InitRecipe failed!" << std::endl;
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

    ~TestApp()
    {
        SAFE_CLEAR(mtl_data_ssbo_accessor)
    }
};//class TestApp:public WorkObject

int os_main(int argc, os_char** argv)
{
    return RunFramework<TestApp>(OS_TEXT("Auto Merge ShaderProgram Instance (ECS Version)"), argc, argv, 1024, 1024);
}
