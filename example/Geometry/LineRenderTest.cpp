#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/components/LinesComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/support/line/LineRenderPipeline.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/color/Color.h>
#include<cmath>
#include<memory>
#include<vector>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

class WireShapeTestApp:public WorkObject
{
    struct AnimatedLineGroup
    {
        TransformComponent *transform = nullptr;
        glm::vec3 base_position{0.0f, 0.0f, 0.0f};
        glm::vec3 base_scale{1.0f, 1.0f, 1.0f};
        float rotate_speed = 0.0f;
        float pulse_speed = 0.0f;
        float phase = 0.0f;
        float orbit_radius = 0.0f;
    };

    hgl::ecs::ECSContext *ecs_world = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;
    hgl::ecs::Entity *lines_entity = nullptr;
    std::vector<AnimatedLineGroup> animated_groups;
    float animation_time = 0.0f;

public:

    using WorkObject::WorkObject;

    ~WireShapeTestApp() override
    {
    }

    bool Init() override
    {
        auto *ecs = GetECSContext();
        if (!ecs)
            return false;

        ecs_world = ecs;

        // CN: 设置线条调色板
        // EN: Setup line color palette
        auto* line_pipeline = dynamic_cast<LineRenderPipeline*>(ecs->GetRenderPipeline("Line"));
        LogInfo("[LineRenderTest] Init: GetRenderPipeline returned %p\n", line_pipeline);
        
        if (line_pipeline)
        {
            LogInfo("[LineRenderTest] Init: Setting 8 palette colors\n");
            line_pipeline->SetPaletteColor(0, hgl::Color4f(1.0f, 0.0f, 0.0f, 1.0f)); // Red
            line_pipeline->SetPaletteColor(1, hgl::Color4f(0.0f, 1.0f, 0.0f, 1.0f)); // Green
            line_pipeline->SetPaletteColor(2, hgl::Color4f(0.0f, 0.0f, 1.0f, 1.0f)); // Blue
            line_pipeline->SetPaletteColor(3, hgl::Color4f(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow
            line_pipeline->SetPaletteColor(4, hgl::Color4f(1.0f, 0.0f, 1.0f, 1.0f)); // Magenta
            line_pipeline->SetPaletteColor(5, hgl::Color4f(0.0f, 1.0f, 1.0f, 1.0f)); // Cyan
            line_pipeline->SetPaletteColor(6, hgl::Color4f(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
            line_pipeline->SetPaletteColor(7, hgl::Color4f(0.5f, 0.0f, 1.0f, 1.0f)); // Purple
            LogInfo("[LineRenderTest] Init: Palette colors set complete\n");
        }
        else
        {
            LogError("[LineRenderTest] Init: Failed to get Line pipeline!\n");
        }

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

        // CN: 构造花哨线条图案（玫瑰曲线 + 辐条 + 连接线）
        // EN: Build fancy line patterns (rose curve + spokes + links)
        auto build_flower_pattern = [](LinesComponent *comp,
                                       float radius,
                                       float z,
                                       uint8_t color_offset,
                                       int petals,
                                       int segments)
        {
            if(!comp || petals <= 0 || segments < 8)
                return;

            const float pi = 3.14159265358979323846f;

            math::Vector3f prev_pos;
            bool has_prev = false;

            for(int i = 0; i <= segments; ++i)
            {
                float t = (2.0f * pi) * (float(i) / float(segments));
                float rose = std::cos(float(petals) * t) * radius;

                math::Vector3f pos(std::cos(t) * rose,
                                   std::sin(t) * rose,
                                   z);

                if(has_prev)
                    comp->AddLine(prev_pos, pos, uint8_t((color_offset + i) & 7));

                prev_pos = pos;
                has_prev = true;
            }

            const int spokes = petals * 4;
            for(int i = 0; i < spokes; ++i)
            {
                float t = (2.0f * pi) * (float(i) / float(spokes));
                math::Vector3f from(0.0f, 0.0f, z);
                math::Vector3f to(std::cos(t) * radius,
                                  std::sin(t) * radius,
                                  z);
                comp->AddLine(from, to, uint8_t((color_offset + i * 3) & 7));
            }
        };

        auto create_fancy_group = [&](const std::string &name,
                                      uint8_t width,
                                      bool with_transform,
                                      const glm::vec3 &position,
                                      const glm::quat &rotation,
                                      const glm::vec3 &scale,
                                      float radius,
                                      float z,
                                      uint8_t color_offset,
                                      int petals)
        {
            auto entity = ecs->CreateEntity<Entity>(name);
            if(!entity)
                return (TransformComponent *)nullptr;

            TransformComponent *transform_ptr = nullptr;

            if(with_transform)
            {
                auto tc = entity->AddComponent<TransformComponent>(Mobility::Movable);
                if(tc)
                {
                    tc->SetLocalPosition(position);
                    tc->SetLocalRotation(rotation);
                    tc->SetLocalScale(scale);
                    transform_ptr = tc.get();
                }
            }

            auto comp = entity->AddComponent<LinesComponent>();
            if(!comp)
                return transform_ptr;

            comp->SetWidth(width);

            build_flower_pattern(comp.get(), radius, z, color_offset, petals, 320);
            build_flower_pattern(comp.get(), radius * 0.45f, z + 0.08f, uint8_t((color_offset + 2) & 7), petals + 2, 240);

            const float pi = 3.14159265358979323846f;
            for(int i = 0; i < 20; ++i)
            {
                float t0 = (2.0f * pi) * (float(i) / 20.0f);
                float t1 = t0 + pi / 2.0f;

                math::Vector3f a(std::cos(t0) * radius * 0.75f,
                                 std::sin(t0) * radius * 0.75f,
                                 z - 0.12f);

                math::Vector3f b(std::cos(t1) * radius * 0.75f,
                                 std::sin(t1) * radius * 0.75f,
                                 z + 0.12f);

                comp->AddLine(a, b, uint8_t((color_offset + i * 5) & 7));
            }

            return transform_ptr;
        };

        lines_comp->SetWidth(1);
        build_flower_pattern(lines_comp.get(), 1.8f, -0.2f, 0, 5, 280);

        // CN: 无 TransformComponent（走 L2W[0] Identity）
        // EN: No TransformComponent (uses L2W[0] identity)
        create_fancy_group("Fancy_NoTransform_Width2", 2, false,
                           glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f),
                           1.6f, -1.4f, 1, 6);

        // CN: 有 TransformComponent（走对应 L2W 索引），可明显看到整体平移/旋转/缩放
        // EN: With TransformComponent (uses resolved L2W index), visibly translated/rotated/scaled
        auto transform_group_a = create_fancy_group("Fancy_WithTransform_Width4", 4, true,
                           glm::vec3(4.0f, 1.5f, 0.0f), glm::quat(0.9238795f, 0.0f, 0.0f, 0.3826834f), glm::vec3(1.35f, 1.35f, 1.0f),
                           1.6f, -1.4f, 3, 6);

        auto transform_group_b = create_fancy_group("Fancy_WithTransform_Width8", 8, true,
                           glm::vec3(-4.2f, -1.4f, 0.0f), glm::quat(0.8660254f, 0.0f, 0.0f, -0.5f), glm::vec3(0.85f, 1.6f, 1.0f),
                           1.5f, 1.3f, 5, 7);

        if(transform_group_a)
        {
            animated_groups.push_back(AnimatedLineGroup{
                transform_group_a,
                glm::vec3(4.0f, 1.5f, 0.0f),
                glm::vec3(1.35f, 1.35f, 1.0f),
                1.2f,
                2.1f,
                0.0f,
                0.55f
            });
        }

        if(transform_group_b)
        {
            animated_groups.push_back(AnimatedLineGroup{
                transform_group_b,
                glm::vec3(-4.2f, -1.4f, 0.0f),
                glm::vec3(0.85f, 1.6f, 1.0f),
                -0.9f,
                1.7f,
                1.4f,
                0.8f
            });
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
            camera->distance = 16.0f;
            camera->yaw = 45.0f;
            camera->pitch = -20.0f;
            camera->is_main_camera = true;
            camera->matrix_dirty = true;
        }

        return true;
    }

    void Tick(double delta_time) override
    {
        animation_time += float(delta_time);

        for(auto &group : animated_groups)
        {
            if(!group.transform)
                continue;

            const float t = animation_time + group.phase;
            const float angle = t * group.rotate_speed;
            const glm::quat rot_z = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            group.transform->SetLocalRotation(rot_z);

            const float pulse = 1.0f + 0.25f * std::sin(t * group.pulse_speed);
            group.transform->SetLocalScale(glm::vec3(group.base_scale.x * pulse,
                                                     group.base_scale.y * (1.0f + 0.18f * std::cos(t * group.pulse_speed * 0.7f)),
                                                     group.base_scale.z));

            const glm::vec3 pos = group.base_position + glm::vec3(std::cos(t * 0.8f) * group.orbit_radius,
                                                                   std::sin(t * 1.1f) * group.orbit_radius * 0.6f,
                                                                   0.0f);
            group.transform->SetLocalPosition(pos);
        }

        // === 让TransformSystem更新所有movable transform ===
        if (auto transform_system = ecs_world->GetSystem<TransformSystem>())
        {
            transform_system->Update(delta_time);
        }

        WorkObject::Tick(delta_time);
    }
};

int os_main(int,os_char **)
{
    return RunFramework<WireShapeTestApp>(OS_TEXT("Wire Shape Test"),1280,720);
}

