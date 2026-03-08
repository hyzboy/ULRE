#pragma once

#include <hgl/graph/gizmo/GizmoTypes.h>

namespace hgl::graph
{

class IGizmoChannel
{
public:
    virtual ~IGizmoChannel() = default;

    virtual const char *Name() const = 0;
    virtual bool SupportsMode(GizmoMode mode) const = 0;
};

} // namespace hgl::graph
