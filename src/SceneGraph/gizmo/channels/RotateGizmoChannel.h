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
};

} // namespace hgl::graph
