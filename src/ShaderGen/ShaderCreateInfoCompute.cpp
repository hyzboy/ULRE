#include<hgl/shadergen/ShaderCreateInfoCompute.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<string>
#include"GLSLCompiler.h"
#include"common/MFCommon.h"

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

        // 添加到shader的开头
        if(final_shader.empty())
            final_shader = layout_str;
        else
            final_shader = layout_str + final_shader;
    }

    bool ShaderCreateInfoCompute::ProcLayout()
    {
        // Compute shader的layout由SetWorkGroupSize设置
        // 这里确保如果没有设置，使用默认值
        if(final_shader.find("local_size_x")==std::string::npos)
        {
            // 默认工作组大小为 (1, 1, 1)
            SetWorkGroupSize(1, 1, 1);
        }

        return true;
    }
}//namespace hgl::graph
