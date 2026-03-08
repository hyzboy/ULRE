#pragma once

#include <memory>

#include <hgl/graph/gizmo/GizmoTypes.h>

namespace hgl::ecs
{
class InputSystem;
class TransformComponent;
}

namespace hgl::graph
{

struct GizmoECS;
struct CameraInfo;
class ViewportInfo;

struct GizmoFrameInput
{
    math::Vector2i mouse_coord{0, 0};
    const CameraInfo *camera_info = nullptr;
    const ViewportInfo *viewport_info = nullptr;
    hgl::ecs::InputSystem *input_system = nullptr;
    bool left_down = false;
    bool left_pressed = false;
    bool left_released = false;
};

struct GizmoTransformSnapshot
{
    math::Vector3f position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    math::Vector3f scale{1.0f, 1.0f, 1.0f};
};

class IGizmoChannel
{
public:
    virtual ~IGizmoChannel() = default;

    virtual const char *Name() const = 0;
    virtual bool SupportsMode(GizmoMode mode) const = 0;

    // Lifecycle hooks are default-noop during migration and become behavior hosts later.
    virtual void OnModeActivated(GizmoECS *gizmo, GizmoMode mode)
    {
        (void)gizmo;
        (void)mode;
    }

    virtual void OnPreUpdate(GizmoECS *gizmo,
                             const GizmoFrameInput &input,
                             const GizmoTransformSnapshot &snapshot)
    {
        (void)gizmo;
        (void)input;
        (void)snapshot;
    }

    virtual void OnPostUpdate(GizmoECS *gizmo,
                              const GizmoFrameInput &input,
                              const GizmoTransformSnapshot &snapshot)
    {
        (void)gizmo;
        (void)input;
        (void)snapshot;
    }

    virtual void RefreshHoverState(GizmoECS *gizmo,
                                   const math::Vector2i &mouse_coord,
                                   const CameraInfo *camera_info,
                                   const ViewportInfo *viewport_info,
                                   bool &handled)
    {
        (void)gizmo;
        (void)mouse_coord;
        (void)camera_info;
        (void)viewport_info;
        handled = false;
    }

    // handled=true means channel consumed begin-drag path (started drag or made final decision).
    // return=true means caller should abort current frame update (e.g. mouse capture failure).
    virtual bool BeginDragIfNeeded(GizmoECS *gizmo,
                                   const math::Vector2i &mouse_coord,
                                   const CameraInfo *camera_info,
                                   const ViewportInfo *viewport_info,
                                   hgl::ecs::InputSystem *input_system,
                                   bool has_view_context,
                                   const math::Vector3f &prev_pos,
                                   const glm::quat &prev_rot,
                                   const math::Vector3f &prev_scale,
                                   bool &handled)
    {
        (void)gizmo;
        (void)mouse_coord;
        (void)camera_info;
        (void)viewport_info;
        (void)input_system;
        (void)has_view_context;
        (void)prev_pos;
        (void)prev_rot;
        (void)prev_scale;
        handled = false;
        return false;
    }

    virtual void EndDragIfNeeded(GizmoECS *gizmo,
                                 const math::Vector2i &mouse_coord,
                                 const CameraInfo *camera_info,
                                 const ViewportInfo *viewport_info,
                                 bool &handled)
    {
        (void)gizmo;
        (void)mouse_coord;
        (void)camera_info;
        (void)viewport_info;
        handled = false;
    }

    virtual void RecoverDragIfReleaseMissed(GizmoECS *gizmo, bool &handled)
    {
        (void)gizmo;
        handled = false;
    }

    // Returns true when this channel handled drag dispatch for the current frame.
    virtual bool DispatchDrag(GizmoECS *gizmo,
                              const math::Vector2i &mouse_coord,
                              const CameraInfo *camera_info,
                              const ViewportInfo *viewport_info,
                              const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                              bool has_view_context,
                              math::Vector3f &cur_effective_scale)
    {
        (void)gizmo;
        (void)mouse_coord;
        (void)camera_info;
        (void)viewport_info;
        (void)target_transform;
        (void)has_view_context;
        (void)cur_effective_scale;
        return false;
    }
};

} // namespace hgl::graph
