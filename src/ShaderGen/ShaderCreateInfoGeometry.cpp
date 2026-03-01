#include<hgl/shadergen/ShaderCreateInfoGeometry.h>
#include<string>
#include"common/MFCommon.h"

namespace hgl
{
    namespace graph
    {
        bool ShaderCreateInfoGeometry::SetGeom(const PrimitiveType &ip,const PrimitiveType &op,const uint32_t mv)
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

        int ShaderCreateInfoGeometry::AddOutput(SVList &sv_list)
        {
            int count=0;

            for(ShaderVariable &sv:sv_list)
            {
                sv.interpolation=Interpolation::Smooth;

                if(gsdi.AddOutput(sv))
                    ++count;
            }

            return count;
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
            std::string block;
            block += "layout(";
            block += input_prim.c_str()?input_prim.c_str():"";
            block += ") in;\n";

            block += "layout(";
            block += output_prim.c_str()?output_prim.c_str():"";
            block += ", max_vertices = ";
            const std::string max_vertices_str=std::to_string(max_vertices);
            block += max_vertices_str;
            block += ") out;\n";

            final_shader += block.c_str();

            return(true);
        }
    }//namespace graph
}//namespace hgl
