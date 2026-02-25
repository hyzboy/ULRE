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

        // ------------------------------------------------------------------
        // ParseSPVData / FreeSPVParseData
        //
        // These wrap gsi->ParseSPV() and return the DLL-allocated parse data.
        // The returned pointer type will become hgl::SPVParseData (from
        // SPVParseData.h) once the GLSLCompiler DLL is updated to the new
        // rich reflection format described in:
        //   doc/refactor/ShaderGen_Compiler_Loader_Separation.md
        //
        // Until then, callers access the raw opaque pointer only through the
        // SPVLayoutBuilder API (inc/hgl/graph/mtl/SPVLayoutBuilder.h).
        // ------------------------------------------------------------------

        // Forward-declare the opaque DLL parse data type.
        // Do NOT dereference this pointer in ULRE code — pass it to
        // SPVLayoutBuilder functions or free it via FreeSPVParseData().
        struct SPVParseDataOpaque;

        SPVParseDataOpaque *  ParseSPVData    (const SPVData *spv_data);
        void                  FreeSPVParseData(SPVParseDataOpaque *parse_data);
    }//namespace graph
}//namespace hgl
#endif//HGL_GLSL_COMPILER_INCLUDE
