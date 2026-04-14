#pragma once

#include <hgl/graph/MRDHandle.h>

namespace hgl::graph
{

class MaterialTemplate;
class DomainMaterialBinding;

/// Registry 返回的三元组，调用方拿它去创建 MI
struct MaterialDomainHandle
{
    MaterialTemplate              *material      = nullptr;
    MRDHandle                      domain_handle;
    DomainMaterialBinding         *binding       = nullptr;

    bool IsValid() const { return material && domain_handle.IsValid() && binding; }
};

} // namespace hgl::graph
