#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/AssetInstanceComponent.h>
#include <hgl/ecs/components/SubWorldComponent.h>
#include <hgl/ecs/systems/tick/AssetInstanceBridgeSystem.h>

#include "../../SceneGraph/gizmo/GizmoInternal.h"

#include <cstdlib>
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
