#pragma once

#include <cstdint>

namespace hgl::graph
{
    class MaterialBindingInstance;
    class ShaderMaterialProgram;
    class ResourceDomain;

    class MaterialBindingInstanceInternalAccess final
    {
    public:

        static ShaderMaterialProgram* GetShaderMaterialProgram(MaterialBindingInstance* mi);
        static const ShaderMaterialProgram* GetShaderMaterialProgram(const MaterialBindingInstance* mi);

        static ResourceDomain* GetDomain(MaterialBindingInstance* mi);
        static const ResourceDomain* GetDomain(const MaterialBindingInstance* mi);

        static uint32_t GetDomainID(const MaterialBindingInstance* mi);
    };
}//namespace hgl::graph