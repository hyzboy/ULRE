#include<hgl/shadergen/ShaderCreateInfoGeometry.h>
#include"common/MFCommon.h"

namespace hgl
{
    namespace graph
    {
        bool ShaderCreateInfoGeometry::SetGeom(const Prim &ip,const Prim &op,const uint32_t mv)
        {
            if(!CheckGeometryShaderIn(ip))return(false);
            if(!CheckGeometryShaderOut(op))return(false);

            if(mv==0)
                return(false);

            input_prim=GetPrimName(ip);
            output_prim=GetPrimName(op);

            max_vertices=mv;
            return(true);
        }

        void ShaderCreateInfoGeometry::AddMaterialInstanceOutput()
        {
            AddOutput(SVT_UINT,mtl::func::MI_ID_OUTPUT,Interpolation::Flat);
            AddFunction(mtl::func::MF_HandoverMI_GS);
        }

        int ShaderCreateInfoGeometry::AddOutput(const SVType &type,const AnsiString &name,Interpolation inter)
        {
            ShaderVariable sv;
    
            hgl::strcpy(sv.name,sizeof(sv.name),name.c_str());

            sv.type=type;
            sv.interpolation=inter;

            return gsdi.AddOutput(sv);
        }
        
        void ShaderCreateInfoGeometry::GetOutputStrcutString(AnsiString &str)
        {
            gsdi.GetOutput().ToString(str);
        }

        bool ShaderCreateInfoGeometry::ProcLayout()
        {
            final_shader+="layout("+input_prim+") in;\n"
                          "layout("+output_prim+", max_vertices = "+AnsiString::numberOf(max_vertices)+") out;\n";

            return(true);
        }
    }//namespace graph
}//namespace hgl
