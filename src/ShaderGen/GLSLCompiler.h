#ifndef HGL_GLSL_COMPILER_INCLUDE
#define HGL_GLSL_COMPILER_INCLUDE

#include<hgl/type/DataType.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
namespace hgl
{
    namespace graph
    {
        class VulkanPhyDevice;

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

        void        SetShaderCompilerVersion(const VulkanPhyDevice *pd);
        void        SetShaderCompilerPhysicalDeviceProfile(const mtl::contract::PhysicalDeviceProfileLite &profile);
        bool        SetShaderCompilerPhysicalDeviceProfileFromJson(const char *json_text);
        void        GetShaderCompilerTargetVersions(uint32 &vulkan_version,uint32 &spv_version);

        SPVData *   CompileShader   (const uint32 type,const char *source);
        void        FreeSPVData     (SPVData *spv_data);
    }//namespace graph
}//namespace hgl
#endif//HGL_GLSL_COMPILER_INCLUDE
