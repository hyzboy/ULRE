#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderStageIO.h>
#include<hgl/mtl/VertexAttributeSpec.h>

namespace hgl::graph
{
    class ShaderCreateInfoVertex:public ShaderCreateInfo
    {
        VertexShaderStageIO *vsdi;

    public:

        VIAArray &GetInput(){return vsdi->GetInput();}
        const VIAArray &GetInput()const{return vsdi->GetInput();}

    public:

        ShaderCreateInfoVertex(MaterialDescriptorDB *m);
        ~ShaderCreateInfoVertex()override=default;

        // DEPRECATED: Direct VIA list bypasses spec validation. Use AddInput(VertexAttributeSpec) instead.
        [[deprecated("Use AddInput(VertexAttributeSpec) instead")]]
        int AddInput(VIAList &);

        // DEPRECATED: VAType-only conversion. Use AddInput(VertexAttributeSpec) for new code.
        [[deprecated("Use AddInput(VertexAttributeSpec) instead")]]
        int AddInput(const VAType &type,const VertexAttrib attrib);
        
        // Spec-based path (preferred): Validates storage format compatibility with shader type.
        int AddInput(const mtl::VertexAttributeSpec &spec);
        
        // Bulk spec-based path: Adds multiple attributes from a VertexAttributeSpec array.
        int AddInput(const mtl::VertexAttributeSpec *specs, uint count);
    };//class ShaderCreateInfoVertex:public ShaderCreateInfo
}//namespace hgl::graph
