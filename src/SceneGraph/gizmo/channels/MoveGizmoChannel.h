#pragma once

#include "IGizmoChannel.h"

namespace hgl::graph
{

class MoveGizmoChannel final : public IGizmoChannel
{
public:
    const char *Name() const override
    {
        return "Move";
    }

    bool SupportsMode(GizmoMode mode) const override
    {
        return mode == GizmoMode::MoveWorld || mode == GizmoMode::MoveLocal;
    }

    void OnModeActivated(GizmoECS *gizmo, GizmoMode mode) override
    {
        (void)gizmo;
        (void)mode;
    }

    void OnPreUpdate(GizmoECS *gizmo,
                     const GizmoFrameInput &input,
                     const GizmoTransformSnapshot &snapshot) override
    {
        (void)gizmo;
        (void)input;
        (void)snapshot;
    }

    void OnPostUpdate(GizmoECS *gizmo,
                      const GizmoFrameInput &input,
                      const GizmoTransformSnapshot &snapshot) override
    {
        (void)gizmo;
        (void)input;
        (void)snapshot;
    }

    void RefreshHoverState(GizmoECS *gizmo,
                           const math::Vector2i &mouse_coord,
                           const CameraInfo *camera_info,
                           const ViewportInfo *viewport_info,
                           bool &handled) override;

    bool BeginDragIfNeeded(GizmoECS *gizmo,
                           const math::Vector2i &mouse_coord,
                           const CameraInfo *camera_info,
                           const ViewportInfo *viewport_info,
                           hgl::ecs::InputSystem *input_system,
                           bool has_view_context,
                           const math::Vector3f &prev_pos,
                           const glm::quat &prev_rot,
                           const math::Vector3f &prev_scale,
                           bool &handled) override;

    void EndDragIfNeeded(GizmoECS *gizmo,
                         const math::Vector2i &mouse_coord,
                         const CameraInfo *camera_info,
                         const ViewportInfo *viewport_info,
                         bool &handled) override;

    void RecoverDragIfReleaseMissed(GizmoECS *gizmo, bool &handled) override;

    bool DispatchDrag(GizmoECS *gizmo,
                      const math::Vector2i &mouse_coord,
                      const CameraInfo *camera_info,
                      const ViewportInfo *viewport_info,
                      const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                      bool has_view_context,
                      math::Vector3f &cur_effective_scale) override;
};

} // namespace hgl::graph
