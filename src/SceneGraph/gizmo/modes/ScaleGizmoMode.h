#pragma once

#include <memory>
#include <vector>
#include "GizmoModeRuntime.h"

namespace hgl::ecs
{
class ECSContext;
class Entity;
struct EntityID;
class InputSystem;
class TransformComponent;
} // namespace hgl::ecs

namespace hgl::graph
{

struct CameraInfo;
struct ViewportInfo;

// Owns all visual + drag state for the Scale gizmo mode.
// No method here takes GizmoECS* — all dependencies pass through explicit parameters.
class ScaleGizmoMode
{
public:
    // ─── Visual entities (owned, not in GizmoECS) ─────────────────────────
    hgl::ecs::Entity                                    *entity        = nullptr;
    
    std::vector<GizmoVisualPrimitive>                    primitives;

    // ─── Hover state ──────────────────────────────────────────────────────
    int hovered_index = -1;

    // ─── Full drag state ──────────────────────────────────────────────────
    GizmoDragState drag;

    // ─── Lifecycle ────────────────────────────────────────────────────────
    void BuildVisual(hgl::ecs::ECSContext *world,
                     hgl::ecs::Entity *parent,
                     std::vector<hgl::ecs::EntityID> &entity_ids);

    void DestroyVisual();
    void SetVisible(bool visible);

    // ─── Per-frame update ─────────────────────────────────────────────────
    void UpdateHover(const GizmoFrameInput &input,
                     const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform);

    // ─── Drag lifecycle ───────────────────────────────────────────────────
    bool TryBeginDrag(const GizmoFrameInput &input,
                      const GizmoPrevTransform &prev,
                      GizmoMode current_mode,
                      bool root_visible);

    // Apply current drag delta. Scale needs cur_effective_scale for CommitTransformChanges.
    void ApplyDrag(const GizmoFrameInput &input,
                   bool allow_negative_scale,
                   const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                   const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform,
                   hgl::math::Vector3f &cur_effective_scale);

    // Release mouse capture and reset all drag state.
    void EndDrag();

    // If left button is up while drag is still active, we missed the release — end it.
    void RecoverIfOrphaned(bool left_down);

    bool IsDragging() const { return drag.active; }
};

} // namespace hgl::graph
