#pragma once

#include <hgl/graph/IDDHandle.h>

namespace hgl::graph
{

class MaterialTemplate;
class DomainMaterialBinding;

/// Registry 返回的三元组，调用方拿它去创建 MI
struct MaterialDomainHandle
{
    MaterialTemplate              *material      = nullptr;
    IDDHandle                      idd_handle;
    DomainMaterialBinding         *binding       = nullptr;

    bool IsValid() const { return material && idd_handle.IsValid() && binding; }
};

} // namespace hgl::graph
