#pragma once

#include "IGizmoChannel.h"

namespace hgl::graph
{

class ScaleGizmoChannel final : public IGizmoChannel
{
public:
    const char *Name() const override
    {
        return "Scale";
    }

    bool SupportsMode(GizmoMode mode) const override
    {
        return mode == GizmoMode::ScaleLocal;
    }
};

} // namespace hgl::graph
