#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl::graph
{
    class ShaderCreateInfoGeometry:public ShaderCreateInfo
    {
        GeometryShaderDescriptorInfo gsdi;

        AnsiString input_prim;
        AnsiString output_prim;
        uint32_t max_vertices;

    public:
    
        bool IsEmptyOutput()const override{return gsdi.IsEmptyOutput();}
        void GetOutputStrcutString(AnsiString &str) override;

        ShaderDescriptorInfo *GetSDI()override{return &gsdi;}

    public:

        ShaderCreateInfoGeometry(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&gsdi,m);}
        ~ShaderCreateInfoGeometry()override=default;

        bool SetGeom(const PrimitiveType &ip,const PrimitiveType &op,const uint32_t mv);
    
        int AddOutput(SVList &);
        int AddOutput(const ShaderVariableType &type,const AnsiString &name,Interpolation inter=Interpolation::Smooth);
        void AddMaterialInstanceOutput() override;

        bool ProcLayout() override;
    };
}//namespace hgl::graph
