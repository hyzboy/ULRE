#include<hgl/shadergen/ShaderCreateInfoGeometry.h>

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

        bool ShaderCreateInfoGeometry::ProcLayout()
        {
            final_shader+="layout("+input_prim+") in;\n"
                          "layout("+output_prim+", max_vertices = "+AnsiString::numberOf(max_vertices)+") out;\n";

            return(true);
        }
    }//namespace graph
}//namespace hgl
