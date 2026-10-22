#ifndef HGL_SHADER_COMPILER_SPV_DATA_H
#define HGL_SHADER_COMPILER_SPV_DATA_H

#include<hgl/type/DataType.h>

namespace hgl
{
    namespace graph
    {
        /**
         * 编译产物句柄。
         *
         * 内存由 GLSL 编译器插件分配、由插件释放（`FreeSPVData`），所以这是一份
         * 跨模块边界的 ABI 布局。定义放在这个独立头里，是为了让「只装载预烘焙
         * SPV」的纯数据面 TU 也能读到字段，而不必 include 生成器的 GLSLCompiler.h。
         */
        struct SPVData
        {
            bool result;
            char *log;
            char *debug_log;

            uint32 *spv_data;
            uint32 spv_length;
        };
    }//namespace graph
}//namespace hgl

#endif//HGL_SHADER_COMPILER_SPV_DATA_H
