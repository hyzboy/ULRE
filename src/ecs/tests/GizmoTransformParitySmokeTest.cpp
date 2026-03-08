#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/systems/tick/AssetInstanceBridgeSystem.h>

#include "../../SceneGraph/gizmo/GizmoInternal.h"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>

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

    struct ScenarioResult
    {
        bool ok = false;
        bool skipped = false;
        math::Vector3f root_pos{0.0f, 0.0f, 0.0f};
        glm::quat root_rot{1.0f, 0.0f, 0.0f, 0.0f};
        math::Vector3f root_scale{1.0f, 1.0f, 1.0f};
        math::Vector3f target_pos{0.0f, 0.0f, 0.0f};
        glm::quat target_rot{1.0f, 0.0f, 0.0f, 0.0f};
        math::Vector3f target_scale{1.0f, 1.0f, 1.0f};
        GizmoMode mode = GizmoMode::MoveWorld;
    };

    ScenarioResult RunScenario(const char *backend)
    {
        ScenarioResult result{};

    #ifdef _WIN32
        _putenv_s("ULRE_GIZMO_BACKEND", backend ? backend : "legacy");
    #endif

        ECSContext ctx(std::string("GizmoParity_") + (backend ? backend : "legacy"));

        auto bridge = ctx.RegisterTickSystem<AssetInstanceBridgeSystem>();
        if (bridge)
        {
            bridge->SetWorld(&ctx);
            bridge->Initialize();
        }

        const bool is_legacy = backend && std::string(backend) == "legacy";
        if (is_legacy && !EnsureGizmoSystemResources(&ctx))
        {
            // Legacy path depends on gizmo render resources which may be unavailable in headless smoke.
            result.ok = true;
            result.skipped = true;
            return result;
        }

        GizmoECS *gizmo = CreateTransformGizmo(&ctx, "GizmoParity", math::Vector3f(1.0f, 2.0f, 3.0f));
        if (!gizmo)
            return result;

        Entity *root = GetGizmoRootEntity(gizmo);
        auto root_transform = root ? root->GetComponent<TransformComponent>() : nullptr;
        if (!root || !root_transform)
        {
            DestroyTransformGizmo(gizmo);
            return result;
        }

        Entity *target = ctx.CreateEntity<Entity>("GizmoParityTarget");
        auto target_transform = target ? target->AddComponent<TransformComponent>(Mobility::Movable) : nullptr;
        if (!target || !target_transform)
        {
            DestroyTransformGizmo(gizmo);
            return result;
        }

        const glm::quat init_rot = glm::normalize(glm::quat(0.95f, 0.1f, 0.2f, -0.1f));
        target_transform->SetLocalTRS(math::Vector3f(4.0f, 5.0f, 6.0f), init_rot, math::Vector3f(-2.0f, 0.01f, 3.0f));

        if (!BindTransformGizmoTargetEntity(gizmo, target))
        {
            DestroyTransformGizmo(gizmo);
            return result;
        }

        SetTransformGizmoMode(gizmo, GizmoMode::RotateLocal);
        SetTransformGizmoMode(gizmo, GizmoMode::ScaleLocal);
        SetTransformGizmoMode(gizmo, GizmoMode::MoveLocal);

        SetTransformGizmoVisible(gizmo, false);
        SetTransformGizmoVisible(gizmo, true);

        SetTransformGizmoAllowNegativeScale(gizmo, false);

        result.root_pos = root_transform->GetLocalPosition();
        result.root_rot = root_transform->GetLocalRotation();
        result.root_scale = root_transform->GetLocalScale();
        result.target_pos = target_transform->GetLocalPosition();
        result.target_rot = target_transform->GetLocalRotation();
        result.target_scale = target_transform->GetLocalScale();
        result.mode = GetTransformGizmoMode(gizmo);
        result.ok = true;

        DestroyTransformGizmo(gizmo);

        return result;
    }

    bool CheckScalePolicy(const math::Vector3f &scale)
    {
        return scale.x >= 0.05f && scale.y >= 0.05f && scale.z >= 0.05f;
    }

    bool RunParitySmoke()
    {
        const ScenarioResult legacy = RunScenario("legacy");
        const ScenarioResult asset = RunScenario("asset");

        if (!CheckOrLog(asset.ok, "asset scenario ok"))
            return false;

        if (legacy.skipped)
        {
            // Headless mode fallback: keep asset scenario sanity as pass condition.
            if (!CheckOrLog(CheckScalePolicy(asset.root_scale), "asset root scale policy (skip mode)"))
                return false;
            if (!CheckOrLog(CheckScalePolicy(asset.target_scale), "asset target scale policy (skip mode)"))
                return false;

            std::cout << "GizmoTransformParitySmoke: legacy path skipped (no gizmo render resources in this environment)" << std::endl;
        #ifdef _WIN32
            _putenv_s("ULRE_GIZMO_BACKEND", "legacy");
        #endif
            return true;
        }

        if (!CheckOrLog(legacy.ok, "legacy scenario ok"))
            return false;

        if (!CheckOrLog(legacy.mode == asset.mode, "mode parity"))
            return false;

        if (!CheckOrLog(glm::length(legacy.root_pos - asset.root_pos) <= 1e-5f, "root position parity"))
            return false;
        if (!CheckOrLog(std::fabs(glm::dot(legacy.root_rot, asset.root_rot)) > 0.99999f, "root rotation parity"))
            return false;
        if (!CheckOrLog(glm::length(legacy.root_scale - asset.root_scale) <= 1e-5f, "root scale parity"))
            return false;

        if (!CheckOrLog(glm::length(legacy.target_pos - asset.target_pos) <= 1e-5f, "target position parity"))
            return false;
        if (!CheckOrLog(std::fabs(glm::dot(legacy.target_rot, asset.target_rot)) > 0.99999f, "target rotation parity"))
            return false;
        if (!CheckOrLog(glm::length(legacy.target_scale - asset.target_scale) <= 1e-5f, "target scale parity"))
            return false;

        if (!CheckOrLog(CheckScalePolicy(legacy.root_scale), "legacy root scale policy"))
            return false;
        if (!CheckOrLog(CheckScalePolicy(asset.root_scale), "asset root scale policy"))
            return false;
        if (!CheckOrLog(CheckScalePolicy(legacy.target_scale), "legacy target scale policy"))
            return false;
        if (!CheckOrLog(CheckScalePolicy(asset.target_scale), "asset target scale policy"))
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
        if (!RunParitySmoke())
        {
            std::cerr << "GizmoTransformParitySmoke: FAIL" << std::endl;
            return 1;
        }

        std::cout << "GizmoTransformParitySmoke: PASS" << std::endl;
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
