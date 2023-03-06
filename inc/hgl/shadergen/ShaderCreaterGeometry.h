#pragma once

#include<hgl/shadergen/ShaderCreater.h>

SHADERGEN_NAMESPACE_BEGIN
class ShaderCreaterGeometry:public ShaderCreater
{
public:

    ShaderCreaterGeometry():ShaderCreater(VK_SHADER_STAGE_GEOMETRY_BIT){}
    ~ShaderCreaterGeometry()=default;
};
SHADERGEN_NAMESPACE_END