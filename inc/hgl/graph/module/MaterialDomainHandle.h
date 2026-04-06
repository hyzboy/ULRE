#pragma once

namespace hgl::graph
{

class MaterialTemplate;
class MaterialResourceDomain;
class DomainMaterialBinding;

/// Registry 返回的三元组，调用方拿它去创建 MI
struct MaterialDomainHandle
{
    MaterialTemplate              *material = nullptr;
    MaterialResourceDomain        *domain   = nullptr;
    DomainMaterialBinding *binding  = nullptr;

    bool IsValid() const { return material && domain && binding; }
};

} // namespace hgl::graph
