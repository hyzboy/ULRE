#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>

namespace hgl{namespace graph{
class ShaderCreateInfoGeometry:public ShaderCreateInfo
{
    AnsiString input_prim;
    AnsiString output_prim;
    uint32_t max_vertices;    

public:

    ShaderCreateInfoGeometry(MaterialDescriptorInfo *m):ShaderCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT,m){}
    ~ShaderCreateInfoGeometry()=default;

    bool SetGeom(const Prim &ip,const Prim &op,const uint32_t mv);

    bool ProcLayout() override;
};
}}//namespace hgl::graph