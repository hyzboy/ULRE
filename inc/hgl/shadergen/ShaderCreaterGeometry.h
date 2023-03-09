#pragma once

#include<hgl/shadergen/ShaderCreater.h>

SHADERGEN_NAMESPACE_BEGIN
class ShaderCreaterGeometry:public ShaderCreater
{
public:

    ShaderCreaterGeometry(MaterialDescriptorManager *m):ShaderCreater(VK_SHADER_STAGE_GEOMETRY_BIT,m){}
    ~ShaderCreaterGeometry()=default;
};
SHADERGEN_NAMESPACE_END