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
};

} // namespace hgl::graph
