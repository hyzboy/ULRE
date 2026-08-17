#include"GLSLCompiler.h"
#include"TBuiltInResourceCompat.h"
#include<hgl/platform/ExternalModule.h>
#include<hgl/type/StringList.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/log/Logger.h>
#include<hgl/shadergen/contract/ShaderGenProfileTargetVersion.h>
#include<hgl/shadergen/contract/ShaderGenPhysicalDeviceProfileJson.h>
#include<hgl/shadergen/ShaderLibraryPath.h>
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

                  uint32_t      vulkan_version  = shadergen::contract::MakeVkVersion(1, 0);
                  uint32_t      spv_version     = shadergen::contract::SPV_VERSION_1_0;
        };

        static shadergen::contract::PhysicalDeviceProfileLite g_pd_profile{};
        static bool g_pd_profile_valid = false;

        static CompileInfo compile_info;

        struct SPVParseData;

        struct GLSLCompilerInterface
        {
            // 接口 ABI 版本：TBuiltInResourceCompat 按 sizeof 跨插件边界传递
            // （本文件 :77/:128 ↔ 插件 glsl2spv.cpp GetLimit/SetLimit 的 size 校验），
            // glslang 升版改 TBuiltInResource 布局时两端必须同步 bump，
            // 否则 limits 静默 no-op。插件侧填常量，加载后校验。
            uint32_t    abi_version;

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

        static void ApplyPhysicalDeviceProfileToCompilerLimits(const shadergen::contract::PhysicalDeviceProfileLite &profile)
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

        static void ApplyShaderCompilerPhysicalDeviceProfile(const shadergen::contract::PhysicalDeviceProfileLite &profile)
        {
            g_pd_profile = profile;
            g_pd_profile_valid = true;

            shadergen::contract::ResolveShaderTargetVersions(profile,
                                                       compile_info.vulkan_version,
                                                       compile_info.spv_version);

            GLogInfo("[GLSLCompiler] target vulkan=%u.%u spv=%u.%u",
                     shadergen::contract::VkVersionMajor(compile_info.vulkan_version),
                     shadergen::contract::VkVersionMinor(compile_info.vulkan_version),
                     (compile_info.spv_version >> 16) & 0xff,
                     (compile_info.spv_version >> 8) & 0xff);

            ApplyPhysicalDeviceProfileToCompilerLimits(g_pd_profile);
        }

        namespace shadergen
        {
        void SetShaderCompilerPhysicalDeviceProfile(const shadergen::contract::PhysicalDeviceProfileLite &profile)
        {
            ApplyShaderCompilerPhysicalDeviceProfile(profile);
        }

        bool SetShaderCompilerPhysicalDeviceProfileFromJson(const char *json_text)
        {
            if(!json_text)
                return false;

            shadergen::contract::PhysicalDeviceProfileLite profile;
            if(!shadergen::contract::BuildPhysicalDeviceProfileFromCollectorJson(json_text,profile))
                return false;

            SetShaderCompilerPhysicalDeviceProfile(profile);
            return true;
        }

        void GetShaderCompilerTargetVersions(uint32 &vulkan_version,uint32 &spv_version)
        {
            vulkan_version=compile_info.vulkan_version;
            spv_version=compile_info.spv_version;
        }
        }//namespace shadergen

        static ExternalModule *gsi_module=nullptr;

        typedef GLSLCompilerInterface *(*GetInterfaceFUNC)();

        // 与插件侧 glsl2spv.cpp 的 plug_in_interface 首字段同步
        constexpr uint32_t kGLSLCompilerInterfaceABIVersion = 1;

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
                // fallback: 可执行文件目录（构建输出目录）——cwd 拼接仅覆盖
                // 从仓库根运行的情况；exe 目录才是 dll 的常规位置
#ifdef _WIN32
                wchar_t module_path[MAX_PATH + 1] = {};
                const DWORD module_path_len =
                    GetModuleFileNameW(nullptr, module_path, MAX_PATH);
                if (module_path_len > 0)
                {
                    const OSString exe_path(module_path);
                    const int slash =
                        exe_path.FindRightChar(HGL_DIRECTORY_SEPARATOR);
                    if (slash >= 0)
                    {
                        glsl_compiler_fullname =
                            filesystem::JoinPathWithFilename(
                                exe_path.SubString(0, slash),
                                OS_TEXT("GLSLCompiler") HGL_PLUGIN_EXTNAME);
                        gsi_module = LoadExternalModule(glsl_compiler_fullname);
                    }
                }
#endif
            }

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
                    if (gsi->abi_version != kGLSLCompilerInterfaceABIVersion)
                    {
                        GLogError(u8"[GLSLCompiler] interface ABI mismatch: plugin=%u expected=%u — "
                                  u8"TBuiltInResourceCompat layout may have drifted (glslang upgrade?)",
                                  gsi->abi_version, kGLSLCompilerInterfaceABIVersion);
                        return false;
                    }

                    if(gsi->Init())
                    {
                        if(g_pd_profile_valid)
                            ApplyPhysicalDeviceProfileToCompilerLimits(g_pd_profile);

                        const std::string shader_library_path =
                            shadergen::GetShaderLibraryPath();
                        shadergen::AddShaderIncludePath(shader_library_path.c_str());

                        // bindless_textures 等模块被拼接进 fs_final 后，其
                        // `#include "descriptor_macros.glsl"`（原相对 common/）
                        // 在无目录上下文中按 -I 搜索——补 common/ 搜索路径，
                        // 与带 common/ 前缀的 include（-I 根下）双路径兼容
                        shadergen::AddShaderIncludePath((shader_library_path + "/common").c_str());

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

        namespace shadergen
        {
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
        }//namespace shadergen

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
                std::fprintf(stderr,"[GLSLCompiler] %s\n", err.c_str());
                GLogError(err.c_str());

                FreeSPVData(spv);
                return(nullptr);
            }

            return spv;
        }
    }//namespace graph
}//namespace hgl
