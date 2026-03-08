#pragma once

#include "IGizmoChannel.h"

namespace hgl::graph
{

class RotateGizmoChannel final : public IGizmoChannel
{
public:
    const char *Name() const override
    {
        return "Rotate";
    }

    bool SupportsMode(GizmoMode mode) const override
    {
        return mode == GizmoMode::RotateWorld || mode == GizmoMode::RotateLocal;
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
};

} // namespace hgl::graph
