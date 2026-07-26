// 该范例主要演示使用ECS架构，在一个材质下使用不同结构体行传递颜色参数绘制三角形
// 并依赖RenderCollector中的自动合并功能，让同一材质下所有不同颜色的对象一次渲染完成
// This example demonstrates using different material rows under one material with ECS architecture
//
// 本范例展示了：
// 1. 使用ECS架构创建多个实体
// 2. 每个实体使用不同的结构体行（不同颜色）
// 3. 所有实体共享同一个Geometry（顶点数据）
// 4. RenderCollector自动合并相同Material的不同结构体行进行批量渲染
// 5. 示例自建 PBRSurface 结构体 SSBO 并注册进 ResourceDomainManager，RDBS 按 SSBOType+ID 严格绑定

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/SSBOSlotAllocator.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/color/Color.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/BufferManager.h>

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
    constexpr uint32_t kAutoMergeMaterialSsboId = hgl::graph::mtl::MakeRecipeSSBOId(8301);

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
    MaterialProgram* material = nullptr;
    Geometry* geometry = nullptr;

    // MI 结构体 SSBO（由本示例创建并注册进 ResourceDomainManager）
    graph::DeviceBuffer* mi_ssbo = nullptr;
    SSBOSlotAllocator slot_allocator;
    graph::mtl::SSBOType material_ssbo_type = graph::mtl::SSBOType::UserDefined;
    uint32_t material_ssbo_id = 0;
    uint32_t material_ssbo_count = 0;
    uint32_t material_ssbo_stride = 0;

    // 每个三角形的数据
    struct TriangleData
    {
        Entity* entity;
        Primitive* primitive;
    };

    TriangleData triangles[DRAW_OBJECT_COUNT];

private:

    bool InitMaterial()
    {
        if (!geometry)
            return false;

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

            material = material_manager->AcquireMaterialProgram(mtl::MaterialPreset::PureColor2D,
                                                                &cfg,
                                                                geometry->GetGeometryVertexFormat());

            if (!material)
                return false;

            std::cout << "[TestApp::InitMaterial] Created material: " << (void*)material << std::endl;
            std::cout << "[TestApp::InitMaterial] MaterialProgram has MI: " << material->hasMI() << std::endl;
            std::cout << "[TestApp::InitMaterial] MaterialProgram MI data bytes: " << material->GetMIDataBytes() << std::endl;

            // 仅创建材质；外部 SSBO 在 InitMISSBO 中配置
            for (uint i = 0; i < DRAW_OBJECT_COUNT; i++)
            {
                Color4f color = GetColor4f((COLOR)(i + int(COLOR::Blue)), 1.0f);

                std::cout << "[TestApp::InitMaterial] Triangle[" << i << "] color: "
                          << "R=" << color.r << ", G=" << color.g << ", B=" << color.b << ", A=" << color.a << std::endl;
            }
        }

        return true;
    }

    bool CreateGeometry()
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

        GeometryCreater pc(device, CreateAutoMergeGeometryVertexFormat(), buffer_manager);
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

        // === 步骤2: 创建12个三角形实体，每个使用不同的结构体行 ===
        for (uint i = 0; i < DRAW_OBJECT_COUNT; i++)
        {
            // 为每个三角形创建Primitive（共享Geometry/MaterialProgram，由 ECS 绑定不同结构体行）
            auto* primitive_manager = graphics_context->GetPrimitiveManager();
            if (!primitive_manager)
                return false;

            triangles[i].primitive = primitive_manager->CreatePrimitive(geometry,
                                                                        material,
                                                                        nullptr,
                                                                        nullptr);

            if (!triangles[i].primitive)
            {
                std::cout << "[TestApp::InitECS] ERROR: Failed to create primitive " << i << std::endl;
                return false;
            }

            std::cout << "[TestApp::InitECS] Created primitive[" << i << "]: " << (void*)triangles[i].primitive << std::endl;

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
            // 每个实体使用不同的Primitive（颜色来自不同结构体行）
            auto primitive_comp = triangles[i].entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            primitive_comp->SetPrimitive(triangles[i].primitive);
            graph::mtl::MaterialRecipe recipe{};
            recipe.recipe_name = "AutoMergeMaterialInstance.PureColor2D";
            recipe.shading_model = graph::mtl::ShadingModel::Unlit;
            recipe.preset_hint = static_cast<uint32_t>(graph::mtl::MaterialPreset::PureColor2D);
            recipe.domain = "AutoMergeMaterialInstance";
            primitive_comp->SetMaterialRecipe(recipe);
            primitive_comp->SetMaterialStructResource(graph::mtl::DataSlot::PBRSurface,
                                                      material_ssbo_type,
                                                      material_ssbo_id,
                                                      mi_ssbo,
                                                      material_ssbo_count,
                                                      material_ssbo_stride,
                                                      i,
                                                      true,
                                                      false);
            primitive_comp->RequestPipeline(InlinePipeline::Solid2D);
            primitive_comp->SetVisible(true);

            std::cout << "[TestApp::InitECS] Entity[" << i << "] setup complete" << std::endl;
        }

        std::cout << "[TestApp::InitECS] === ECS Setup Complete ===" << std::endl;
        std::cout << "[TestApp::InitECS] Created " << DRAW_OBJECT_COUNT << " entities" << std::endl;
        std::cout << "[TestApp::InitECS] Each entity uses a different struct row (different color)" << std::endl;
        std::cout << "[TestApp::InitECS] RenderCollector will automatically merge them into batches" << std::endl;
        std::cout << "[TestApp::InitECS] MaterialProgram index tables are bound by strict SSBOType+ssbo_id routing" << std::endl;

        return true;
    }

    /**
     * 创建 PBRSurface 结构体 SSBO 并注册进 ResourceDomainManager。
     * 这是"新终极形态"示范：资源生产方自建 SSBO，向 RDBS 登记 layout，
     * 由 RenderDescriptorBindingSystem 按 SSBOType+ssbo_id 严格绑定，
     * PrimitiveBatchPipeline 负责按 draw order 写 DataIndex 行表。
     */
    bool InitMISSBO()
    {
        const uint32_t mi_data_bytes = material->GetMIDataBytes();
        if (mi_data_bytes == 0)
            return true;  // 材质无 MI 数据，无需创建
        if (mi_data_bytes != sizeof(Color4f))
            return false;

        if (!ecs_world)
            ecs_world = GetECSContext();
        if (!ecs_world)
        {
            std::cout << "[TestApp::InitMISSBO] ERROR: Failed to get ECS context!" << std::endl;
            return false;
        }

        auto* render_context  = GetRenderContext();
        if (!render_context) return false;
        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context) return false;

        auto* buffer_manager  = graphics_context->GetBufferManager();
        if (!buffer_manager) return false;

        if (!slot_allocator.Init(DRAW_OBJECT_COUNT))
            return false;

        const VkDeviceSize ssbo_size = static_cast<VkDeviceSize>(DRAW_OBJECT_COUNT) * mi_data_bytes;
        material_ssbo_count = DRAW_OBJECT_COUNT;
        material_ssbo_stride = mi_data_bytes;
        mi_ssbo = buffer_manager->CreateSSBO("Example:PBRSurface:MIData", ssbo_size);
        if (!mi_ssbo)
        {
            std::cout << "[TestApp::InitMISSBO] ERROR: failed to create PBRSurface SSBO" << std::endl;
            return false;
        }

        auto* gpu_buf = mi_ssbo->GetGPUBuffer();
        if (!gpu_buf)
        {
            std::cout << "[TestApp::InitMISSBO] ERROR: no GPU buffer" << std::endl;
            return false;
        }
        uint8_t* ptr = static_cast<uint8_t*>(gpu_buf->Map(0, ssbo_size));
        if (!ptr)
        {
            std::cout << "[TestApp::InitMISSBO] ERROR: map failed" << std::endl;
            return false;
        }
        memset(ptr, 0, static_cast<size_t>(ssbo_size));
        for (uint i = 0; i < DRAW_OBJECT_COUNT; i++)
        {
            uint32_t slot_index = 0;
            if (!slot_allocator.Allocate(slot_index))
                return false;

            Color4f color = GetColor4f((COLOR)(i + int(COLOR::Blue)), 1.0f);
            memcpy(ptr + static_cast<VkDeviceSize>(slot_index) * mi_data_bytes, &color, mi_data_bytes);

            triangles[i].entity = nullptr;
            triangles[i].primitive = nullptr;
        }
        gpu_buf->Unmap();

        bool has_struct_binding = false;
        for (const auto &req : material->GetMaterialResourceLayout().requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                continue;

            has_struct_binding = true;
            material_ssbo_type = req.ssbo_type;
            material_ssbo_id = kAutoMergeMaterialSsboId;
            break;
        }

        std::cout << "[TestApp::InitMISSBO] PBRSurface SSBO registered: "
                  << DRAW_OBJECT_COUNT << " instances x " << mi_data_bytes << " bytes"
                  << ", ssbo_id=" << material_ssbo_id << std::endl;

        return has_struct_binding;
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

        if (!InitMaterial())
        {
            std::cout << "[TestApp::Init] ERROR: InitMaterial failed!" << std::endl;
            return false;
        }

        if (!InitMISSBO())
        {
            std::cout << "[TestApp::Init] ERROR: InitMISSBO failed!" << std::endl;
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

    ~TestApp()
    {
        SAFE_CLEAR(mi_ssbo)
    }
};//class TestApp:public WorkObject

int os_main(int argc, os_char** argv)
{
    return RunFramework<TestApp>(OS_TEXT("Auto Merge MaterialProgram Instance (ECS Version)"), argc, argv, 1024, 1024);
}
