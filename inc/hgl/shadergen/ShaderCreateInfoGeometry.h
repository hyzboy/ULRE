#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl::graph
{
    class ShaderCreateInfoGeometry:public ShaderCreateInfo
    {
        GeometryShaderDescriptorInfo gsdi;

        std::string input_prim;
        std::string output_prim;
        uint32_t max_vertices;

    public:

        bool IsEmptyOutput()const override{return gsdi.IsEmptyOutput();}
        void GetOutputStrcutString(std::string &str) override;

        ShaderDescriptorInfo *GetSDI()override{return &gsdi;}

    public:

        ShaderCreateInfoGeometry(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&gsdi,m);}
        ~ShaderCreateInfoGeometry()override=default;

        bool SetGeom(const PrimitiveType &ip,const PrimitiveType &op,const uint32_t mv);

        int AddOutput(SVList &);
        int AddOutput(const ShaderVariableType &type,const std::string &name,Interpolation inter=Interpolation::Smooth);
        int AddOutput(const ShaderVariableType &type,const char *name,Interpolation inter=Interpolation::Smooth)
        {
            return AddOutput(type,std::string(name?name:""),inter);
        }
        void AddMaterialInstanceOutput() override;

        bool ProcLayout() override;
    };
}//namespace hgl::graph
