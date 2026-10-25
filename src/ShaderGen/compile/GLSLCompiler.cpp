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
#include<hgl/mtl/ShaderCompilerProfileAPI.h>
#include "ShaderReflectionData.h"
#include "SPVParseFlat.h"

#include <cstring>

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
        static OSString g_include_root;
        static AnsiString g_include_root_utf8;
        static const char *g_include_root_ptr = nullptr;

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

            // These tail callbacks already exist in plugin ABI v2; the host
            // previously declared only the interface prefix.
            uint8_t *    (*CreateShaderFlat)(
                uint32_t shader_stage,
                const char *entrypoint,
                const SPVData *spv_data,
                const SPVParseData *parsed,
                uint32_t *out_size);
            void         (*FreeShaderFlat)(uint8_t *flat_data);
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

                // 描述符集终态 2 集（Scene/Bindless 全局）——Vulkan 规范
                // maxBoundDescriptorSets 下限 4 恒满足，无需门槛检查（DESCRIPTOR_SET_TYPE_COUNT=2）。
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

        }//namespace mtl

        static ExternalModule *gsi_module=nullptr;

        typedef GLSLCompilerInterface *(*GetInterfaceFUNC)();

        // 与插件侧 glsl2spv.cpp 的 plug_in_interface 首字段同步
        constexpr uint32_t kGLSLCompilerInterfaceABIVersion = 2;

        bool InitShaderCompiler()
        {
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
                        GLogError(u8"[GLSLCompiler] interface ABI mismatch: plugin=%u expected=%u; "
                                  u8"rebuild the plugin for the current include/resource contract",
                                  gsi->abi_version, kGLSLCompilerInterfaceABIVersion);
                        gsi=nullptr;
                        CloseShaderCompiler();
                        return false;
                    }

                    if(gsi->Init())
                    {
                        if(g_pd_profile_valid)
                            ApplyPhysicalDeviceProfileToCompilerLimits(g_pd_profile);

                        if (mtl::GetShaderIncludeRoot().IsEmpty())
                        {
                            CloseShaderCompiler();
                            return false;
                        }

                        // 生成器就绪 ⇒ 立刻接管运行时构建入口
                        // （接缝见 inc/hgl/mtl/ShaderProgramBuildBackend.h）
                        mtl::InstallShaderGenProgramBuildBackend();
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

            gsi=nullptr;
            delete gsi_module;
            gsi_module=nullptr;
            return(false);
        }

        void CloseShaderCompiler()
        {
            // 先摘接缝，避免关闭窗口期还有 TU 拿旧指针乱转
            mtl::UninstallShaderGenProgramBuildBackend();

            g_include_root.Clear();
            g_include_root_utf8.Clear();
            g_include_root_ptr=nullptr;
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
        bool SetShaderIncludeRoot(const char *path)
        {
            OSString root = filesystem::NormalizeFilename(ToOSString(path ? path : ""));
            if (!root.IsEmpty()
             && root[0] != HGL_DIRECTORY_SEPARATOR
             && (root.Length() < 2 || root[1] != ':'))
            {
                OSString current;
                if (!filesystem::GetCurrentPath(current))
                {
                    GLogError(u8"[GLSLCompiler] Cannot resolve include root");
                    return false;
                }
                root = filesystem::NormalizeFilename(
                    (filesystem::Path(current) / root).ToOSString());
            }
            if (root.IsEmpty() || !filesystem::IsDirectory(root))
            {
                GLogError(u8"[GLSLCompiler] Include root must be an existing directory");
                return false;
            }

            const U8String utf8 = ToU8String(root);
            g_include_root_utf8 = AnsiString(
                reinterpret_cast<const char *>(utf8.c_str()), utf8.Length());
            g_include_root = root;
            g_include_root_ptr = g_include_root_utf8.c_str();
            compile_info.includes = &g_include_root_ptr;
            compile_info.includes_count = 1;
            return true;
        }

        OSString GetShaderIncludeRoot()
        {
            if (g_include_root.IsEmpty()
             && !SetShaderIncludeRoot(GetShaderLibraryPath().c_str()))
                return {};
            return g_include_root;
        }
        }//namespace mtl

        void FreeSPVData(SPVData *spv_data)
        {
            if(gsi)
                gsi->Free(spv_data);
        }

        bool ReflectSPVData(
            SPVData *spv_data,
            const uint32 shader_stage,
            const char *entrypoint,
            ValueArray<uint8> &flat_storage,
            ::ShaderFlatView &out_view)
        {
            flat_storage.Clear();
            out_view = {};

            if(!gsi
             || !spv_data
             || !spv_data->result
             || !spv_data->spv_data
             || spv_data->spv_length == 0
             || (spv_data->spv_length % sizeof(uint32)) != 0
             || !gsi->ParseSPV
             || !gsi->FreeParseSPVData
             || !gsi->CreateShaderFlat
             || !gsi->FreeShaderFlat)
            {
                GLogError(u8"[GLSLCompiler] SPIR-V reflection API is unavailable");
                return false;
            }

            SPVParseData *parsed = gsi->ParseSPV(spv_data);
            if(!parsed)
            {
                GLogError(u8"[GLSLCompiler] SPIR-V reflection failed");
                return false;
            }

            uint32_t flat_size = 0;
            uint8_t *flat_data = gsi->CreateShaderFlat(
                shader_stage, entrypoint, spv_data, parsed, &flat_size);
            if(!flat_data || flat_size == 0
             || flat_size > static_cast<uint32_t>(0x7fffffff))
            {
                if(flat_data)
                    gsi->FreeShaderFlat(flat_data);
                gsi->FreeParseSPVData(parsed);
                GLogError(u8"[GLSLCompiler] Failed to serialize SPIR-V reflection");
                return false;
            }

            const bool copied = flat_storage.Resize(static_cast<int>(flat_size));
            if(copied)
                std::memcpy(flat_storage.GetData(), flat_data, flat_size);

            gsi->FreeShaderFlat(flat_data);
            gsi->FreeParseSPVData(parsed);

            if(!copied)
            {
                flat_storage.Clear();
                GLogError(u8"[GLSLCompiler] Failed to allocate SPIR-V reflection storage");
                return false;
            }

            if(!ShaderFlatLoad(flat_storage.GetData(), flat_size, &out_view))
            {
                flat_storage.Clear();
                out_view = {};
                GLogError(u8"[GLSLCompiler] Serialized SPIR-V reflection is invalid");
                return false;
            }

            return true;
        }

        bool ReflectSPVData(
            const uint32 *spv_data,
            const size_t spv_size,
            const uint32 shader_stage,
            const char *entrypoint,
            ValueArray<uint8> &flat_storage,
            ::ShaderFlatView &out_view)
        {
            if(!spv_data
             || spv_size == 0
             || spv_size > static_cast<size_t>(uint32(-1))
             || (spv_size % sizeof(uint32)) != 0)
            {
                flat_storage.Clear();
                out_view = {};
                GLogError(u8"[GLSLCompiler] Invalid raw SPIR-V buffer for reflection");
                return false;
            }

            SPVData borrowed_spv{};
            borrowed_spv.result = true;
            borrowed_spv.spv_data = const_cast<uint32 *>(spv_data);
            borrowed_spv.spv_length = static_cast<uint32>(spv_size);
            return ReflectSPVData(
                &borrowed_spv,
                shader_stage,
                entrypoint,
                flat_storage,
                out_view);
        }

        bool GetSPVPushConstantEnd(
            const uint32 *spv_data,
            const size_t spv_size,
            const uint32 shader_stage,
            uint32 &out_end)
        {
            out_end = 0;

            ValueArray<uint8> flat_storage;
            ShaderFlatView view{};
            if(!ReflectSPVData(
                    spv_data,
                    spv_size,
                    shader_stage,
                    "main",
                    flat_storage,
                    view))
                return false;

            mtl::ShaderReflectionData reflection;
            if(!reflection.Extract(view)
             || !reflection.GetPushConstantEnd(out_end))
            {
                out_end = 0;
                GLogError(u8"[GLSLCompiler] Failed to extract reflected push-constant extent");
                return false;
            }

            return true;
        }

        SPVData *CompileShader(const uint32_t type,const char *source,AnsiString *out_diagnostic)
        {
            if(out_diagnostic)
                out_diagnostic->Clear();

            if(!gsi)
            {
                std::fprintf(stderr,
                    "[GLSLCompiler] CompileShader failed: compiler not initialized (GLSLCompiler plugin unavailable?)\n");
                if(out_diagnostic)
                    *out_diagnostic = "compiler not initialized (GLSLCompiler plugin unavailable?)";
                return(nullptr);
            }

            ByteOrderMask bom=CheckBOM(source);

            if(bom==ByteOrderMask::UTF8)
                source+=3;
            else
            if(bom!=ByteOrderMask::NONE)
            {
                std::fprintf(stderr,"[GLSLCompiler] CompileShader failed: unsupported BOM\n");
                if(out_diagnostic)
                    *out_diagnostic = "unsupported BOM";
                return(nullptr);
            }

            SPVData *spv=gsi->Compile(type,source,&compile_info);

            if(!spv)
            {
                std::fprintf(stderr,"[GLSLCompiler] CompileShader failed: gsi->Compile returned null\n");
                if(out_diagnostic)
                    *out_diagnostic = "GLSLCompiler::Compile returned null";
                return(nullptr);
            }

            const bool result=spv->result;

            if(!result)
            {
                GLogError(u8"[GLSLCompiler] Compile failed: %s", spv->log ? spv->log : "");

                if(out_diagnostic)
                    *out_diagnostic = spv->log ? spv->log : "compile failed without a log";

                FreeSPVData(spv);
                return(nullptr);
            }

            return spv;
        }

        SPVData *CompileShader(const uint32_t type,const char *source)
        {
            return CompileShader(type,source,nullptr);
        }
    }//namespace graph
}//namespace hgl
