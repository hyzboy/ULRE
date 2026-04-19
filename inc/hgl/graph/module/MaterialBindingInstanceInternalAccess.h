#pragma once

#include <cstdint>

namespace hgl::graph
{
    class MaterialBindingInstance;
    class ShaderMaterialProgram;
    class ResourceDomain;
    class DomainResourceBinding;

    class MaterialBindingInstanceInternalAccess final
    {
    public:

        static ShaderMaterialProgram* GetShaderMaterialProgram(MaterialBindingInstance* mi);
        static const ShaderMaterialProgram* GetShaderMaterialProgram(const MaterialBindingInstance* mi);

        static ResourceDomain* GetDomain(MaterialBindingInstance* mi);
        static const ResourceDomain* GetDomain(const MaterialBindingInstance* mi);

        static uint32_t GetDomainID(const MaterialBindingInstance* mi);

        static DomainResourceBinding* GetDomainBinding(MaterialBindingInstance* mi);
        static const DomainResourceBinding* GetDomainBinding(const MaterialBindingInstance* mi);
        static void SetDomainBinding(MaterialBindingInstance* mi, DomainResourceBinding* binding);
    };
}//namespace hgl::graph