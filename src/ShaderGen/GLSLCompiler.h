#ifndef HGL_GLSL_COMPILER_INCLUDE
#define HGL_GLSL_COMPILER_INCLUDE

#include<hgl/type/DataType.h>
#include<hgl/graph/mtl/SPVParseData.h>

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
        // ParseSPVData calls the GLSLCompiler DLL's ParseSPV, then converts
        // the legacy DLL-internal layout to hgl::SPVParseData (defined in
        // inc/hgl/graph/mtl/SPVParseData.h).  All sub-arrays in the returned
        // struct are ULRE-allocated and must be freed by calling
        // FreeSPVParseData().
        //
        // When the GLSLCompiler DLL is updated to natively fill hgl::SPVParseData
        // (see doc/refactor/GLSLCompiler_Update.md), the bridge code in
        // GLSLCompiler.cpp can be removed and these functions will simply forward
        // to the new DLL API.
        // ------------------------------------------------------------------

        hgl::SPVParseData *  ParseSPVData    (const SPVData *spv_data);
        void                 FreeSPVParseData(hgl::SPVParseData *parse_data);

    }//namespace graph
}//namespace hgl
#endif//HGL_GLSL_COMPILER_INCLUDE
