#pragma once

#include <memory>
#include <vector>
#include "GizmoModeRuntime.h"

namespace hgl::ecs
{
class ECSContext;
class Entity;
class EntityID;
class InputSystem;
class TransformComponent;
} // namespace hgl::ecs

namespace hgl::graph
{

struct CameraInfo;
struct ViewportInfo;

// Owns all visual + drag state for the Move gizmo mode.
// No method here takes GizmoECS* — all dependencies pass through explicit parameters.
class MoveGizmoMode
{
public:
    // ─── Visual entities (owned, not in GizmoECS) ─────────────────────────
    hgl::ecs::Entity                                    *entity        = nullptr;

    std::vector<GizmoVisualPrimitive>                    primitives;

    // ─── Hover state ──────────────────────────────────────────────────────
    int hovered_index = -1;

    // ─── Full drag state (replaces GizmoECS::AssetDragState for Move) ─────
    GizmoDragState drag;

    // ─── Lifecycle ────────────────────────────────────────────────────────
    // Build child visual entities under `parent`.
    // `entity_ids` receives the IDs of all created entities for batch cleanup.
    void BuildVisual(hgl::ecs::ECSContext *world,
                     hgl::ecs::Entity *parent,
                     std::vector<hgl::ecs::EntityID> &entity_ids);

    void DestroyVisual();
    void SetVisible(bool visible);

    // ─── Per-frame update ─────────────────────────────────────────────────
    // Compute hover pick, update hovered_index + drag.pick, apply visual highlight.
    void UpdateHover(const GizmoFrameInput &input,
                     const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform);

    // ─── Drag lifecycle ───────────────────────────────────────────────────
    // Attempt to begin a drag from the current hover state.
    // Returns true if mouse capture failed and the caller should abort.
    bool TryBeginDrag(const GizmoFrameInput &input,
                      const GizmoPrevTransform &prev,
                      GizmoMode current_mode,
                      bool root_visible);

    // Apply current drag delta, writing the new position into root_transform.
    void ApplyDrag(const GizmoFrameInput &input,
                   const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform);

    // Release mouse capture and reset all drag state.
    void EndDrag();

    // If left button is up while drag is still active, we missed the release — end it.
    void RecoverIfOrphaned(bool left_down);

    bool IsDragging() const { return drag.active; }
};

} // namespace hgl::graph
