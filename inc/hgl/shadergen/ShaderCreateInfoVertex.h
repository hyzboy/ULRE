#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>

SHADERGEN_NAMESPACE_BEGIN
class ShaderCreateInfoVertex:public ShaderCreateInfo
{
    bool ProcInput(ShaderCreateInfo *) override;

public:

    ShaderCreateInfoVertex(MaterialDescriptorInfo *m):ShaderCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,m){}
    ~ShaderCreateInfoVertex()=default;

    int AddInput(const graph::VAT &type,const AnsiString &name);
    int AddInput(const AnsiString &type,const AnsiString &name);
};
SHADERGEN_NAMESPACE_END
