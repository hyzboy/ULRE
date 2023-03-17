#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>

SHADERGEN_NAMESPACE_BEGIN
class ShaderCreateInfoGeometry:public ShaderCreateInfo
{
public:

    ShaderCreateInfoGeometry(MaterialDescriptorManager *m):ShaderCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT,m){}
    ~ShaderCreateInfoGeometry()=default;
};
SHADERGEN_NAMESPACE_END