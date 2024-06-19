﻿#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl{namespace graph{
class ShaderCreateInfoGeometry:public ShaderCreateInfo
{
    GeometryShaderDescriptorInfo gsdi;

    AnsiString input_prim;
    AnsiString output_prim;
    uint32_t max_vertices;

public:

    ShaderDescriptorInfo *GetSDI()override{return &gsdi;}

public:

    ShaderCreateInfoGeometry(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&gsdi,m);}
    ~ShaderCreateInfoGeometry()override=default;

    bool SetGeom(const Prim &ip,const Prim &op,const uint32_t mv);
    
    int AddOutput(const ShaderVariableType &type,const AnsiString &name,Interpolation inter=Interpolation::Smooth);
    void AddMaterialInstanceOutput() override;

    bool ProcLayout() override;
};
}}//namespace hgl::graph