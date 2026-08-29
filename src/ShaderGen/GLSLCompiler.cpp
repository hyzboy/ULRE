#include"GLSLCompiler.h"
#include"TBuiltInResourceCompat.h"
#include<hgl/platform/ExternalModule.h>
#include<hgl/type/StringList.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/log/Logger.h>
#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include<hgl/mtl/contract/ShaderGenPhysicalDeviceProfileJson.h>
#include<hgl/mtl/ShaderLibraryPath.h>
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

            uint32_t            vulkan_version  = mtl::contract::TARGET_VULKAN_VERSION;
            uint32_t            spv_version     = mtl::contract::TARGET_SPV_VERSION;
        };

        static mtl::contract::PhysicalDeviceProfileLite g_pd_profile{};
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

                // Phase 5：描述符集布局为 5 个（Scene/PerObject/Material/Bindless/Vertex）。
                // Vulkan 规范 maxPerStageDescriptorSets 下限是 4，Vertex 集要求设备支持 >=5；
                // 引擎已硬性要求 mesh shader 扩展（实际设备均支持），此处显式声明门槛。
                static bool s_descriptor_set_limit_warned = false;
                if(!s_descriptor_set_limit_warned
                 && sets < int(DESCRIPTOR_SET_TYPE_COUNT))
                {
                    s_descriptor_set_limit_warned = true;
                    GLogError(u8"[GLSLCompiler] device maxBoundDescriptorSets=%d < required %d — "
                              u8"Vertex descriptor set (Set 4) is not supported on this device",
                              sets, int(DESCRIPTOR_SET_TYPE_COUNT));
                }
            }

            bir.maxGeometryOutputVertices = profile.features.geometry_shader ? bir.maxGeometryOutputVertices : 0;
            bir.maxTessPatchComponents = profile.features.tessellation_shader ? bir.maxTessPatchComponents : 0;
            bir.maxTessGenLevel = profile.features.tessellation_shader ? bir.maxTessGenLevel : 0;

            // VK_EXT_mesh_shader limits（从物理设备实际查询，替代插件硬编码默认值）
            if(profile.limits.max_mesh_work_group_size_x>0)
            {
                bir.maxMeshWorkGroupSizeX_EXT   = static_cast<int>(profile.limits.max_mesh_work_group_size_x);
                bir.maxMeshWorkGroupSizeY_EXT   = static_cast<int>(profile.limits.max_mesh_work_group_size_y);
                bir.maxMeshWorkGroupSizeZ_EXT   = static_cast<int>(profile.limits.max_mesh_work_group_size_z);
                bir.maxTaskWorkGroupSizeX_EXT   = static_cast<int>(profile.limits.max_task_work_group_size_x);
                bir.maxTaskWorkGroupSizeY_EXT   = static_cast<int>(profile.limits.max_task_work_group_size_y);
                bir.maxTaskWorkGroupSizeZ_EXT   = static_cast<int>(profile.limits.max_task_work_group_size_z);
                bir.maxMeshOutputVerticesEXT    = static_cast<int>(profile.limits.max_mesh_output_vertices);
                bir.maxMeshOutputPrimitivesEXT  = static_cast<int>(profile.limits.max_mesh_output_primitives);
                bir.maxMeshViewCountEXT         = static_cast<int>(profile.limits.max_mesh_view_count);
            }

            // Vulkan 1.4 核心：descriptor indexing 无条件可用（vulkan1.4.md 第 1 项）
            bir.limits.generalUniformIndexing = true;
            bir.limits.generalSamplerIndexing = true;
            bir.limits.generalVariableIndexing = true;

            gsi->SetLimit(&bir, sizeof(TBuiltInResourceCompat));
        }

        static void ApplyShaderCompilerPhysicalDeviceProfile(const mtl::contract::PhysicalDeviceProfileLite &profile)
        {
            g_pd_profile = profile;
            g_pd_profile_valid = true;

            compile_info.vulkan_version = mtl::contract::TARGET_VULKAN_VERSION;
            compile_info.spv_version = mtl::contract::TARGET_SPV_VERSION;

            GLogInfo("[GLSLCompiler] target vulkan=%u.%u spv=%u.%u",
                     mtl::contract::VkVersionMajor(compile_info.vulkan_version),
                     mtl::contract::VkVersionMinor(compile_info.vulkan_version),
                     (compile_info.spv_version >> 16) & 0xff,
                     (compile_info.spv_version >> 8) & 0xff);

            ApplyPhysicalDeviceProfileToCompilerLimits(g_pd_profile);
        }

        namespace mtl
        {
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
        }//namespace mtl

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
                            mtl::GetShaderLibraryPath();
                        mtl::AddShaderIncludePath(shader_library_path.c_str());

                        // bindless_textures 等模块被拼接进 fs_final 后，其
                        // `#include "descriptor_macros.glsl"`（原相对 common/）
                        // 在无目录上下文中按 -I 搜索——补 common/ 搜索路径，
                        // 与带 common/ 前缀的 include（-I 根下）双路径兼容
                        mtl::AddShaderIncludePath((shader_library_path + "/common").c_str());

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

        namespace mtl
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
        }//namespace mtl

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
