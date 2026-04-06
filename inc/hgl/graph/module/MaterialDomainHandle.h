#pragma once

namespace hgl::graph
{

class ShaderProgram;
class MaterialResourceDomain;
class DomainMaterialBinding;

/// Registry 返回的三元组，调用方拿它去创建 MI
struct MaterialDomainHandle
{
    ShaderProgram              *material = nullptr;
    MaterialResourceDomain        *domain   = nullptr;
    DomainMaterialBinding *binding  = nullptr;

    bool IsValid() const { return material && domain && binding; }
};

} // namespace hgl::graph
