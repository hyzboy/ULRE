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

#include<hgl/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/color/Color.h>
#include<ctime>
#include<chrono>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<cmath>

// 引入ECS相关头文件
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

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
    Material* material = nullptr;
    Pipeline* pipeline = nullptr;
    Geometry* geometry = nullptr;

    // 刻度数据
    struct TickData
    {
        Entity* entity;
        MaterialInstance* mi;
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

        #ifndef USE_MATERIAL_FILE
            auto* device = graphics_context->GetDevice();
            if (!device)
                return false;

            mtl::MaterialCreateInfo* mci = mtl::CreatePureColor2D(device->GetDevAttr(), &cfg);
            material = material_manager->CreateMaterial("PureColor2D", mci);
        #else
            material = material_manager->LoadMaterial("Std2D/PureColor2D", &cfg);
        #endif//USE_MATERIAL_FILE

            if (!material)
                return false;

            std::cout << "[ClockApp::InitMaterial] Created material: " << (void*)material << std::endl;
        }

        {
            // 刻度颜色（白色）
            Color4f tick_color(1.0f, 1.0f, 1.0f, 1.0f);

            mi_tick = material_manager->CreateMaterialInstance(material);
            if(mi_tick)
                mi_tick->WriteMIData(tick_color);

            // 指针颜色
            Color4f hand_colors[3] = {
                Color4f(1.0f, 0.0f, 0.0f, 1.0f),   // 时针 - 红色
                Color4f(0.0f, 1.0f, 0.0f, 1.0f),   // 分针 - 绿色
                Color4f(0.0f, 0.0f, 1.0f, 1.0f)    // 秒针 - 蓝色
            };

            for (uint i = 0; i < 3; i++)
            {
                hands[i].mi = material_manager->CreateMaterialInstance(material);
                if (!hands[i].mi)
                    return false;
                hands[i].mi->WriteMIData(hand_colors[i]);
            }
        }

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material, InlinePipeline::Solid2D) : nullptr;

        if (!pipeline)
        {
            std::cout << "[ClockApp::InitMaterial] ERROR: Failed to create pipeline!" << std::endl;
            return false;
        }

        std::cout << "[ClockApp::InitMaterial] Created pipeline: " << (void*)pipeline << std::endl;

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
            ticks[i].primitive = primitive_manager->CreatePrimitive(geometry, mi_tick, pipeline);

            if (!ticks[i].primitive)
            {
                std::cout << "[ClockApp::InitECS] ERROR: Failed to create tick primitive " << i << std::endl;
                return false;
            }

            // 创建刻度实体
            ticks[i].entity = ecs_world->CreateEntity<Entity>("ClockTick_" + std::to_string(i));

            // 添加TransformComponent - 静态变换
            auto transform = ticks[i].entity->AddComponent<TransformComponent>();

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
            primitive_comp->SetVisible(true);

            std::cout << "[ClockApp::InitECS] Created static tick [" << i << "] at angle " << (30.0f * i) << " degrees" << std::endl;
        }

        // === 创建3个指针（Movable Transform） ===
        // 指针长度倍数：时针最短，分针中等，秒针最长
        float hand_scales[3] = { 0.5f, 0.7f, 0.9f };
        const char* hand_names[3] = { "HourHand", "MinuteHand", "SecondHand" };

        for (uint i = 0; i < 3; i++)
        {
            hands[i].primitive = primitive_manager->CreatePrimitive(geometry, hands[i].mi, pipeline);

            if (!hands[i].primitive)
            {
                std::cout << "[ClockApp::InitECS] ERROR: Failed to create hand primitive " << i << std::endl;
                return false;
            }

            // 创建指针实体
            hands[i].entity = ecs_world->CreateEntity<Entity>(hand_names[i]);

            // 添加TransformComponent - 动态变换
            hands[i].transform = hands[i].entity->AddComponent<TransformComponent>().get();

            hands[i].transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            hands[i].transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));  // 单位四元数
            hands[i].transform->SetLocalScale(glm::vec3(hand_scales[i], hand_scales[i], 1.0f));

            // 关键：设置为可移动对象，每帧更新
            hands[i].transform->SetMovable(true);

            hands[i].length_scale = hand_scales[i];

            // 添加PrimitiveComponent
            auto primitive_comp = hands[i].entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            primitive_comp->SetPrimitive(hands[i].primitive);
            primitive_comp->SetVisible(true);

            std::cout << "[ClockApp::InitECS] Created movable hand [" << i << "] (" << hand_names[i] << ")" << std::endl;
        }

        std::cout << "[ClockApp::InitECS] === ECS Setup Complete ===" << std::endl;
        std::cout << "[ClockApp::InitECS] Created " << TICK_COUNT << " static ticks (offline baked)" << std::endl;
        std::cout << "[ClockApp::InitECS] Created 3 movable hands (updated every frame)" << std::endl;

        return true;
    }

public:

    using WorkObject::WorkObject;

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
};//class ClockApp:public WorkObject

int os_main(int, os_char**)
{
    return RunFramework<ClockApp>(OS_TEXT("Clock (Static and Movable Transform Separation with ECS)"), 1024, 1024);
}

