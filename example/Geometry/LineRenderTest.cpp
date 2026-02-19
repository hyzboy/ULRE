#include<hgl/WorkManager.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/graph/geo/line/LineRenderManager.h>
#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/components/LinesComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<cmath>
#include<memory>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

class WireShapeTestApp:public WorkObject
{
    hgl::ecs::ECSContext *ecs_world = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;
    hgl::ecs::Entity *lines_entity = nullptr;
    std::shared_ptr<hgl::ecs::LineRenderSystem> line_render_system;
    LineRenderManager *line_manager = nullptr;

public:

    using WorkObject::WorkObject;

    ~WireShapeTestApp() override
    {
        delete line_manager;
        line_manager = nullptr;
    }

    bool Init() override
    {
        auto *ecs = GetECSContext();
        if (!ecs)
            return false;

        auto *render_target = ecs->GetRenderTarget();
        if (!render_target)
            return false;

        ecs_world = ecs;

        // CN: 注册线条渲染系统（延迟初始化）
        // EN: Register line render system (lazy initialization)
        line_render_system = ecs->GetSystem<LineRenderSystem>();
        if (!line_render_system)
            line_render_system = ecs->RegisterRenderSystem<LineRenderSystem>();

        if (!line_render_system)
        {
            LogError("WireShapeTestApp::Init: Failed to create/register LineRenderSystem\n");
            return false;
        }

        // CN: 创建并设置 LineRenderManager
        // EN: Create and set LineRenderManager
        auto *render_context = ecs->GetRenderContext();
        if (render_context)
            line_manager = CreateLineRenderManager(render_context, render_target);
        else if (auto *graphics_context = ecs->GetGraphicsContext())
            line_manager = CreateLineRenderManager(graphics_context, render_target);

        if (!line_manager)
        {
            LogError("WireShapeTestApp::Init: Failed to create LineRenderManager\n");
            return false;
        }

        line_render_system->SetLineRenderManager(line_manager);

        // CN: 创建存储线条的 Entity
        // EN: Create entity to hold lines
        lines_entity = ecs->CreateEntity<Entity>("DebugLines");
        if (!lines_entity)
        {
            LogError("WireShapeTestApp::Init: Failed to create lines entity\n");
            return false;
        }

        // CN: 添加 LinesComponent
        // EN: Add LinesComponent
        auto lines_comp = lines_entity->AddComponent<LinesComponent>();
        if (!lines_comp)
        {
            LogError("WireShapeTestApp::Init: Failed to add LinesComponent\n");
            return false;
        }

        // CN: 设置调色板颜色
        // EN: Set palette colors
        Color4f palette[8] = {
            Color4f(1,0,0,1),              // red
            Color4f(0,1,0,1),              // green
            Color4f(0,0,1,1),              // blue
            Color4f(1,1,0,1),              // yellow
            Color4f(0,1,1,1),              // cyan
            Color4f(1,0,1,1),              // magenta
            Color4f(1,1,1,1),              // white
            Color4f(0.5f,0.5f,0.5f,1)      // gray
        };

        for (int i = 0; i < 8; ++i)
        {
            line_render_system->SetColor(i, palette[i]);
        }

        // CN: 创建同心圆层，每层是辐条环
        // EN: Create concentric layers for widths 1..6; each layer is a ring of radial spokes
        const int spokes = 24;            // number of spokes per layer
        const float inner_radius = 0.5f;  // start of each spoke from center
        const float base_radius = 2.0f;   // base radius for the innermost layer
        const float radius_step = 0.8f;   // how much each layer's radius increases
        const float z_step = 0.5f;       // z offset between layers so they don't overlap exactly

        // CN: 按宽度分组，每个宽度一个 LinesComponent
        // EN: Group by width - for demo, we'll use a single component with varying widths
        for(int width = 1; width <= 16; ++width)
        {
            float radius = base_radius ;
            float z = (width%2?1:-1)*(width - 1) * z_step; // layer height
            uint8_t color_index = uint8_t((width - 1) % 8);
            uint8_t w = uint8_t(width);

            for(int i = 0; i < spokes; ++i)
            {
                float angle = (2.0f * 3.14159265358979323846f) * (float(i) / float(spokes));

                math::Vector3f from(std::cos(angle) * inner_radius, std::sin(angle) * inner_radius, z);
                math::Vector3f to  (std::cos(angle) * radius,       std::sin(angle) * radius,       z);

                // CN: 为更大的宽度添加线条
                // EN: Add line to component
                if (width == 1)
                {
                    lines_comp->AddLine(from, to, color_index);
                }
                else
                {
                    // CN: 这里可以创建多个 Entity，每个不同宽度一个
                    // EN: For simplicity, we create one per width
                    auto w_entity = ecs->CreateEntity<Entity>(std::string("Lines_Width_") + std::to_string(width));
                    auto w_comp = w_entity->AddComponent<LinesComponent>();
                    w_comp->SetWidth(w);
                    w_comp->AddLine(from, to, color_index);
                }
            }
        }

        // CN: 设置相机
        // EN: Setup camera
        if (ecs_world)
        {
            auto camera_system = ecs_world->GetSystem<CameraSystem>();
            if (!camera_system)
                camera_system = ecs_world->RegisterTickSystem<CameraSystem>(ecs_world);

            camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
            auto camera = camera_entity->AddComponent<CameraComponent>();

            camera->control_mode = CameraComponent::ControlMode::ViewModel;
            camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
            camera->distance = 12.0f;
            camera->yaw = 45.0f;
            camera->pitch = -20.0f;
            camera->is_main_camera = true;
            camera->matrix_dirty = true;
        }

        return true;
    }
};

int os_main(int,os_char **)
{
    return RunFramework<WireShapeTestApp>(OS_TEXT("Wire Shape Test"),1280,720);
}

