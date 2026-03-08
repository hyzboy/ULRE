#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/AssetInstanceComponent.h>
#include <hgl/ecs/components/SubWorldComponent.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/systems/tick/AssetInstanceBridgeSystem.h>

#include "../../SceneGraph/gizmo/GizmoInternal.h"

#include <cstdlib>
#include <cmath>
#include <exception>
#include <iostream>
#include <vector>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

namespace
{
    bool CheckOrLog(bool condition, const char *message)
    {
        if (!condition)
            std::cerr << "CHECK FAILED: " << message << std::endl;

        return condition;
    }

    Entity *FindEntityByName(ECSContext &ctx, const char *name)
    {
        std::vector<Entity *> entities;
        ctx.GetAllEntities(entities);

        for (auto *e : entities)
        {
            if (e && e->GetName() == name)
                return e;
        }

        return nullptr;
    }

    bool RunGizmoAssetBackendSmoke()
    {
    #ifdef _WIN32
        _putenv_s("ULRE_GIZMO_BACKEND", "asset");
    #endif

        ECSContext ctx("GizmoAssetBackendSmoke");

        auto bridge = ctx.RegisterTickSystem<AssetInstanceBridgeSystem>();
        if (!CheckOrLog(bridge != nullptr, "bridge create"))
            return false;

        bridge->SetWorld(&ctx);
        bridge->Initialize();

        GizmoECS *gizmo = CreateTransformGizmo(&ctx, "GizmoAssetSmoke", math::Vector3f(1.0f, 2.0f, 3.0f));
        if (!CheckOrLog(gizmo != nullptr, "create gizmo"))
            return false;

        Entity *root = GetGizmoRootEntity(gizmo);
        if (!CheckOrLog(root != nullptr, "root exists"))
            return false;

        auto root_transform = root->GetComponent<TransformComponent>();
        if (!CheckOrLog(root_transform != nullptr, "root transform exists"))
            return false;

        Entity *target = ctx.CreateEntity<Entity>("GizmoTarget");
        if (!CheckOrLog(target != nullptr, "create target"))
            return false;

        auto target_transform = target->AddComponent<TransformComponent>(Mobility::Movable);
        if (!CheckOrLog(target_transform != nullptr, "target transform exists"))
            return false;

        target_transform->SetLocalTRS(math::Vector3f(5.0f, 6.0f, 7.0f),
                                      glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                      math::Vector3f(1.0f, 1.0f, 1.0f));

        if (!CheckOrLog(BindTransformGizmoTargetEntity(gizmo, target), "bind target"))
            return false;

        uint32_t changed_count = 0;
        SetTransformGizmoChangedCallback(gizmo, [&changed_count](const GizmoTransformChange&)
        {
            ++changed_count;
        });

        auto run_interaction_sequence = [&](const math::Vector3f &start_pos,
                                            const glm::quat &start_rot,
                                            const math::Vector3f &start_scale)
        {
            target_transform->SetLocalTRS(start_pos, start_rot, start_scale);
            root_transform->SetLocalTRS(start_pos, start_rot, start_scale);

            SetTransformGizmoMode(gizmo, GizmoMode::MoveWorld);
            UpdateTransformGizmo(gizmo, math::Vector2i(100, 100), nullptr, nullptr, nullptr, true, true, false);
            UpdateTransformGizmo(gizmo, math::Vector2i(140, 120), nullptr, nullptr, nullptr, true, false, false);
            UpdateTransformGizmo(gizmo, math::Vector2i(140, 120), nullptr, nullptr, nullptr, false, false, true);

            SetTransformGizmoMode(gizmo, GizmoMode::RotateLocal);
            UpdateTransformGizmo(gizmo, math::Vector2i(200, 200), nullptr, nullptr, nullptr, true, true, false);
            UpdateTransformGizmo(gizmo, math::Vector2i(260, 170), nullptr, nullptr, nullptr, true, false, false);
            UpdateTransformGizmo(gizmo, math::Vector2i(260, 170), nullptr, nullptr, nullptr, false, false, true);

            SetTransformGizmoMode(gizmo, GizmoMode::ScaleLocal);
            UpdateTransformGizmo(gizmo, math::Vector2i(300, 200), nullptr, nullptr, nullptr, true, true, false);
            UpdateTransformGizmo(gizmo, math::Vector2i(300, 160), nullptr, nullptr, nullptr, true, false, false);
            UpdateTransformGizmo(gizmo, math::Vector2i(300, 160), nullptr, nullptr, nullptr, false, false, true);

            struct Result
            {
                math::Vector3f position;
                glm::quat rotation;
                math::Vector3f scale;
                uint32_t changed_count_total = 0;
            };

            Result r{};
            r.position = root_transform->GetLocalPosition();
            r.rotation = root_transform->GetLocalRotation();
            r.scale = root_transform->GetLocalScale();
            r.changed_count_total = changed_count;
            return r;
        };

        Entity *move = FindEntityByName(ctx, "Gizmo_Move");
        Entity *rotate = FindEntityByName(ctx, "Gizmo_Rotate");
        Entity *scale = FindEntityByName(ctx, "Gizmo_Scale");
        if (!CheckOrLog(move && rotate && scale, "mode entities exist"))
            return false;

        auto move_ai = move->GetComponent<AssetInstanceComponent>();
        auto rotate_ai = rotate->GetComponent<AssetInstanceComponent>();
        auto scale_ai = scale->GetComponent<AssetInstanceComponent>();
        if (!CheckOrLog(move_ai && rotate_ai && scale_ai, "asset instance components exist"))
            return false;

        bridge->Update(0.016f);
        const auto s0 = bridge->GetStats();
        if (!CheckOrLog(s0.runtime_state_count == 3u, "bridge runtime states after create"))
            return false;
        if (!CheckOrLog(s0.emitted_draw_packet_count_frame == 3u, "bridge draw packets after create"))
            return false;

        if (!CheckOrLog(!move->HasComponent<SubWorldComponent>(), "move has no subworld"))
            return false;
        if (!CheckOrLog(!rotate->HasComponent<SubWorldComponent>(), "rotate has no subworld"))
            return false;
        if (!CheckOrLog(!scale->HasComponent<SubWorldComponent>(), "scale has no subworld"))
            return false;

        if (!CheckOrLog(move_ai->GetVisibilityMask() == ~0ull, "move active on create"))
            return false;
        if (!CheckOrLog(rotate_ai->GetVisibilityMask() == 0ull, "rotate inactive on create"))
            return false;
        if (!CheckOrLog(scale_ai->GetVisibilityMask() == 0ull, "scale inactive on create"))
            return false;

        const uint32_t rev_move_before = move_ai->GetOverrideRef().revision;
        const uint32_t rev_rotate_before = rotate_ai->GetOverrideRef().revision;

        SetTransformGizmoMode(gizmo, GizmoMode::RotateLocal);
        if (!CheckOrLog(move_ai->GetVisibilityMask() == 0ull, "move inactive after rotate mode"))
            return false;
        if (!CheckOrLog(rotate_ai->GetVisibilityMask() == ~0ull, "rotate active after rotate mode"))
            return false;
        if (!CheckOrLog(rotate_ai->GetOverrideRef().revision > rev_rotate_before, "rotate revision bumped"))
            return false;
        if (!CheckOrLog(move_ai->GetOverrideRef().revision > rev_move_before, "move revision bumped"))
            return false;

        bridge->Update(0.016f);
        const auto s1 = bridge->GetStats();
        if (!CheckOrLog(s1.rebuild_count_frame >= 1u, "bridge rebuilds after mode switch"))
            return false;

        SetTransformGizmoVisible(gizmo, false);
        if (!CheckOrLog(move_ai->GetVisibilityMask() == 0ull, "move hidden by root"))
            return false;
        if (!CheckOrLog(rotate_ai->GetVisibilityMask() == 0ull, "rotate hidden by root"))
            return false;
        if (!CheckOrLog(scale_ai->GetVisibilityMask() == 0ull, "scale hidden by root"))
            return false;

        SetTransformGizmoVisible(gizmo, true);
        if (!CheckOrLog(rotate_ai->GetVisibilityMask() == ~0ull, "rotate restored by root visible"))
            return false;

        // Deterministic interaction smoke: run identical input sequence twice.
        const auto result1 = run_interaction_sequence(math::Vector3f(5.0f, 6.0f, 7.0f),
                                                      glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                                      math::Vector3f(1.0f, 1.0f, 1.0f));

        if (!CheckOrLog(glm::length(target_transform->GetLocalPosition() - result1.position) <= 1e-5f, "target follows sequence result1"))
            return false;

        const math::Vector3f expected_pos(5.4f, 5.8f, 7.0f);
        if (!CheckOrLog(glm::length(result1.position - expected_pos) <= 1e-4f, "result1 deterministic position"))
            return false;

        const glm::quat expected_rot = glm::normalize(
            glm::angleAxis(-60.0f * 0.005f, math::AxisVector::Y)
            * glm::angleAxis(30.0f * 0.005f, math::AxisVector::X));
        if (!CheckOrLog(std::fabs(glm::dot(expected_rot, result1.rotation)) > 0.9995f, "result1 deterministic quaternion"))
            return false;

        if (!CheckOrLog(glm::length(result1.scale - math::Vector3f(1.4f, 1.4f, 1.4f)) <= 1e-4f, "result1 deterministic scale"))
            return false;

        const auto result2 = run_interaction_sequence(math::Vector3f(5.0f, 6.0f, 7.0f),
                                                      glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                                      math::Vector3f(1.0f, 1.0f, 1.0f));

        if (!CheckOrLog(glm::length(result2.position - result1.position) <= 1e-6f, "replay deterministic position"))
            return false;
        if (!CheckOrLog(std::fabs(glm::dot(result2.rotation, result1.rotation)) > 0.999999f, "replay deterministic rotation"))
            return false;
        if (!CheckOrLog(glm::length(result2.scale - result1.scale) <= 1e-6f, "replay deterministic scale"))
            return false;
        if (!CheckOrLog(result2.changed_count_total > result1.changed_count_total, "replay callback increment"))
            return false;

        if (!CheckOrLog(changed_count > 0u, "changed callback fired"))
            return false;

        bridge->Update(0.016f);
        const auto s2 = bridge->GetStats();
        if (!CheckOrLog(s2.runtime_state_count == 3u, "bridge runtime states after interactions"))
            return false;
        if (!CheckOrLog(s2.emitted_draw_packet_count_frame == 3u, "bridge draw packets after interactions"))
            return false;

        DestroyTransformGizmo(gizmo);
        if (!CheckOrLog(FindEntityByName(ctx, "GizmoAssetSmoke") == nullptr, "root destroyed"))
            return false;
        if (!CheckOrLog(FindEntityByName(ctx, "Gizmo_Move") == nullptr, "move destroyed"))
            return false;
        if (!CheckOrLog(FindEntityByName(ctx, "Gizmo_Rotate") == nullptr, "rotate destroyed"))
            return false;
        if (!CheckOrLog(FindEntityByName(ctx, "Gizmo_Scale") == nullptr, "scale destroyed"))
            return false;

    #ifdef _WIN32
        _putenv_s("ULRE_GIZMO_BACKEND", "legacy");
    #endif

        return true;
    }
}

int main()
{
    try
    {
        if (!RunGizmoAssetBackendSmoke())
        {
            std::cerr << "GizmoAssetBackendSmoke: FAIL" << std::endl;
            return 1;
        }

        std::cout << "GizmoAssetBackendSmoke: PASS" << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 2;
    }
    catch (...)
    {
        std::cerr << "Unknown exception" << std::endl;
        return 3;
    }
}
