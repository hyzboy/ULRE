#include"GLSLCompiler.h"
#include<hgl/platform/ExternalModule.h>
#include<hgl/filesystem/FileSystem.h>
#include<vulkan/vulkan.h>
#include<iostream>
#include<hgl/vk/VKPhysicalDevice.h>

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

        static CompileInfo compile_info;

        void SetShaderCompilerVersion(const VulkanPhyDevice *pd)
        {
            const auto &pdp=pd->GetProperties();

            compile_info.vulkan_version =pdp.apiVersion;

            if(pdp.apiVersion>=VK_API_VERSION_1_3)compile_info.spv_version=SPV_VERSION_1_6;else
            if(pdp.apiVersion>=VK_API_VERSION_1_2)compile_info.spv_version=SPV_VERSION_1_5;else
            if(pd->CheckExtensionSupport(VK_KHR_SPIRV_1_4_EXTENSION_NAME))
                                                  compile_info.spv_version=SPV_VERSION_1_4;else
            if(pdp.apiVersion>=VK_API_VERSION_1_1)compile_info.spv_version=SPV_VERSION_1_3;else
                                                  compile_info.spv_version=SPV_VERSION_1_0;
        }

        // -----------------------------------------------------------------------
        // GLSLCOMP_ABI — exact mirror of GLSLCompiler/glsl2spv.cpp private structs
        //
        // GLSLCompiler defines these structs internally without a shared header.
        // We reproduce them here so ConvertOldSPVParseData() can read DLL-
        // allocated memory and convert it to hgl::SPVParseData.
        //
        // Static-assert guards catch layout mismatches at compile time.
        // Migration guide: doc/refactor/GLSLCompiler_Update.md
        // -----------------------------------------------------------------------
        namespace GLSLCOMP_ABI {

        constexpr uint32_t NAME_MAX = 32;

        struct ShaderAttribute {
            char    name[NAME_MAX]; // 32 bytes
            uint8_t location;       // NOTE: uint8_t — max 255 (sufficient for Vulkan)
            uint8_t basetype;
            uint8_t vec_size;
        }; // 35 bytes, alignment=1, no padding
        static_assert(sizeof(ShaderAttribute) == 35,
            "GLSLCOMP_ABI::ShaderAttribute layout mismatch — DLL ABI broken");

        struct ShaderAttributeArray {
            uint32_t          count;  // 4 bytes
            // 4 bytes implicit padding on 64-bit (pointer alignment)
            ShaderAttribute * items;  // 8 bytes on 64-bit
        };

        struct Descriptor {
            char    name[NAME_MAX]; // 32 bytes
            uint8_t set;
            uint8_t binding;
        }; // 34 bytes, alignment=1
        static_assert(sizeof(Descriptor) == 34,
            "GLSLCOMP_ABI::Descriptor layout mismatch — DLL ABI broken");

        struct PushConstant {
            char    name[NAME_MAX]; // 32 bytes
            uint8_t offset;  // NOTE: truncated from uint32_t in DLL — see GLSLCompiler_Update.md
            uint8_t size;    // NOTE: truncated from uint32_t in DLL
        }; // 34 bytes, alignment=1
        static_assert(sizeof(PushConstant) == 34,
            "GLSLCOMP_ABI::PushConstant layout mismatch — DLL ABI broken");

        struct SubpassInput {
            char    name[NAME_MAX];         // 32 bytes
            uint8_t input_attachment_index;
            uint8_t binding;
        }; // 34 bytes, alignment=1
        static_assert(sizeof(SubpassInput) == 34,
            "GLSLCOMP_ABI::SubpassInput layout mismatch — DLL ABI broken");

        template<typename T>
        struct ShaderResourceData { uint32_t count; T *items; };

        constexpr int DESCRIPTOR_TYPE_COUNT = 11;
        using DescriptorResources = ShaderResourceData<Descriptor>[DESCRIPTOR_TYPE_COUNT];

        struct ShaderStageIO {
            ShaderAttributeArray input;
            ShaderAttributeArray output;
        };

        struct SPVParseData {
            ShaderStageIO                    stage_io;
            DescriptorResources              resource;
            ShaderResourceData<PushConstant> push_constant;
            ShaderResourceData<SubpassInput> subpass_input;
        };

        // VkDescriptorType slot indices filled by GLSLCompiler's ParseSPV.
        // Slots 4,5 (texel buffers) and 8,9 (dynamic) are never filled.
        // Slot 10 (INPUT_ATTACHMENT) is stored in subpass_input, not resource[].
        constexpr int IDX_SAMPLER        = 0;
        constexpr int IDX_COMBINED_IMAGE = 1;
        constexpr int IDX_SAMPLED_IMAGE  = 2;
        constexpr int IDX_STORAGE_IMAGE  = 3;
        constexpr int IDX_UNIFORM_BUFFER = 6;
        constexpr int IDX_STORAGE_BUFFER = 7;

        } // namespace GLSLCOMP_ABI

        // Raw opaque pointer type used when calling the old DLL API.
        // We never dereference it on this side — it goes straight to
        // FreeParseSPVData so the DLL can delete its own allocation.
        using RawParsePtr = struct GLSLCOMP_RawParse *;

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

            // Old DLL API: returns the internal SPVParseData as an opaque pointer.
            // Replaced by FillSPVParseData in updated DLL; see GLSLCompiler_Update.md.
            RawParsePtr (*ParseSPV)(SPVData *spv_data);
            void        (*FreeParseSPVData)(RawParsePtr);
        };

        static ExternalModule *gsi_module=nullptr;

        static GLSLCompilerInterface *gsi=nullptr;

        typedef GLSLCompilerInterface *(*GetInterfaceFUNC)();

        bool InitShaderCompiler()
        {
            compile_info.includes=nullptr;
            compile_info.includes_count=0;

            if(gsi)return(true);

            OSString cur_path;
            OSString glsl_compiler_fullname;

            if(!filesystem::GetCurrentPath(cur_path))
                return(false);
            glsl_compiler_fullname=filesystem::JoinPathWithFilename(cur_path,OS_TEXT("GLSLCompiler") HGL_PLUGIN_EXTNAME);

            gsi_module=LoadExternalModule(glsl_compiler_fullname);

            if(!gsi_module)return(false);

            GetInterfaceFUNC get_func;

            get_func=GetInterfaceFUNC(gsi_module->GetFunc("GetInterface"));

            if(get_func)
            {
                gsi=get_func();
                if(gsi)
                {
                    if(gsi->Init())
                        return(true);
                }
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
                return(nullptr);

            ByteOrderMask bom=CheckBOM(source);

            if(bom==ByteOrderMask::UTF8)
                source+=3;
            else
            if(bom!=ByteOrderMask::NONE)
                return(nullptr);

            SPVData *spv=gsi->Compile(type,source,&compile_info);

            if(!spv)return(nullptr);

            const bool result=spv->result;

            if(!result)
            {
                std::cerr << "Compile shader failed, error info: " << spv->log << "
";

                FreeSPVData(spv);
                return(nullptr);
            }

            return spv;
        }

        // -----------------------------------------------------------------------
        // ConvertOldSPVParseData
        //
        // Converts a DLL-allocated GLSLCOMP_ABI::SPVParseData into a freshly
        // ULRE-allocated hgl::SPVParseData.  Sub-arrays are owned by ULRE and
        // must be freed via FreeConvertedSPVParseData().
        //
        // Fields not available in the old DLL ABI are zeroed (array_count=1,
        // buffer_size=0, member_count=0, component=0).  They will be populated
        // once GLSLCompiler is updated per doc/refactor/GLSLCompiler_Update.md.
        // -----------------------------------------------------------------------
        static hgl::SPVParseData *ConvertOldSPVParseData(
            const GLSLCOMP_ABI::SPVParseData *old)
        {
            if (!old) return nullptr;

            hgl::SPVParseData *pd = new hgl::SPVParseData;

            // --- stage inputs ---
            const uint32_t ni = old->stage_io.input.count;
            if (ni > 0) {
                pd->stage_inputs.count = ni;
                pd->stage_inputs.items = new hgl::SPVStageAttribute[ni];
                for (uint32_t i = 0; i < ni; ++i) {
                    const auto &s = old->stage_io.input.items[i];
                    auto       &d = pd->stage_inputs.items[i];
                    strncpy(d.name, s.name, hgl::SPV_NAME_MAX - 1);
                    d.name[hgl::SPV_NAME_MAX - 1] = '\0';
                    d.location  = s.location;
                    d.component = 0; // not in old ABI
                    d.basetype  = static_cast<hgl::SPVBaseType>(s.basetype);
                    d.vec_size  = s.vec_size;
                }
            }

            // --- stage outputs ---
            const uint32_t no = old->stage_io.output.count;
            if (no > 0) {
                pd->stage_outputs.count = no;
                pd->stage_outputs.items = new hgl::SPVStageAttribute[no];
                for (uint32_t i = 0; i < no; ++i) {
                    const auto &s = old->stage_io.output.items[i];
                    auto       &d = pd->stage_outputs.items[i];
                    strncpy(d.name, s.name, hgl::SPV_NAME_MAX - 1);
                    d.name[hgl::SPV_NAME_MAX - 1] = '\0';
                    d.location  = s.location;
                    d.component = 0;
                    d.basetype  = static_cast<hgl::SPVBaseType>(s.basetype);
                    d.vec_size  = s.vec_size;
                }
            }

            // --- descriptors ---
            // Each resource kind lives in a specific slot of the old resource[] array.
            struct { int idx; hgl::SPVDescriptorKind kind; } kmap[] = {
                { GLSLCOMP_ABI::IDX_UNIFORM_BUFFER, hgl::SPVDescriptorKind::UniformBuffer        },
                { GLSLCOMP_ABI::IDX_STORAGE_BUFFER, hgl::SPVDescriptorKind::StorageBuffer        },
                { GLSLCOMP_ABI::IDX_COMBINED_IMAGE, hgl::SPVDescriptorKind::CombinedImageSampler },
                { GLSLCOMP_ABI::IDX_SAMPLER,        hgl::SPVDescriptorKind::StorageSampler       },
                { GLSLCOMP_ABI::IDX_SAMPLED_IMAGE,  hgl::SPVDescriptorKind::SampledImage         },
                { GLSLCOMP_ABI::IDX_STORAGE_IMAGE,  hgl::SPVDescriptorKind::StorageImage         },
            };
            uint32_t desc_total = 0;
            for (const auto &km : kmap)
                desc_total += old->resource[km.idx].count;
            if (desc_total > 0) {
                pd->descriptors.count = desc_total;
                pd->descriptors.items = new hgl::SPVDescriptorBinding[desc_total];
                uint32_t di = 0;
                for (const auto &km : kmap) {
                    const auto &src = old->resource[km.idx];
                    for (uint32_t j = 0; j < src.count; ++j, ++di) {
                        auto &d = pd->descriptors.items[di];
                        strncpy(d.name, src.items[j].name, hgl::SPV_NAME_MAX - 1);
                        d.name[hgl::SPV_NAME_MAX - 1] = '\0';
                        d.set          = src.items[j].set;
                        d.binding      = src.items[j].binding;
                        d.kind         = km.kind;
                        d.array_count  = 1;       // not in old ABI
                        d.buffer_size  = 0;       // not in old ABI
                        d.member_count = 0;
                        d.members      = nullptr;
                    }
                }
            }

            // --- push constants ---
            const uint32_t npc = old->push_constant.count;
            if (npc > 0) {
                pd->push_constants.count = npc;
                pd->push_constants.items = new hgl::SPVPushConstantRange[npc];
                for (uint32_t i = 0; i < npc; ++i) {
                    const auto &s = old->push_constant.items[i];
                    auto       &d = pd->push_constants.items[i];
                    strncpy(d.name, s.name, hgl::SPV_NAME_MAX - 1);
                    d.name[hgl::SPV_NAME_MAX - 1] = '\0';
                    d.offset = s.offset; // uint8_t → uint32_t (see GLSLCompiler_Update.md)
                    d.size   = s.size;
                }
            }

            // --- subpass inputs ---
            const uint32_t ns = old->subpass_input.count;
            if (ns > 0) {
                pd->subpass_inputs.count = ns;
                pd->subpass_inputs.items = new hgl::SPVSubpassInput[ns];
                for (uint32_t i = 0; i < ns; ++i) {
                    const auto &s = old->subpass_input.items[i];
                    auto       &d = pd->subpass_inputs.items[i];
                    strncpy(d.name, s.name, hgl::SPV_NAME_MAX - 1);
                    d.name[hgl::SPV_NAME_MAX - 1] = '\0';
                    d.attachment_index = s.input_attachment_index;
                    d.binding          = s.binding;
                }
            }

            return pd;
        }

        // Free all ULRE-allocated arrays in a converted hgl::SPVParseData.
        static void FreeConvertedSPVParseData(hgl::SPVParseData *pd)
        {
            if (!pd) return;
            for (uint32_t i = 0; i < pd->descriptors.count; ++i)
                delete[] pd->descriptors.items[i].members;
            delete[] pd->stage_inputs.items;
            delete[] pd->stage_outputs.items;
            delete[] pd->descriptors.items;
            delete[] pd->push_constants.items;
            delete[] pd->subpass_inputs.items;
            delete pd;
        }

        hgl::SPVParseData *ParseSPVData(const SPVData *spv_data)
        {
            if (!gsi || !spv_data || !spv_data->result)
                return nullptr;

            // Call old DLL ParseSPV, get raw pointer, convert, free DLL copy.
            RawParsePtr raw = gsi->ParseSPV(const_cast<SPVData *>(spv_data));
            if (!raw) return nullptr;

            hgl::SPVParseData *result = ConvertOldSPVParseData(
                reinterpret_cast<const GLSLCOMP_ABI::SPVParseData *>(raw));
            gsi->FreeParseSPVData(raw);
            return result;
        }

        void FreeSPVParseData(hgl::SPVParseData *parse_data)
        {
            FreeConvertedSPVParseData(parse_data);
        }
    }//namespace graph
}//namespace hgl
