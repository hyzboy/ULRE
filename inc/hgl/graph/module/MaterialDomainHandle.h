#pragma once

namespace hgl::graph
{

class ShaderMaterialProgram;
class ResourceDomain;
class DomainMaterialBinding;

/// Registry 返回的三元组，调用方拿它去创建 MI
struct MaterialDomainHandle
{
    ShaderMaterialProgram              *material = nullptr;
    ResourceDomain        *domain   = nullptr;
    DomainMaterialBinding *binding  = nullptr;

    bool IsValid() const { return material && domain && binding; }
};

} // namespace hgl::graph
