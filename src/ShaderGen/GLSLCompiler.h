#ifndef HGL_GLSL_COMPILER_INCLUDE
#define HGL_GLSL_COMPILER_INCLUDE

#include<hgl/type/DataType.h>
namespace hgl
{
    namespace graph
    {
        struct SPVData
        {
            bool result;
            char *log;
            char *debug_log;

            uint32 *spv_data;
            uint32 spv_length;
        };

        bool        InitShaderCompiler();
        void        CloseShaderCompiler();
    
        SPVData *   CompileShader   (const uint32 type,const char *source);
        void        FreeSPVData     (SPVData *spv_data);
    }//namespace graph
}//namespace hgl
#endif//HGL_GLSL_COMPILER_INCLUDE
