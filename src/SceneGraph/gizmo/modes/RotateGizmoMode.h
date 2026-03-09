#pragma once

#include <memory>
#include <vector>
#include "GizmoModeRuntime.h"

namespace hgl::ecs
{
class AssetInstanceComponent;
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

// Owns all visual + drag state for the Rotate gizmo mode.
// No method here takes GizmoECS* — all dependencies pass through explicit parameters.
class RotateGizmoMode
{
public:
    // ─── Visual entities (owned, not in GizmoECS) ─────────────────────────
    hgl::ecs::Entity                                    *entity        = nullptr;
    std::shared_ptr<hgl::ecs::AssetInstanceComponent>   asset_instance;
    std::vector<GizmoVisualPrimitive>                    primitives;

    // Extra transform handle for the white view-facing ring.
    std::shared_ptr<hgl::ecs::TransformComponent>        aux_transform;

    // ─── Hover state ──────────────────────────────────────────────────────
    int hovered_index = -1;

    // ─── Full drag state ──────────────────────────────────────────────────
    RotateDragState drag;

    // ─── Lifecycle ────────────────────────────────────────────────────────
    void BuildVisual(hgl::ecs::ECSContext *world,
                     hgl::ecs::Entity *parent,
                     std::vector<hgl::ecs::EntityID> &entity_ids);

    void DestroyVisual();
    void SetVisible(bool visible);

    // ─── Per-frame update ─────────────────────────────────────────────────
    void UpdateHover(const hgl::math::Vector2i &mouse,
                     const CameraInfo *cam,
                     const ViewportInfo *vp,
                     const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform);

    // ─── Drag lifecycle ───────────────────────────────────────────────────
    bool TryBeginDrag(const hgl::math::Vector2i &mouse,
                      const CameraInfo *cam,
                      const ViewportInfo *vp,
                      hgl::ecs::InputSystem *input_sys,
                      bool has_view_context,
                      const hgl::math::Vector3f &prev_pos,
                      const glm::quat &prev_rot,
                      const hgl::math::Vector3f &prev_scale,
                      GizmoMode current_mode,
                      bool root_visible);

    void ApplyDrag(const hgl::math::Vector2i &mouse,
                   const CameraInfo *cam,
                   const ViewportInfo *vp,
                   const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform);

    void EndDrag();
    void RecoverIfOrphaned(bool left_down);

    bool IsDragging() const { return drag.active; }
};

} // namespace hgl::graph
