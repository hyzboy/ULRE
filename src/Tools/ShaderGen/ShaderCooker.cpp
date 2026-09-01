/// ShaderCooker.cpp — 离线 shader 预编译（方向三：离线 cook + 运行时加载）
///
/// 枚举 材质定义 × purpose × 图元类型 变体，经与生产完全相同的管线
///（CreateMaterialFromDefinition + FinalizeShaderBuildContext）编译 SPV 并写入
/// ShaderArtifactStore（BuildIfMissing）。产物随构建分发后，运行时以
/// ULRE_SHADER_CACHE_MODE=readonly 启动即可跳过全部 glslang 编译。
///
/// 用法：
///   ShaderCooker [--store <path>] [--profile <collector.json>]
///                [--material <definition_id>] [--lines] [--list]
///
///   --store    artifact store 根目录（默认与运行时同源：环境变量
///              ULRE_SHADER_CACHE_PATH > exe 所在目录）
///   --profile  目标设备 collector JSON（VulkanPhysicalDeviceProfileCollector
///              产出）。编译器 profile 哈希参与缓存 key——为特定设备 cook
///              必须传该设备的 JSON，否则 key 与运行时不匹配
///   --material 只 cook 指定 definition_id（冒烟测试/增量补齐）
///   --lines    额外 cook Lines 图元变体（LineQuad mesh 模式）
///   --list     仅列出将 cook 的变体，不编译
///
/// 退出码：全部变体构建成功 = 0；存在失败 = 1（Sky 材质的 depth/shadow
/// 用途为引擎定义的不支持组合，自动跳过，不计入失败）。

#include <hgl/CoreType.h>
#include <hgl/ShaderCompilerAPI.h>
#include <hgl/log/Log.h>
#include <hgl/filesystem/FileSystem.h>

#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/MaterialDefinitionFile.h>
#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/ShaderArtifactStore.h>
#include <hgl/mtl/ShaderCompilerProfileAPI.h>
#include <hgl/mtl/contract/ShaderGenPhysicalDeviceProfileJson.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/filesystem/Path.h>
#include <hgl/type/Smart.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    using namespace hgl;
    using namespace hgl::graph;

    struct CookOptions
    {
        OSString store_root;                 // 空 = 默认解析
        std::string profile_json_path;
        std::string material_filter;
        bool cook_lines = false;
        bool list_only = false;
    };

    bool ParseArgs(const int argc, char **argv, CookOptions &options)
    {
        for (int i = 1; i < argc; ++i)
        {
            const char *arg = argv[i];

            if (std::strcmp(arg, "--store") == 0)
            {
                if (++i >= argc)
                    return false;
                options.store_root = ToOSString(argv[i]);
            }
            else if (std::strcmp(arg, "--profile") == 0)
            {
                if (++i >= argc)
                    return false;
                options.profile_json_path = argv[i];
            }
            else if (std::strcmp(arg, "--material") == 0)
            {
                if (++i >= argc)
                    return false;
                options.material_filter = argv[i];
            }
            else if (std::strcmp(arg, "--lines") == 0)
            {
                options.cook_lines = true;
            }
            else if (std::strcmp(arg, "--list") == 0)
            {
                options.list_only = true;
            }
            else
            {
                std::fprintf(stderr, "Unknown argument: %s\n", arg);
                return false;
            }
        }
        return true;
    }

    // 与运行时 GetRuntimeShaderArtifactStore 同源的根目录解析：
    // 环境变量 ULRE_SHADER_CACHE_PATH > exe 所在目录 > cwd
    OSString ResolveStoreRoot()
    {
        const wchar_t *env_value = _wgetenv(L"ULRE_SHADER_CACHE_PATH");
        if (env_value && env_value[0])
            return OSString(env_value);

        OSString program_path;
        if (filesystem::GetCurrentProgramPath(program_path))
            return program_path;

        OSString current_path;
        if (filesystem::GetCurrentPath(current_path))
            return current_path;

        return OSString();
    }

    // 材质顶点语义 → 代表性格式。与引擎内联几何的常规布局对齐：
    // cook 出的是"该材质的规范几何类"变体（运行时几何恰好多携带属性时，
    // vertex_input_hash 不同 → 开发模式 BuildIfMissing 兜底）。
    bool AppendSemanticFormat(
        GeometryVertexFormat &geometry,
        const VertexSemantic semantic)
    {
        VkFormat format = VK_FORMAT_UNDEFINED;
        switch (semantic)
        {
        case VertexSemantic::Position:    format = VF_V3F;   break;
        case VertexSemantic::TexCoord:    format = VF_V2F;   break;
        case VertexSemantic::Normal:      format = VF_V3F;   break;
        case VertexSemantic::Tangent:     format = VF_V3F;   break;
        case VertexSemantic::Bitangent:   format = VF_V3F;   break;
        case VertexSemantic::Color:       format = VF_V4UN8; break;
        case VertexSemantic::Luminance:   format = VF_V1UN8; break;
        case VertexSemantic::TransformID: format = VF_V1U;   break;
        case VertexSemantic::Size:        format = VF_V1F;   break;
        default:                                                return false;
        }
        // vec_size/stride 由格式推断（Add 内部 InferVecSizeFromFormat/GetStrideByFormat）
        return geometry.Add(semantic, format, 0, 0);
    }

    std::string PurposeName(const mtl::ShaderProgramPurpose purpose)
    {
        switch (purpose)
        {
        case mtl::ShaderProgramPurpose::ForwardColor: return "forward";
        case mtl::ShaderProgramPurpose::DepthOnly:    return "depth";
        case mtl::ShaderProgramPurpose::ShadowDepth:  return "shadow";
        default:                                      return "unknown";
        }
    }

    std::string PrimitiveName(const PrimitiveType primitive)
    {
        return primitive == PrimitiveType::Lines ? "lines" : "triangles";
    }
}

int main(const int argc, char **argv)
{
    CookOptions options;
    if (!ParseArgs(argc, argv, options))
    {
        std::fprintf(stderr,
            "usage: ShaderCooker [--store <path>] [--profile <collector.json>]\n"
            "                   [--material <definition_id>] [--lines] [--list]\n");
        return 2;
    }

    // ── 编译器初始化（glslang 插件；cwd/exe 目录须有 GLSLCompiler.dll）──
    if (!InitShaderCompiler())
    {
        std::fprintf(stderr,
            "[ShaderCooker] InitShaderCompiler failed — run from a directory "
            "containing GLSLCompiler.dll (repo root)\n");
        return 2;
    }

    // ── sampler 预设（与 GraphicsContext 同源加载）─────────────────────
    // 采样器下标宏（#define <name>Sampler <idx>u）依赖此表；不加载则全部
    // 缺失，材质 FS 编译报 undeclared identifier。
    {
        const hgl::filesystem::Path sampler_toml =
            hgl::filesystem::Path(ToOSString(mtl::GetShaderLibraryPath()))
            / OSString(OS_TEXT("sampler.toml"));
        if (!mtl::SamplerPresetLibrary::Instance().Load(sampler_toml.ToOSString()))
        {
            std::fprintf(stderr,
                "[ShaderCooker] sampler.toml load failed - sampler macros would be missing\n");
            return 2;
        }
    }

    // ── 设备 profile（参与缓存 key）────────────────────────────────────
    mtl::contract::PhysicalDeviceProfileLite profile{};
    bool has_profile = false;
    if (!options.profile_json_path.empty())
    {
        int64 json_size = 0;
        void *json_data = filesystem::LoadFileToMemory(
            ToOSString(options.profile_json_path), json_size);
        if (!json_data)
        {
            std::fprintf(stderr, "[ShaderCooker] cannot read profile json: %s\n",
                         options.profile_json_path.c_str());
            return 2;
        }
        const std::string json_text(
            static_cast<const char *>(json_data), size_t(json_size));
        delete[] static_cast<uint8 *>(json_data);

        if (!mtl::contract::BuildPhysicalDeviceProfileFromCollectorJson(
                json_text, profile))
        {
            std::fprintf(stderr, "[ShaderCooker] invalid profile json: %s\n",
                         options.profile_json_path.c_str());
            return 2;
        }
        mtl::SetShaderCompilerPhysicalDeviceProfile(profile);
        has_profile = true;
    }

    // ── 材质定义（TOML 目录随 ShaderLibrary 定位自动加载）──────────────
    const mtl::MaterialDefinitionFileRegistry &registry =
        mtl::GetMaterialDefinitionFileRegistry();
    const int definition_count = registry.GetCount();
    if (definition_count <= 0)
    {
        std::fprintf(stderr, "[ShaderCooker] no material definitions loaded\n");
        return 2;
    }

    // ── store ──────────────────────────────────────────────────────────
    const OSString store_root = !options.store_root.IsEmpty()
        ? options.store_root : ResolveStoreRoot();
    if (store_root.IsEmpty())
    {
        std::fprintf(stderr, "[ShaderCooker] cannot resolve store root\n");
        return 2;
    }
    mtl::ShaderArtifactStore store(store_root, mtl::ShaderCacheMode::BuildIfMissing);
    {
        const U8String root_utf8 = ToU8String(store_root);
        GLogInfo(u8"[ShaderCooker] store root=%s", root_utf8.c_str());
    }

    // ── 枚举并 cook ────────────────────────────────────────────────────
    const mtl::ShaderProgramPurpose purposes[] =
    {
        mtl::ShaderProgramPurpose::ForwardColor,
        mtl::ShaderProgramPurpose::DepthOnly,
        mtl::ShaderProgramPurpose::ShadowDepth,
    };

    int cooked = 0;
    int failed = 0;
    int skipped = 0;
    int listed = 0;

    for (int def_index = 0; def_index < definition_count; ++def_index)
    {
        const mtl::MaterialDefinitionFileData *file_data = registry.GetAt(def_index);
        if (!file_data)
            continue;
        const mtl::MaterialDefinition &definition = file_data->definition;

        if (!options.material_filter.empty()
         && definition.definition_id != options.material_filter)
            continue;

        // 该材质的规范几何：Position（按节点配置的输入模式选格式）+ 语义需求全集。
        // 2D 材质（Vec2Position/Vec2IntPosition）的 s2/s3 模块按 vec2/int 构造，
        // 强加 vec3 会直接编译失败。
        GeometryVertexFormat geometry;
        {
            VkFormat position_format = VK_FORMAT_UNDEFINED;
            switch (definition.vertex_node_config.input)
            {
            case mtl::VertexInputMode::Vec2Position:    position_format = VF_V2F; break;
            case mtl::VertexInputMode::Vec3Position:    position_format = VF_V3F; break;
            case mtl::VertexInputMode::Vec2IntPosition: position_format = VF_V2I; break;
            case mtl::VertexInputMode::None:            break;
            default:                               position_format = VF_V3F; break;
            }
            if (position_format != VK_FORMAT_UNDEFINED
             && !geometry.Add(VertexSemantic::Position, position_format, 0, 0))
                ++skipped;
        }
        for (int req_index = 0;
             req_index < definition.vertex_semantic_requirements.GetCount();
             ++req_index)
        {
            const VertexSemantic semantic =
                GetVertexSemanticFromGLSLCodeModuleSemantic(
                    definition.vertex_semantic_requirements[req_index].semantic);
            if (semantic == VertexSemantic::Unknown)
                continue;
            if (!AppendSemanticFormat(geometry, semantic))
                ++skipped;
        }

        for (const mtl::ShaderProgramPurpose purpose : purposes)
        {
            // Sky 材质无深度/阴影 pass（引擎定义的不支持组合）
            if (purpose != mtl::ShaderProgramPurpose::ForwardColor
             && definition.compositor_surface == SurfaceType::Sky)
            {
                ++skipped;
                continue;
            }

            const PrimitiveType primitives[] = {PrimitiveType::Triangles, PrimitiveType::Lines};
            for (const PrimitiveType primitive : primitives)
            {
                if (primitive == PrimitiveType::Lines
                 && (!options.cook_lines
                  || IsCharQuadMode(definition.mesh_shader_mode)))
                    continue;

                ++listed;
                if (options.list_only)
                {
                    std::printf("cook %s [%s/%s]\n",
                                definition.definition_id.c_str(),
                                PurposeName(purpose).c_str(),
                                PrimitiveName(primitive).c_str());
                    continue;
                }

                mtl::MaterialRecipe recipe{};
                recipe.mtl_def_id = definition.definition_id;
                mtl::NormalizeRecipe(recipe);

                mtl::MaterialDefinitionBuildRequest request{};
                request.recipe = std::move(recipe);
                request.primitive_type = primitive;
                request.geometry_vertex_format = &geometry;
                request.override_shader_program_purpose = true;
                request.shader_program_purpose = purpose;
                request.shader_artifact_store = &store;
                request.defer_finalize = false;

                AutoDelete<mtl::ShaderBuildContext> build_context =
                    mtl::CreateMaterialFromDefinition(
                        has_profile ? &profile : nullptr,
                        definition,
                        request);
                if (!build_context)
                {
                    ++failed;
                    std::fprintf(stderr,
                        "[ShaderCooker] FAIL %s [%s/%s] (build)\n",
                        definition.definition_id.c_str(),
                        PurposeName(purpose).c_str(),
                        PrimitiveName(primitive).c_str());
                    continue;
                }

                if (!mtl::FinalizeShaderBuildContext(build_context))
                {
                    ++failed;
                    std::fprintf(stderr,
                        "[ShaderCooker] FAIL %s [%s/%s] (compile/save)\n",
                        definition.definition_id.c_str(),
                        PurposeName(purpose).c_str(),
                        PrimitiveName(primitive).c_str());
                    continue;
                }

                ++cooked;
                std::printf("[ShaderCooker] ok %s [%s/%s]\n",
                            definition.definition_id.c_str(),
                            PurposeName(purpose).c_str(),
                            PrimitiveName(primitive).c_str());
            }
        }
    }

    if (options.list_only)
    {
        std::printf("[ShaderCooker] %d variants listed (%d skipped)\n",
                    listed, skipped);
        return 0;
    }

    std::printf("[ShaderCooker] done: cooked=%d failed=%d skipped=%d (store=%ls)\n",
                cooked, failed, skipped,
                store_root.IsEmpty() ? L"<empty>" : store_root.c_str());
    return failed > 0 ? 1 : 0;
}
