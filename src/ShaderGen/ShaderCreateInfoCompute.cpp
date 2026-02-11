#include<hgl/shadergen/ShaderCreateInfoCompute.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include"GLSLCompiler.h"
#include"common/MFCommon.h"
#include"ShaderLibrary.h"

namespace hgl::graph
{
    void ShaderCreateInfoCompute::SetWorkGroupSize(uint32 x, uint32 y, uint32 z)
    {
        AnsiString layout_str = "layout(local_size_x = ";
        layout_str += AnsiString::numberOf(x);
        layout_str += ", local_size_y = ";
        layout_str += AnsiString::numberOf(y);
        layout_str += ", local_size_z = ";
        layout_str += AnsiString::numberOf(z);
        layout_str += ") in;\n";
        
        // Add to the beginning of shader
        if(final_shader.IsEmpty())
            final_shader = layout_str;
        else
            final_shader = layout_str + final_shader;
    }

    bool ShaderCreateInfoCompute::ProcLayout()
    {
        // Compute shader layout is set by SetWorkGroupSize
        // Ensure default values are used if not set
        if(final_shader.Find("local_size_x") == -1)
        {
            // Default work group size is (1, 1, 1)
            SetWorkGroupSize(1, 1, 1);
        }
        
        return true;
    }
}//namespace hgl::graph
