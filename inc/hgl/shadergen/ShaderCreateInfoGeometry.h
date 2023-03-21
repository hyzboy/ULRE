#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>

namespace hgl{namespace graph{
class ShaderCreateInfoGeometry:public ShaderCreateInfo
{
public:

    ShaderCreateInfoGeometry(MaterialDescriptorInfo *m):ShaderCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT,m){}
    ~ShaderCreateInfoGeometry()=default;
};
}}//namespace hgl::graph