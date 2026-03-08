#pragma once

#include <memory>
#include <vector>
#include "GizmoModeRuntime.h"

namespace hgl::ecs
{
class AssetInstanceComponent;
class Entity;
} // namespace hgl::ecs

namespace hgl::graph
{

// Owns all visual + pick state for the Move gizmo mode.
// Behavior methods will be added in Phase 3.
class MoveGizmoMode
{
public:
    // ─── Visual entities (owned, not in GizmoECS) ─────────────────────────
    hgl::ecs::Entity                                    *entity        = nullptr;
    std::shared_ptr<hgl::ecs::AssetInstanceComponent>   asset_instance;
    std::vector<GizmoVisualPrimitive>                    primitives;

    // ─── Hover state ──────────────────────────────────────────────────────
    int hovered_index = -1;

    // ─── Pick snapshot captured at drag-begin ─────────────────────────────
    GizmoPickState pick_state;
};

} // namespace hgl::graph
