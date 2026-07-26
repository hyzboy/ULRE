// 该范例主要演示使用ECS架构结合Static/Movable Transform分离的时钟示例
// 刻度是静态的三角形（Static Transform），指针是动态更新的三角形（Movable Transform）
// This example demonstrates a clock using ECS architecture with Static/Movable Transform separation
//
// 本范例展示了：
// 1. 使用ECS架构创建时钟刻度和指针实体
// 2. 刻度三角形使用Static Transform（离线计算，不需要每帧更新）
// 3. 指针使用Movable Transform（每帧更新旋转角度）
// 4. TransformSystem自动管理Static和Movable transform的更新
// 5. ECS中static/movable数据完全分离，提高缓存效率

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/graph/SSBOSlotAllocator.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/color/Color.h>
#include<ctime>
#include<chrono>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<cmath>
#include<cstring>

// 引入ECS相关头文件
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateClockGeometryVertexFormat()
    {
        GeometryVertexFormat gvf;
        gvf.Add(VertexSemantic::Position, VF_V2F, 2, sizeof(float) * 2);
        return gvf;
    }
}

constexpr uint32_t VERTEX_COUNT = 3;

// 三角形顶点数据（基底在原点，尖端指向上方，占满窗口大小）
constexpr float position_data[VERTEX_COUNT * 2] =
{
    -0.05,  0.0,
    0.05,  0.0,
    0.0,   0.85
};

// 刻度数量
constexpr uint TICK_COUNT = 12;

// 刻度的半径位置（距离中心的距离）
constexpr float TICK_RADIUS = 0.75f;

//#define USE_MATERIAL_FILE   true        //是否使用材质文件

class ClockApp : public WorkObject
{
private:

    // ECS组件
    ECSContext* ecs_world = nullptr;

    // 传统渲染资源
    MaterialProgram* material = nullptr;
    Geometry* geometry = nullptr;
    graph::DeviceBuffer* mi_ssbo = nullptr;
    DescriptorBindingSet *tick_binding_set = nullptr;
    SSBOSlotAllocator slot_allocator;

    // 刻度数据
    struct TickData
    {
        Entity* entity;
        Primitive* primitive;
    };

    TickData ticks[TICK_COUNT];

    MaterialInstance *mi_tick;

    // 指针数据
    struct HandData
    {
        Entity* entity;
        MaterialInstance* mi;
        Primitive* primitive;
        TransformComponent* transform;
        float length_scale;  // 指针长度倍数
    };

    enum HandType { Hour, Minute, Second };
    HandData hands[3];  // 0=hour, 1=minute, 2=second
    DescriptorBindingSet *hand_binding_sets[3]{};

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

            material = material_manager->AcquireMaterialProgram(mtl::MaterialPreset::PureColor2D, &cfg);

            if (!material)
                return false;

            std::cout << "[ClockApp::InitMaterial] Created material: " << (void*)material << std::endl;
        }

        {
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

        GeometryCreater pc(device, CreateClockGeometryVertexFormat(), buffer_manager);
        pc.Init("TriangleForClock", VERTEX_COUNT);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data))
            return false;

        geometry = pc.Create();

        if (!geometry)
        {
            std::cout << "[ClockApp::InitGeometry] ERROR: Failed to create geometry!" << std::endl;
            return false;
        }

        std::cout << "[ClockApp::InitGeometry] Created geometry: " << (void*)geometry << std::endl;

        geometry_manager->Add(geometry);

        return true;
    }

    bool InitMISSBO()
    {
        if (!material)
            return false;

        if (!ecs_world)
            ecs_world = GetECSContext();
        if (!ecs_world)
            return false;

        const uint32_t mi_data_bytes = material->GetMIDataBytes();
        if (mi_data_bytes == 0)
            return true;
        if (mi_data_bytes != sizeof(Color4f))
            return false;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* buffer_manager = graphics_context->GetBufferManager();
        auto* domain_manager = graphics_context->GetResourceDomainManager();
        if (!buffer_manager || !domain_manager)
            return false;

        auto descriptor_system = ecs_world->GetSystem<RenderDescriptorBindingSystem>();
        if (!descriptor_system)
            return false;

        if (!slot_allocator.Init(4))
            return false;

        const uint32_t mi_count = 4;
        const VkDeviceSize ssbo_size = static_cast<VkDeviceSize>(mi_count) * mi_data_bytes;

        mi_ssbo = buffer_manager->CreateSSBO("Clock:PBRSurface:MIData", ssbo_size, nullptr, SharingMode::Exclusive);
        if (!mi_ssbo)
        {
            std::cout << "[ClockApp::InitMISSBO] ERROR: failed to create MI SSBO" << std::endl;
            return false;
        }

        auto *gpu_buf = mi_ssbo->GetGPUBuffer();
        if (!gpu_buf)
            return false;

        uint8_t *dst = static_cast<uint8_t *>(gpu_buf->Map(0, ssbo_size));
        if (!dst)
            return false;

        memset(dst, 0, static_cast<size_t>(ssbo_size));

        uint32_t tick_slot = 0;
        if (!slot_allocator.Allocate(tick_slot))
            return false;

        const Color4f tick_color(1.0f, 1.0f, 1.0f, 1.0f);
        memcpy(dst + static_cast<VkDeviceSize>(tick_slot) * mi_data_bytes, &tick_color, mi_data_bytes);

        Color4f hand_colors[3] = {
            Color4f(1.0f, 0.0f, 0.0f, 1.0f),
            Color4f(0.0f, 1.0f, 0.0f, 1.0f),
            Color4f(0.0f, 0.0f, 1.0f, 1.0f)
        };
        uint32_t hand_slots[3]{};
        for (uint i = 0; i < 3; ++i)
        {
            if (!slot_allocator.Allocate(hand_slots[i]))
                return false;
            memcpy(dst + static_cast<VkDeviceSize>(hand_slots[i]) * mi_data_bytes, &hand_colors[i], mi_data_bytes);
        }

        gpu_buf->Unmap();

        tick_binding_set = new DescriptorBindingSet(material);
        if (!tick_binding_set)
            return false;
        for (uint i = 0; i < 3; ++i)
        {
            hand_binding_sets[i] = new DescriptorBindingSet(material);
            if (!hand_binding_sets[i])
                return false;
        }

        bool has_struct_binding = false;
        for (const auto &req : material->GetMaterialResourceLayout().requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                continue;

            has_struct_binding = true;
            const graph::mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
            if (!domain_manager->RegisterBuffer(addr, mi_ssbo, mi_count))
                return false;

            if (!tick_binding_set->SetSSBOBinding(req.ssbo_type, req.ssbo_id, tick_slot))
                return false;
            for (uint i = 0; i < 3; ++i)
            {
                if (!hand_binding_sets[i]->SetSSBOBinding(req.ssbo_type, req.ssbo_id, hand_slots[i]))
                    return false;
            }
        }

        std::cout << "[ClockApp::InitMISSBO] registered PBRSurface SSBO: count=" << mi_count
                  << ", stride=" << mi_data_bytes << std::endl;
        return has_struct_binding;
    }

    bool InitECS()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
        {
            std::cout << "[ClockApp::InitECS] ERROR: Missing RenderContext!" << std::endl;
            return false;
        }

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
        {
            std::cout << "[ClockApp::InitECS] ERROR: Missing GraphicsContext!" << std::endl;
            return false;
        }

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager)
        {
            std::cout << "[ClockApp::InitECS] ERROR: Missing PrimitiveManager!" << std::endl;
            return false;
        }

        // === 获取ECS世界 ===
        ecs_world = GetECSContext();
        if (!ecs_world)
        {
            std::cout << "[ClockApp::InitECS] ERROR: Failed to get ECS context!" << std::endl;
            return false;
        }

        std::cout << "[ClockApp::InitECS] Got ECS context: " << (void*)ecs_world << std::endl;

        // === 创建12个刻度（Static Transform） ===
        for (uint i = 0; i < TICK_COUNT; i++)
        {
            ticks[i].primitive = primitive_manager->CreatePrimitive(geometry, material, tick_binding_set, nullptr);

            if (!ticks[i].primitive)
            {
                std::cout << "[ClockApp::InitECS] ERROR: Failed to create tick primitive " << i << std::endl;
                return false;
            }

            // 创建刻度实体
            ticks[i].entity = ecs_world->CreateEntity<Entity>("ClockTick_" + std::to_string(i));

            // 添加TransformComponent - 静态变换
            auto transform = ticks[i].entity->AddComponent<TransformComponent>(Mobility::Static);

            // 计算刻度角度（360 / 12 = 30度）
            float tick_angle = deg2rad(30.0f * i);

            // 计算刻度在圆周上的位置
            float x = TICK_RADIUS * sin(tick_angle);
            float y = TICK_RADIUS * cos(tick_angle);

            // 让刻度指向圆心（局部+Y指向圆心方向，取反角度）
            float to_center_angle = -std::atan2(-x, -y);
            glm::quat rotation = glm::angleAxis(to_center_angle, glm::vec3(0.0f, 0.0f, 1.0f));

            transform->SetLocalPosition(glm::vec3(x, y, 0.0f));
            transform->SetLocalRotation(rotation);
            transform->SetLocalScale(glm::vec3(0.8f, 0.15f, 1.0f));  // 缩小刻度尺寸

            // 关键：设置为静态对象，不需要每帧更新
            transform->SetMovable(false);

            // 添加PrimitiveComponent
            auto primitive_comp = ticks[i].entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            primitive_comp->SetPrimitive(ticks[i].primitive);
            primitive_comp->RequestPipeline(InlinePipeline::Solid2D);
            primitive_comp->SetVisible(true);

            std::cout << "[ClockApp::InitECS] Created static tick [" << i << "] at angle " << (30.0f * i) << " degrees" << std::endl;
        }

        // === 创建3个指针（Movable Transform） ===
        // 指针长度倍数：时针最短，分针中等，秒针最长
        float hand_scales[3] = { 0.5f, 0.7f, 0.9f };
        const char* hand_names[3] = { "HourHand", "MinuteHand", "SecondHand" };

        for (uint i = 0; i < 3; i++)
        {
            hands[i].primitive = primitive_manager->CreatePrimitive(geometry,
                                                                    material,
                                                                    hand_binding_sets[i],
                                                                    nullptr);

            if (!hands[i].primitive)
            {
                std::cout << "[ClockApp::InitECS] ERROR: Failed to create hand primitive " << i << std::endl;
                return false;
            }

            // 创建指针实体
            hands[i].entity = ecs_world->CreateEntity<Entity>(hand_names[i]);

            // 添加TransformComponent - 动态变换
            hands[i].transform = hands[i].entity->AddComponent<TransformComponent>(Mobility::Movable).get();

            hands[i].transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            hands[i].transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));  // 单位四元数
            hands[i].transform->SetLocalScale(glm::vec3(hand_scales[i], hand_scales[i], 1.0f));

            // 关键：设置为可移动对象，每帧更新
            hands[i].transform->SetMovable(true);

            hands[i].length_scale = hand_scales[i];

            // 添加PrimitiveComponent
            auto primitive_comp = hands[i].entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            primitive_comp->SetPrimitive(hands[i].primitive);
            primitive_comp->RequestPipeline(InlinePipeline::Solid2D);
            primitive_comp->SetVisible(true);

            std::cout << "[ClockApp::InitECS] Created movable hand [" << i << "] (" << hand_names[i] << ")" << std::endl;
        }

        std::cout << "[ClockApp::InitECS] === ECS Setup Complete ===" << std::endl;
        std::cout << "[ClockApp::InitECS] Created " << TICK_COUNT << " static ticks (offline baked)" << std::endl;
        std::cout << "[ClockApp::InitECS] Created 3 movable hands (updated every frame)" << std::endl;

        return true;
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.1f, 0.1f, 0.1f, 1.0f));

        std::cout << "[ClockApp::Init] === Initializing Clock Application ===" << std::endl;

        if (!InitMaterial())
        {
            std::cout << "[ClockApp::Init] ERROR: InitMaterial failed!" << std::endl;
            return false;
        }

        if (!InitGeometry())
        {
            std::cout << "[ClockApp::Init] ERROR: InitGeometry failed!" << std::endl;
            return false;
        }

        if (!InitMISSBO())
        {
            std::cout << "[ClockApp::Init] ERROR: InitMISSBO failed!" << std::endl;
            return false;
        }

        if (!InitECS())
        {
            std::cout << "[ClockApp::Init] ERROR: InitECS failed!" << std::endl;
            return false;
        }

        std::cout << "[ClockApp::Init] === Initialization Complete ===" << std::endl;

        return true;
    }

    void Tick(double delta_time) override
    {
        // === 获取当前时间 ===
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        struct tm* time_info = std::localtime(&time_t_now);

        int hour = time_info->tm_hour % 12;
        int minute = time_info->tm_min;
        int second = time_info->tm_sec;

        // 获取毫秒部分用于秒针的平滑移动
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        // === 更新指针的旋转角度 ===

        // 时针：12小时 = 360度，每小时30度 + 分钟贡献（正值，但需要上下颠倒）
        float hour_angle = deg2rad((hour * 30.0f) + (minute * 0.5f)) + glm::pi<float>();
        glm::quat hour_rotation = glm::angleAxis(hour_angle, glm::vec3(0.0f, 0.0f, 1.0f));
        hands[Hour].transform->SetLocalRotation(hour_rotation);

        // 分针：60分钟 = 360度，每分钟6度 + 秒钟贡献（正值，但需要上下颠倒）
        float minute_angle = deg2rad((minute * 6.0f) + (second * 0.1f)) + glm::pi<float>();
        glm::quat minute_rotation = glm::angleAxis(minute_angle, glm::vec3(0.0f, 0.0f, 1.0f));
        hands[Minute].transform->SetLocalRotation(minute_rotation);

        // 秒针：60秒 = 360度，每秒6度 + 毫秒贡献用于平滑（取正值，反向于之前）
        float second_angle = deg2rad((second * 6.0f) + (ms.count() * 0.006f));
        glm::quat second_rotation = glm::angleAxis(second_angle, glm::vec3(0.0f, 0.0f, 1.0f));
        hands[Second].transform->SetLocalRotation(second_rotation);

        // === 标记指针为脏，等待系统更新 ===
        for (uint i = 0; i < 3; i++)
        {
            hands[i].transform->MarkDirty();
        }

        // === 让TransformSystem更新所有movable transform ===
        if (auto transform_system = ecs_world->GetSystem<TransformSystem>())
        {
            transform_system->Update(delta_time);
        }

        WorkObject::Tick(delta_time);
    }

    ~ClockApp()
    {
        delete tick_binding_set;
        for (auto *dbs : hand_binding_sets)
            delete dbs;
        SAFE_CLEAR(mi_ssbo)
    }
};//class ClockApp:public WorkObject

int os_main(int argc, os_char** argv)
{
    return RunFramework<ClockApp>(OS_TEXT("Clock (Static and Movable Transform Separation with ECS)"), argc, argv, 1024, 1024);
}
