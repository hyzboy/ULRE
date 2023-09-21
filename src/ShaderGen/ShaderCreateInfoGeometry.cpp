#include<hgl/shadergen/ShaderCreateInfoGeometry.h>

namespace hgl
{
    namespace graph
    {
        bool ShaderCreateInfoGeometry::SetGeom(const Prim &ip,const Prim &op,const uint32_t mv)
        {
            if(ip==Prim::Points         )input_prim="points";else
            if(ip==Prim::Lines          )input_prim="lines";else
            if(ip==Prim::LinesAdj       )input_prim="lines_adjacency";else
            if(ip==Prim::Triangles      )input_prim="triangles";else
            if(ip==Prim::TrianglesAdj   )input_prim="triangles_adjacency";else
                return(false);

            if(op==Prim::Points         )output_prim="points";else
            if(op==Prim::LineStrip      )output_prim="line_strip";else
            if(op==Prim::TriangleStrip  )output_prim="triangle_strip";else
                return(false);

            if(mv==0)
                return(false);

            max_vertices=mv;
            return(true);
        }
    }//namespace graph
}//namespace hgl
