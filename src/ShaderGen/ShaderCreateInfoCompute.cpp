#include<hgl/shadergen/ShaderCreateInfoCompute.h>
#include<string>

namespace hgl::graph
{
    void ShaderCreateInfoCompute::SetWorkGroupSize(uint32 x, uint32 y, uint32 z)
    {
        std::string layout_str;
        layout_str.reserve(96);
        layout_str += "layout(local_size_x = ";
        layout_str += std::to_string(x);
        layout_str += ", local_size_y = ";
        layout_str += std::to_string(y);
        layout_str += ", local_size_z = ";
        layout_str += std::to_string(z);
        layout_str += ") in;\n";

        if(final_shader.empty())
            final_shader = layout_str;
        else
            final_shader = layout_str + final_shader;
    }
}//namespace hgl::graph
