#include"GLSLCompiler.h"
#include"TBuiltInResourceCompat.h"
#include<hgl/platform/ExternalModule.h>
#include<hgl/type/StringList.h>
#include<hgl/filesystem/FileSystem.h>
#include<vulkan/vulkan.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/log/Logger.h>
#include<hgl/shadergen/contract/ShaderGenPhysicalDeviceProfileAdapter.h>
#include<hgl/shadergen/contract/ShaderGenPhysicalDeviceProfileJson.h>

namespace hgl
{
    namespace graph
    {
        // ?????EShTargetLanguageVersion
        constexpr const uint32_t SPV_VERSION_1_0 = (1 << 16);                     // SPIR-V 1.0
        constexpr const uint32_t SPV_VERSION_1_1 = (1 << 16) | (1 << 8);          // SPIR-V 1.1
        constexpr const uint32_t SPV_VERSION_1_2 = (1 << 16) | (2 << 8);          // SPIR-V 1.2
        constexpr const uint32_t SPV_VERSION_1_3 = (1 << 16) | (3 << 8);          // SPIR-V 1.3
        constexpr const uint32_t SPV_VERSION_1_4 = (1 << 16) | (4 << 8);          // SPIR-V 1.4
        constexpr const uint32_t SPV_VERSION_1_5 = (1 << 16) | (5 << 8);          // SPIR-V 1.5
        constexpr const uint32_t SPV_VERSION_1_6 = (1 << 16) | (6 << 8);          // SPIR-V 1.6

        enum class ShaderLanguageType
        {
            GLSL=0,
            HLSL,

            MAX=0xff
        };//enum class ShaderType

        struct CompileInfo
        {
            ShaderLanguageType  shader_type     = ShaderLanguageType::GLSL;
            const char *        entrypoint      = nullptr;                      //it only used in HLSL

            uint32_t            includes_count  = 0;
            const char **       includes        = nullptr;

            const char *        preamble        = nullptr;

                  uint32_t      vulkan_version  = VK_API_VERSION_1_0;
                  uint32_t      spv_version     = SPV_VERSION_1_0;
        };

        static mtl::contract::PhysicalDeviceProfileLite g_pd_profile{};
        static bool g_pd_profile_valid = false;

        static CompileInfo compile_info;

        struct SPVParseData;

        struct GLSLCompilerInterface
        {
            bool        (*Init)();
            void        (*Close)();

            bool        (*GetLimit)(void *,const int);
            bool        (*SetLimit)(void *,const int);

            uint32_t    (*GetType)(const char *ext_name);
            SPVData *   (*Compile)(const uint32_t stage,const char *shader_source, const CompileInfo *ci);
            SPVData *   (*CompileFromPath)(const uint32_t stage,const char *shader_filename, const CompileInfo *ci);

            void        (*Free)(SPVData *);

            SPVParseData *(*ParseSPV)(SPVData *spv_data);
            void        (*FreeParseSPVData)(SPVParseData *);
        };

        static GLSLCompilerInterface *gsi=nullptr;

        static int ClampU64ToInt(const uint64_t value)
        {
            return value > 0x7fffffffull ? 0x7fffffff : static_cast<int>(value);
        }

        static void ApplyPhysicalDeviceProfileToCompilerLimits(const mtl::contract::PhysicalDeviceProfileLite &profile)
        {
            if(!gsi || !gsi->GetLimit || !gsi->SetLimit)
                return;

            TBuiltInResourceCompat bir{};
            if(!gsi->GetLimit(&bir, sizeof(TBuiltInResourceCompat)))
                return;

            if(profile.limits.max_vertex_input_attributes>0)
            {
                const int attribs=static_cast<int>(profile.limits.max_vertex_input_attributes);

                bir.maxVertexAttribs=attribs;
                bir.maxVertexOutputVectors=attribs;
                bir.maxVaryingVectors=(attribs>=8)?8:attribs;

                const int components=attribs*4;
                bir.maxVertexOutputComponents=components;
                bir.maxFragmentInputComponents=components;
            }

            if(profile.limits.max_uniform_buffer_range>0)
            {
                const int uniform_components=ClampU64ToInt(profile.limits.max_uniform_buffer_range/16ull);
                bir.maxVertexUniformComponents=uniform_components;
                bir.maxFragmentUniformComponents=uniform_components;
                bir.maxComputeUniformComponents=uniform_components;
                bir.maxGeometryUniformComponents=uniform_components;
                bir.maxTessControlUniformComponents=uniform_components;
                bir.maxTessEvaluationUniformComponents=uniform_components;
            }

            if(profile.limits.max_storage_buffer_range>0)
            {
                bir.maxAtomicCounterBufferSize=ClampU64ToInt(profile.limits.max_storage_buffer_range);
            }

            if(profile.limits.max_bound_descriptor_sets>0)
            {
                const int sets=static_cast<int>(profile.limits.max_bound_descriptor_sets);
                bir.maxCombinedTextureImageUnits=sets*8;
                bir.maxCombinedImageUnitsAndFragmentOutputs=sets*4;
                bir.maxCombinedShaderOutputResources=sets*4;
            }

            bir.maxGeometryOutputVertices = profile.features.geometry_shader ? bir.maxGeometryOutputVertices : 0;
            bir.maxTessPatchComponents = profile.features.tessellation_shader ? bir.maxTessPatchComponents : 0;
            bir.maxTessGenLevel = profile.features.tessellation_shader ? bir.maxTessGenLevel : 0;

            if(profile.features.descriptor_indexing)
            {
                bir.limits.generalUniformIndexing = true;
                bir.limits.generalSamplerIndexing = true;
                bir.limits.generalVariableIndexing = true;
            }

            gsi->SetLimit(&bir, sizeof(TBuiltInResourceCompat));
        }

        static void SelectCompileTargets(const uint32_t api_version,const bool has_spirv_1_4_extension)
        {
            compile_info.vulkan_version = VK_API_VERSION_1_0;
            compile_info.spv_version = SPV_VERSION_1_0;

            if(api_version >= VK_API_VERSION_1_3)
            {
                compile_info.vulkan_version = VK_API_VERSION_1_3;
                compile_info.spv_version = SPV_VERSION_1_6;
            }
            else
            if(api_version >= VK_API_VERSION_1_2)
            {
                compile_info.vulkan_version = VK_API_VERSION_1_2;
                compile_info.spv_version = SPV_VERSION_1_5;
            }
            else
            if(api_version >= VK_API_VERSION_1_1)
            {
                compile_info.vulkan_version = VK_API_VERSION_1_1;
                compile_info.spv_version = has_spirv_1_4_extension ? SPV_VERSION_1_4 : SPV_VERSION_1_3;
            }
        }

        static void ApplyShaderCompilerPhysicalDeviceProfile(const mtl::contract::PhysicalDeviceProfileLite &profile,const bool has_spirv_1_4_extension)
        {
            g_pd_profile = profile;
            g_pd_profile_valid = true;

            SelectCompileTargets(profile.api_version,has_spirv_1_4_extension);

            GLogInfo("[GLSLCompiler] target vulkan=%u.%u spv=%u.%u",
                     VK_VERSION_MAJOR(compile_info.vulkan_version),
                     VK_VERSION_MINOR(compile_info.vulkan_version),
                     (compile_info.spv_version >> 16) & 0xff,
                     (compile_info.spv_version >> 8) & 0xff);

            ApplyPhysicalDeviceProfileToCompilerLimits(g_pd_profile);
        }

        void SetShaderCompilerPhysicalDeviceProfileFromRuntimeDevice(const VulkanPhyDevice *pd)
        {
            compile_info.vulkan_version = VK_API_VERSION_1_0;
            compile_info.spv_version = SPV_VERSION_1_0;

            g_pd_profile_valid = false;
            g_pd_profile = mtl::contract::PhysicalDeviceProfileLite{};

            if(!pd)
                return;

            const auto profile=mtl::contract::BuildPhysicalDeviceProfileFromVulkanPhyDevice(*pd);
            ApplyShaderCompilerPhysicalDeviceProfile(profile,pd->CheckExtensionSupport(VK_KHR_SPIRV_1_4_EXTENSION_NAME));
        }

        void SetShaderCompilerPhysicalDeviceProfile(const mtl::contract::PhysicalDeviceProfileLite &profile)
        {
            ApplyShaderCompilerPhysicalDeviceProfile(profile,false);
        }

        bool SetShaderCompilerPhysicalDeviceProfileFromJson(const char *json_text)
        {
            if(!json_text)
                return false;

            mtl::contract::PhysicalDeviceProfileLite profile;
            if(!mtl::contract::BuildPhysicalDeviceProfileFromCollectorJson(json_text,profile))
                return false;

            SetShaderCompilerPhysicalDeviceProfile(profile);
            return true;
        }

        void GetShaderCompilerTargetVersions(uint32 &vulkan_version,uint32 &spv_version)
        {
            vulkan_version=compile_info.vulkan_version;
            spv_version=compile_info.spv_version;
        }

        uint32 GetShaderCompilerLegacyVersionApiCallCount()
        {
            return 0;
        }

        static ExternalModule *gsi_module=nullptr;

        typedef GLSLCompilerInterface *(*GetInterfaceFUNC)();

        bool InitShaderCompiler()
        {
            compile_info.includes=nullptr;
            compile_info.includes_count=0;

            if(gsi)return(true);

            OSString cur_path;
            OSString glsl_compiler_fullname;

            if(!filesystem::GetCurrentPath(cur_path))
            {
                std::fprintf(stderr,"[GLSLCompiler] Init failed: cannot get current path\n");
                return(false);
            }
            glsl_compiler_fullname=filesystem::JoinPathWithFilename(cur_path,OS_TEXT("GLSLCompiler") HGL_PLUGIN_EXTNAME);

            gsi_module=LoadExternalModule(glsl_compiler_fullname);

            if(!gsi_module)
            {
                std::fprintf(stderr,
                    "[GLSLCompiler] Init failed: cannot load GLSLCompiler plugin module\n");
                return(false);
            }

            GetInterfaceFUNC get_func;

            get_func=GetInterfaceFUNC(gsi_module->GetFunc("GetInterface"));

            if(get_func)
            {
                gsi=get_func();
                if(gsi)
                {
                    if(gsi->Init())
                    {
                        if(g_pd_profile_valid)
                            ApplyPhysicalDeviceProfileToCompilerLimits(g_pd_profile);

                        return(true);
                    }

                    std::fprintf(stderr,"[GLSLCompiler] Init failed: plugin interface init returned false\n");
                }
                else
                {
                    std::fprintf(stderr,"[GLSLCompiler] Init failed: GetInterface returned null\n");
                }
            }
            else
            {
                std::fprintf(stderr,"[GLSLCompiler] Init failed: cannot resolve GetInterface symbol\n");
            }

            delete gsi_module;
            gsi_module=nullptr;
            return(false);
        }

        void CloseShaderCompiler()
        {
            delete[] compile_info.includes;
            compile_info.includes=nullptr;

            if(gsi)
            {
                gsi->Close();
                gsi=nullptr;
            }

            if(gsi_module)
            {
                delete gsi_module;
                gsi_module=nullptr;
            }
        }

        const char PreambleString[]="";//#extension GL_GOOGLE_include_directive : require\n";

        void RebuildGLSLIncludePath()
        {
            compile_info.preamble=PreambleString;
        }

        void FreeSPVData(SPVData *spv_data)
        {
            if(gsi)
                gsi->Free(spv_data);
        }

        SPVData *CompileShader(const uint32_t type,const char *source)
        {
            if(!gsi)
            {
                std::fprintf(stderr,
                    "[GLSLCompiler] CompileShader failed: compiler not initialized (GLSLCompiler plugin unavailable?)\n");
                return(nullptr);
            }

            ByteOrderMask bom=CheckBOM(source);

            if(bom==ByteOrderMask::UTF8)
                source+=3;
            else
            if(bom!=ByteOrderMask::NONE)
            {
                std::fprintf(stderr,"[GLSLCompiler] CompileShader failed: unsupported BOM\n");
                return(nullptr);
            }

            SPVData *spv=gsi->Compile(type,source,&compile_info);

            if(!spv)
            {
                std::fprintf(stderr,"[GLSLCompiler] CompileShader failed: gsi->Compile returned null\n");
                return(nullptr);
            }

            const bool result=spv->result;

            if(!result)
            {
                std::string err="Compile shader failed, error info: ";
                err+=spv->log?spv->log:"";
                GLogError(err.c_str());

                FreeSPVData(spv);
                return(nullptr);
            }

            return spv;
        }
    }//namespace graph
}//namespace hgl
