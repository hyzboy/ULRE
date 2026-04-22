#include"GLSLCompiler.h"
#include"TBuiltInResourceCompat.h"
#include"SPVParseData.h"
#include <hgl/shadergen/ShaderLibraryPath.h>
#include<hgl/platform/ExternalModule.h>
#include<hgl/type/StringList.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/log/Logger.h>
#include<hgl/shadergen/device/DeviceProfileTargetVersion.h>
#include<hgl/shadergen/device/DeviceProfileJson.h>
#include<vector>
#include<string>

namespace hgl
{
    namespace graph
    {
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

                  uint32_t      vulkan_version  = mtl::contract::MakeVkVersion(1, 0);
                  uint32_t      spv_version     = mtl::contract::SPV_VERSION_1_0;
        };

        static mtl::contract::PhysicalDeviceProfileLite g_pd_profile{};
        static bool g_pd_profile_valid = false;

        static CompileInfo compile_info;

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

        static const char *GetDescriptorTypeName(const uint32 index)
        {
            switch(index)
            {
                case VK_DESCRIPTOR_TYPE_SAMPLER: return "Sampler";
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return "CombinedImageSampler";
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return "SampledImage";
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: return "StorageImage";
                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return "UniformTexelBuffer";
                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return "StorageTexelBuffer";
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return "UniformBuffer";
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: return "StorageBuffer";
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return "UniformBufferDynamic";
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return "StorageBufferDynamic";
                case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return "InputAttachment";
                default: return "Unknown";
            }
        }

        static void DumpParseSPVData(const uint32 type,const SPVParseData *parse_data)
        {
            if(!parse_data)
                return;

            std::fprintf(stderr,
                         "[GLSLCompiler] ParseSPV(stage=0x%08X): in=%u out=%u push=%u subpass=%u\n",
                         type,
                         parse_data->stage_io.input.count,
                         parse_data->stage_io.output.count,
                         parse_data->push_constant.count,
                         parse_data->subpass_input.count);

            for(uint32 i=0;i<VK_DESCRIPTOR_TYPE_COUNT;i++)
            {
                const auto &bucket=parse_data->resource[i];
                if(bucket.count==0)
                    continue;

                std::fprintf(stderr,
                             "[GLSLCompiler]   %s count=%u\n",
                             GetDescriptorTypeName(i),
                             bucket.count);

                for(uint32 j=0;j<bucket.count;j++)
                {
                    const Descriptor &d=bucket.items[j];
                    std::fprintf(stderr,
                                 "[GLSLCompiler]     name=%s set=%u binding=%u\n",
                                 d.name,
                                 d.set,
                                 d.binding);
                }
            }
        }

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

        static void ApplyShaderCompilerPhysicalDeviceProfile(const mtl::contract::PhysicalDeviceProfileLite &profile)
        {
            g_pd_profile = profile;
            g_pd_profile_valid = true;

            mtl::contract::ResolveShaderTargetVersions(profile,
                                                       compile_info.vulkan_version,
                                                       compile_info.spv_version);

            GLogInfo("[GLSLCompiler] target vulkan=%u.%u spv=%u.%u",
                     mtl::contract::VkVersionMajor(compile_info.vulkan_version),
                     mtl::contract::VkVersionMinor(compile_info.vulkan_version),
                     (compile_info.spv_version >> 16) & 0xff,
                     (compile_info.spv_version >> 8) & 0xff);

            ApplyPhysicalDeviceProfileToCompilerLimits(g_pd_profile);
        }

        void SetShaderCompilerPhysicalDeviceProfile(const mtl::contract::PhysicalDeviceProfileLite &profile)
        {
            ApplyShaderCompilerPhysicalDeviceProfile(profile);
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

                        AddShaderIncludePath(GetShaderLibraryPath().c_str());

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

        static std::vector<std::string> g_include_path_storage;   // 持有字符串
        static std::vector<const char*> g_include_path_ptrs;      // 持有指针

        void CloseShaderCompiler()
        {
            g_include_path_storage.clear();
            g_include_path_ptrs.clear();
            compile_info.includes=nullptr;
            compile_info.includes_count=0;

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

        const char PreambleString[]="";

        void AddShaderIncludePath(const char *path)
        {
            if(!path || path[0]=='\0') return;

            g_include_path_storage.emplace_back(path);

            // 重建指针数组
            g_include_path_ptrs.clear();
            for(auto &s : g_include_path_storage)
                g_include_path_ptrs.push_back(s.c_str());

            compile_info.includes       = g_include_path_ptrs.data();
            compile_info.includes_count  = static_cast<uint32_t>(g_include_path_ptrs.size());
        }

        void RebuildGLSLIncludePath()
        {
            compile_info.preamble=PreambleString;
        }

        void FreeSPVData(SPVData *spv_data)
        {
            if(gsi)
                gsi->Free(spv_data);
        }

        SPVParseData *ParseShaderSPV(SPVData *spv_data)
        {
            if(!gsi||!gsi->ParseSPV||!spv_data)
                return(nullptr);

            return gsi->ParseSPV(spv_data);
        }

        void FreeShaderSPVParseData(SPVParseData *parse_data)
        {
            if(gsi&&gsi->FreeParseSPVData)
                gsi->FreeParseSPVData(parse_data);
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
                std::fprintf(stderr,"[GLSLCompiler] %s\n", err.c_str());
                GLogError(err.c_str());

                FreeSPVData(spv);
                return(nullptr);
            }
            else
            {
                GLogInfo("[GLSLCompiler] Compile shader success, shader:\n %s", source);
            }

            if(gsi->ParseSPV && gsi->FreeParseSPVData)
            {
                SPVParseData *parse_data=ParseShaderSPV(spv);
                if(parse_data)
                {
                    DumpParseSPVData(type,parse_data);
                    FreeShaderSPVParseData(parse_data);
                }
                else
                {
                    std::fprintf(stderr,"[GLSLCompiler] ParseSPV returned null\n");
                }
            }

            return spv;
        }
    }//namespace graph
}//namespace hgl
