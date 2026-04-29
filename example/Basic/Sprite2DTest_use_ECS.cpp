// example/Basic/Sprite2DTest_use_ECS.cpp
// Step 5: 03_Sprite2DPerspectiveECS
// Covers scenarios ①–⑦ from doc/Sprite2D_Step5_示例.md

#include<hgl/framework/WorkManager.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/DefaultSystems.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/components/Sprite2DComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/support/sprite2d/Sprite2DRenderPipelineGroup.h>
#include<hgl/color/Color4f.h>
#include<glm/glm.hpp>
#include<glm/gtc/constants.hpp>
#include<iostream>
#include<vector>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

class Sprite2DPerspectiveECSApp : public WorkObject
{
    ECSContext*          ecs_context   = nullptr;
    Entity*              camera_entity = nullptr;

    // Sprites for mode-switching (scenarios ① ② ⑦)
    std::vector<Entity*> mode_sprites;

    // Edge-triggered key states
    bool last_key_1 = false;
    bool last_key_2 = false;
    bool last_key_3 = false;

private:

    // ──────────────────────────────────────────────────────────────────────
    // ECS bootstrap
    // ──────────────────────────────────────────────────────────────────────

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context) return false;

        // Opt-in Sprite2D group (Sprite2DResourcePrepareSystem + Sprite2DMaterialBindingSystem)
        if (!EnsureSystemGroupSystems(ecs_context, "Sprite2D", ecs_context->GetRenderTarget()))
            return false;

        return true;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Camera
    // ──────────────────────────────────────────────────────────────────────

    bool InitCamera()
    {
        if (!ecs_context->EnsureCameraSystem()) return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        if (!camera_entity) return false;

        auto cam = camera_entity->AddComponent<CameraComponent>();
        cam->control_mode   = CameraComponent::ControlMode::ViewModel;
        cam->target         = math::Vector3f(0.0f, 0.0f, 0.0f);
        cam->distance       = 30.0f;
        cam->yaw            = 45.0f;
        cam->pitch          = -25.0f;
        cam->is_main_camera = true;
        cam->matrix_dirty   = true;

        cam->camera_data   = GetCamera();
        cam->camera_info   = const_cast<graph::CameraInfo*>(GetCameraInfo());
        cam->viewport_info = GetViewportInfo();

        return true;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Sprite creation helpers
    // ──────────────────────────────────────────────────────────────────────

    Entity* CreateSpriteEntity(const char* name, const glm::vec3& pos)
    {
        Entity* e = ecs_context->CreateEntity<Entity>(name);
        if (!e) return nullptr;

        auto t = e->AddComponent<TransformComponent>(Mobility::Static);
        t->SetLocalPosition(pos);
        t->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        t->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        t->SetMovable(false);

        return e;
    }

    bool CreateSprites()
    {
        // ─────────────────────────────────────────────────────────────────
        // ① AxisLocked + fixed_size: screen-space fixed pixel, camera-stable
        // ─────────────────────────────────────────────────────────────────
        {
            Entity* e = CreateSpriteEntity("Sprite_AxisLockedFixed", glm::vec3(-8.0f, 0.0f, 0.0f));
            if (!e) return false;

            auto s = e->AddComponent<Sprite2DComponent>();
            s->SetFixedSize(true);
            s->SetPixelSize(256, 256);
            s->SetPivot(glm::vec2(0.5f, 0.5f));
            s->SetRotation(0.0f);
            s->SetTexturePath(OS_TEXT("res/image/lena.Tex2D"));
            s->SetVisible(true);

            mode_sprites.push_back(e);
        }

        // ─────────────────────────────────────────────────────────────────
        // ② CameraFacing + world_size: world-space billboard
        // ─────────────────────────────────────────────────────────────────
        {
            Entity* e = CreateSpriteEntity("Sprite_CameraFacingWorld", glm::vec3(0.0f, 0.0f, 0.0f));
            if (!e) return false;

            auto s = e->AddComponent<Sprite2DComponent>();
            s->SetFixedSize(false);
            s->SetWorldSize(4.0f, 4.0f);
            s->SetPivot(glm::vec2(0.5f, 0.5f));
            s->SetRotation(0.0f);
            s->SetTexturePath(OS_TEXT("res/image/lena.Tex2D"));
            s->SetVisible(true);

            mode_sprites.push_back(e);
        }

        // ─────────────────────────────────────────────────────────────────
        // ③ Pivot at bottom-center (foot-anchor): sprite "grows" upward from y=0
        // ─────────────────────────────────────────────────────────────────
        {
            Entity* e = CreateSpriteEntity("Sprite_PivotBottom", glm::vec3(8.0f, 0.0f, 0.0f));
            if (!e) return false;

            auto s = e->AddComponent<Sprite2DComponent>();
            s->SetFixedSize(false);
            s->SetWorldSize(4.0f, 4.0f);
            s->SetPivot(glm::vec2(0.5f, 1.0f));   // bottom-center anchor
            s->SetRotation(0.0f);
            s->SetTexturePath(OS_TEXT("res/image/lena.Tex2D"));
            s->SetVisible(true);
        }

        // ─────────────────────────────────────────────────────────────────
        // ④ Rotation: 30° around screen normal, pivot stays as rotation center
        // ─────────────────────────────────────────────────────────────────
        {
            Entity* e = CreateSpriteEntity("Sprite_Rotated30", glm::vec3(-8.0f, 6.0f, 0.0f));
            if (!e) return false;

            auto s = e->AddComponent<Sprite2DComponent>();
            s->SetFixedSize(false);
            s->SetWorldSize(4.0f, 4.0f);
            s->SetPivot(glm::vec2(0.5f, 0.5f));
            s->SetRotation(glm::radians(30.0f));
            s->SetTexturePath(OS_TEXT("res/image/lena.Tex2D"));
            s->SetVisible(true);
        }

        // ─────────────────────────────────────────────────────────────────
        // ⑤ Tint: red tint multiplied onto the texture
        // ─────────────────────────────────────────────────────────────────
        {
            Entity* e = CreateSpriteEntity("Sprite_Tinted", glm::vec3(0.0f, 6.0f, 0.0f));
            if (!e) return false;

            auto s = e->AddComponent<Sprite2DComponent>();
            s->SetFixedSize(false);
            s->SetWorldSize(4.0f, 4.0f);
            s->SetPivot(glm::vec2(0.5f, 0.5f));
            s->SetRotation(0.0f);
            s->SetTint(glm::u8vec4(255, 100, 100, 255));   // red tint
            s->SetTexturePath(OS_TEXT("res/image/lena.Tex2D"));
            s->SetVisible(true);
        }

        // ─────────────────────────────────────────────────────────────────
        // ⑥ Texture2DArray domain batching: N sprites, same domain_tag → 1 drawcall
        // ─────────────────────────────────────────────────────────────────
        {
            constexpr int   kBatchCount  = 20;
            constexpr float kSpacing     = 2.5f;
            constexpr float kStartX      = -((kBatchCount - 1) * kSpacing * 0.5f);

            for (int i = 0; i < kBatchCount; ++i)
            {
                const std::string name = std::string("Sprite_Domain_") + std::to_string(i);
                const glm::vec3   pos  = glm::vec3(kStartX + i * kSpacing, 12.0f, 0.0f);

                Entity* e = CreateSpriteEntity(name.c_str(), pos);
                if (!e) return false;

                auto s = e->AddComponent<Sprite2DComponent>();
                s->SetFixedSize(false);
                s->SetWorldSize(2.0f, 2.0f);
                s->SetPivot(glm::vec2(0.5f, 0.5f));
                s->SetRotation(0.0f);
                s->SetDomainTag("sprite_atlas_A");
                s->SetTexturePath(OS_TEXT("res/image/lena.Tex2D"));
                s->SetVisible(true);
            }
        }

        return true;
    }

    // ──────────────────────────────────────────────────────────────────────
    // ⑦ Runtime mode switching
    // ──────────────────────────────────────────────────────────────────────

    // Mode 0: AxisLocked fixed pixel (same as scenario ①)
    void ApplyModeAxisLocked()
    {
        for (auto* e : mode_sprites)
        {
            auto s = e->GetComponent<Sprite2DComponent>();
            if (!s) continue;
            s->SetFixedSize(true);
            s->SetPixelSize(256, 256);
        }
        std::cout << "[Sprite2D] Mode: AxisLocked Fixed\n";
    }

    // Mode 1: CameraFacing world-space
    void ApplyModeCameraFacing()
    {
        for (auto* e : mode_sprites)
        {
            auto s = e->GetComponent<Sprite2DComponent>();
            if (!s) continue;
            s->SetFixedSize(false);
            s->SetWorldSize(4.0f, 4.0f);
        }
        std::cout << "[Sprite2D] Mode: CameraFacing World\n";
    }

    // Mode 2: CameraFacing + 30° rotation
    void ApplyModeRotated()
    {
        for (auto* e : mode_sprites)
        {
            auto s = e->GetComponent<Sprite2DComponent>();
            if (!s) continue;
            s->SetFixedSize(false);
            s->SetWorldSize(4.0f, 4.0f);
            s->SetRotation(glm::radians(30.0f));
        }
        std::cout << "[Sprite2D] Mode: Rotated 30deg\n";
    }

    void HandleRuntimeModeSwitch()
    {
        if (!ecs_context) return;

        auto input_system = ecs_context->GetSystem<InputSystem>();
        if (!input_system) return;

        const bool key_1 = input_system->IsKeyDown(io::KeyboardButton::_1);
        const bool key_2 = input_system->IsKeyDown(io::KeyboardButton::_2);
        const bool key_3 = input_system->IsKeyDown(io::KeyboardButton::_3);

        if (key_1 && !last_key_1) ApplyModeAxisLocked();
        else if (key_2 && !last_key_2) ApplyModeCameraFacing();
        else if (key_3 && !last_key_3) ApplyModeRotated();

        last_key_1 = key_1;
        last_key_2 = key_2;
        last_key_3 = key_3;
    }

public:

    bool Init() override
    {
        SetClearColor(Color4f(0.15f, 0.15f, 0.18f, 1.0f));

        if (!InitECS())    return false;
        if (!InitCamera()) return false;
        if (!CreateSprites()) return false;

        std::cout << "[Sprite2D] Keys: [1]=AxisLocked Fixed  [2]=CameraFacing World  [3]=Rotated 30deg\n";
        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
        HandleRuntimeModeSwitch();
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<Sprite2DPerspectiveECSApp>(
        OS_TEXT("Sprite2D Perspective ECS"), argc, argv, 1280, 720);
}
