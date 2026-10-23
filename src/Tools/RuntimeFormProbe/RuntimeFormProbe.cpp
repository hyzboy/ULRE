/**
 * RuntimeFormProbe —— 「发行形态」可执行证明。
 *
 * 只链接 `ULRE::Runtime`（永不含 `ULRE.ShaderGen` / glslang），因此本文件能
 * 编出来、这一行链接能过，本身就是「运行时不依赖生成器」的硬断言。
 *
 * 它消费由开发形态烘焙出来的 shader-cache 夹具（`program_manifest.pmf` +
 * `compute_manifest.pmf` + 各 stage SPV + 两份 expectation），用
 * `ShaderCacheMode::ReadOnly` 打开，验证发行形态的真实路径：
 *
 *   1. 清单能从磁盘装载（冷启动，没有任何同进程残留状态）；
 *   2. 运行时自算的语义键与烘焙时一致；
 *   3. 命中后装配出的 build context 与原 program 身份完全一致，SPV 字节正确；
 *   4. compute 程序按名字查表能取到与烘焙期逐字节相同的 SPV；
 *   5. 未命中的键**硬失败**（发行形态没有编译器可回落）；
 *   6. ReadOnly 下不回写任何清单。
 *
 * 夹具由 `ShaderResourceSchemaRegressionGate emit-release-cache <dir>` 产出。
 */

#include <hgl/common/RenderOptions.h>
#include <hgl/CoreType.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/SceneRenderTemplateResolver.h>
#include <hgl/mtl/ShaderArtifactStore.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderComputeProgramManifest.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/ShaderProgramBuildBackend.h>
#include <hgl/mtl/ShaderProgramManifest.h>
#include <hgl/mtl/ShaderProgramManifestBackend.h>
#include <hgl/mtl/ShaderProgramManifestKey.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/log/Log.h>
#include <hgl/utf.h>
#include <hgl/util/hash/FNV1a.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::graph::mtl;

namespace
{
    struct Expectation
    {
        std::string definition_id;
        uint64 program_digest = 0;
        uint64 lookup_key = 0;
        uint64 mesh_stage_digest = 0;
        uint64 fragment_stage_digest = 0;
        uint32 mesh_spv_bytes = 0;
        uint32 fragment_spv_bytes = 0;
        bool requires_recipe_runtime_rows = false;
        bool has_all = false;
    };

    /// expectation.txt 的字段名与
    /// ShaderResourceSchemaRegressionGate.cpp 的 WriteManifestExpectation() 严格对应。
    bool LoadExpectation(const std::filesystem::path &path, Expectation &out)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            std::fprintf(stderr,
                         "[FAIL] expectation file unreadable: %s\n",
                         path.string().c_str());
            return false;
        }

        int seen = 0;
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            const size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            const std::string key = line.substr(0, eq);
            const std::string value = line.substr(eq + 1);
            auto number = [&value]() -> unsigned long long
            {
                return std::strtoull(value.c_str(), nullptr, 10);
            };

            if (key == "definition_id")
            {
                out.definition_id = value;
                ++seen;
            }
            else if (key == "program_digest")
            {
                out.program_digest = number();
                ++seen;
            }
            else if (key == "lookup_key")
            {
                out.lookup_key = number();
                ++seen;
            }
            else if (key == "mesh_stage_digest")
            {
                out.mesh_stage_digest = number();
                ++seen;
            }
            else if (key == "fragment_stage_digest")
            {
                out.fragment_stage_digest = number();
                ++seen;
            }
            else if (key == "mesh_spv_bytes")
            {
                out.mesh_spv_bytes = static_cast<uint32>(number());
                ++seen;
            }
            else if (key == "fragment_spv_bytes")
            {
                out.fragment_spv_bytes = static_cast<uint32>(number());
                ++seen;
            }
            else if (key == "requires_recipe_runtime_rows")
            {
                out.requires_recipe_runtime_rows = number() != 0;
                ++seen;
            }
        }

        out.has_all = seen == 8 && !out.definition_id.empty();
        if (!out.has_all)
        {
            std::fprintf(stderr,
                         "[FAIL] expectation file incomplete (%d/8 fields): %s\n",
                         seen, path.string().c_str());
            return false;
        }

        return true;
    }

    /// compute_expectation.txt 的字段名与回归门 WriteComputeExpectation() 严格对应。
    struct ComputeExpectation
    {
        std::string program_name;
        uint64 spv_digest = 0;
        uint32 spv_bytes = 0;
        uint32 push_constant_size = 0;
        bool has_all = false;
    };

    bool LoadComputeExpectation(
        const std::filesystem::path &path, ComputeExpectation &out)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            std::fprintf(stderr,
                         "[FAIL] compute expectation file unreadable: %s\n",
                         path.string().c_str());
            return false;
        }

        int seen = 0;
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            const size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            const std::string key = line.substr(0, eq);
            const std::string value = line.substr(eq + 1);
            auto number = [&value]() -> unsigned long long
            {
                return std::strtoull(value.c_str(), nullptr, 10);
            };

            if (key == "program_name")
            {
                out.program_name = value;
                ++seen;
            }
            else if (key == "spv_digest")
            {
                out.spv_digest = number();
                ++seen;
            }
            else if (key == "spv_bytes")
            {
                out.spv_bytes = static_cast<uint32>(number());
                ++seen;
            }
            else if (key == "push_constant_size")
            {
                out.push_constant_size = static_cast<uint32>(number());
                ++seen;
            }
        }

        out.has_all = seen == 4 && !out.program_name.empty();
        if (!out.has_all)
        {
            std::fprintf(stderr,
                         "[FAIL] compute expectation file incomplete (%d/4 fields): %s\n",
                         seen, path.string().c_str());
            return false;
        }

        return true;
    }

    class Probe
    {
        std::vector<std::string> failures_;

    public:
        void Check(const bool ok, const char *what)
        {
            if (ok)
                return;

            failures_.push_back(what);
            std::fprintf(stderr, "  - %s\n", what);
        }

        bool Passed() const { return failures_.empty(); }
        size_t FailureCount() const { return failures_.size(); }
    };

    /// 与烘焙侧（回归门 RunProgramManifestLookupCase）逐字段相同的请求构造。
    /// 两侧一旦漂移，键对不上就会在这里炸出来 —— 这正是本探针要守的东西。
    bool BuildProbeRequest(
        const MaterialDefinition &definition,
        MaterialDefinitionBuildRequest &request,
        std::string &error)
    {
        static const GeometryVertexFormat geometry{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F}
        };

        request = MaterialDefinitionBuildRequest{};
        request.recipe.mtl_def_id = definition.definition_id;
        NormalizeRecipe(request.recipe);
        request.geometry_vertex_format = &geometry;
        request.defer_finalize = true;
        request.shader_program_purpose = ShaderProgramPurpose::ForwardColor;

        RenderTemplateValidationDiagnostic template_diagnostic{};
        if (!ResolveSceneRenderTemplateRequest(
                RenderTemplateID::ForwardLitShadowedAO,
                hgl::graph::ShaderStage::Fragment,
                MakeIdentityForwardLitProfile(),
                request.render_template_request, template_diagnostic))
        {
            error = "render template resolution failed (runtime-only API)";
            return false;
        }

        if (!AppendMaterialRenderTemplateRoots(
                definition, request.render_template_request))
        {
            error = "material render template roots could not be appended";
            return false;
        }

        return true;
    }

    int Run(const std::filesystem::path &cache_root)
    {
        // 发行形态真实缓存模式：只读，命中即用，未命中硬失败，绝不回写。
        ShaderArtifactStore store(
            hgl::OSString(cache_root.wstring().c_str()),
            ShaderCacheMode::ReadOnly);

        if (store.GetCacheMode() != ShaderCacheMode::ReadOnly)
        {
            std::fprintf(stderr, "[FAIL] artifact store was not opened read-only\n");
            return 1;
        }

        // 清单与 expectation.txt 都放在 `<root>/shader-cache/` 下 —— 路径由
        // MakeShaderProgramManifestPath 唯一决定，探针不自己拼目录布局。
        const std::filesystem::path manifest_path(
            MakeShaderProgramManifestPath(store.GetRootPath()).c_str());

        Expectation expected{};
        if (!LoadExpectation(manifest_path.parent_path() / "expectation.txt", expected))
            return 1;

        if (!std::filesystem::exists(manifest_path))
        {
            std::fprintf(stderr, "[FAIL] manifest missing: %s\n",
                         manifest_path.string().c_str());
            return 1;
        }

        MaterialDefinition definition{};
        if (!TryGetMaterialDefinitionByID(expected.definition_id, definition))
        {
            std::fprintf(stderr,
                         "[FAIL] material definition unavailable at runtime: %s\n",
                         expected.definition_id.c_str());
            return 1;
        }

        MaterialDefinitionBuildRequest request{};
        std::string build_error;
        if (!BuildProbeRequest(definition, request, build_error))
        {
            std::fprintf(stderr, "[FAIL] %s\n", build_error.c_str());
            return 1;
        }

        const auto manifest_size_before =
            std::filesystem::file_size(manifest_path);
        const auto manifest_time_before =
            std::filesystem::last_write_time(manifest_path);

        Probe probe;

        probe.Check(EnsureShaderProgramManifestBackend(&store),
                    "release form: manifest backend could not be installed");
        probe.Check(GetShaderProgramManifestEntryCount() == 1,
                    "release form: manifest did not load exactly one entry");

        ShaderProgramBuildBackend *backend = GetShaderProgramBuildBackend();
        probe.Check(backend != nullptr,
                    "release form: no program build backend is installed");
        if (!backend)
            return 1;

        // 运行时自算的语义键必须与烘焙侧一致 —— 这是「跨进程跨形态查表能对上」
        // 的唯一保证；不一致意味着键函数依赖了只有生成器才有的输入。
        probe.Check(ComputeMaterialProgramManifestKey(definition, request, nullptr)
                        == expected.lookup_key,
                    "release form: runtime semantic key differs from the baked lookup key");

        std::unique_ptr<ShaderBuildContext> context(
            backend->CreateBuildContext(nullptr, definition, request));
        probe.Check(context != nullptr,
                    "release form: manifest backend failed to assemble a build context");

        if (context)
        {
            probe.Check(context->HasProgramLink()
                            && context->GetProgramLink().BuildKey().GetDigest()
                                   == expected.program_digest,
                        "release form: assembled context carries a foreign program link");
            probe.Check(context->UsedCachedMeshStage()
                            && context->UsedCachedFragmentStage(),
                        "release form: assembled context did not use both cached stages");
            probe.Check(context->RequiresRecipeRuntimeRows()
                            == expected.requires_recipe_runtime_rows,
                        "release form: recipe runtime row requirement changed");
            probe.Check(backend->FinalizeBuildContext(context.get()),
                        "release form: finalize rejected the manifest-assembled context");

            const ShaderCreateInfo *mesh =
                context->GetStageShader(ShaderStage::Mesh);
            const ShaderCreateInfo *fragment =
                context->GetStageShader(ShaderStage::Fragment);

            probe.Check(mesh && mesh->GetSPVSize() == expected.mesh_spv_bytes,
                        "release form: mesh stage SPV size mismatch");
            probe.Check(fragment
                            && fragment->GetSPVSize() == expected.fragment_spv_bytes,
                        "release form: fragment stage SPV size mismatch");
            // 两个 stage 的假 SPV 共享魔数，只有第二个字不同 —— 跳过魔数比一格，
            // 确认装进来的是各自的 stage 数据，而不是同一份被装了两遍。
            probe.Check(mesh && fragment
                            && mesh->GetSPVSize() >= sizeof(uint64)
                            && fragment->GetSPVSize() >= sizeof(uint64)
                            && std::memcmp(mesh->GetSPVData(),
                                           fragment->GetSPVData(),
                                           sizeof(uint64)) != 0,
                        "release form: both stages loaded identical SPV payloads");
        }

        // 未命中的 ambient provider：发行形态必须硬失败，不能悄悄回落任何东西。
        {
            MaterialDefinitionBuildRequest miss = request;
            miss.ambient_module_name = "AmbientVertexGlobalColor";

            std::unique_ptr<ShaderBuildContext> miss_context(
                backend->CreateBuildContext(nullptr, definition, miss));
            probe.Check(!miss_context,
                        "release form: unknown ambient provider must hard-fail");

            probe.Check(ComputeMaterialProgramManifestKey(definition, miss, nullptr)
                            != expected.lookup_key,
                        "release form: unknown ambient provider must not alias a baked key");
        }

        // ReadOnly 下不回写：清单字节与时间戳都必须原样。
        {
            ShaderProgramManifestEntry entry{};
            entry.lookup_key = expected.lookup_key;
            entry.requires_recipe_runtime_rows =
                expected.requires_recipe_runtime_rows;

            probe.Check(!RecordShaderProgramManifestEntry(&store, nullptr, entry),
                        "release form: read-only store must refuse manifest writes");
            probe.Check(std::filesystem::file_size(manifest_path)
                            == manifest_size_before,
                        "release form: manifest file changed size during a read-only run");
            probe.Check(std::filesystem::last_write_time(manifest_path)
                            == manifest_time_before,
                        "release form: manifest file was rewritten during a read-only run");
        }

        // ── compute 程序：按名字只读查表 ─────────────────────────────
        // compute 在发行形态既没有材质语义键，也没有 mesh/fragment 两段 link，
        // 所以走另一份清单（compute_manifest.pmf）。这里验的是同一件事：冷进程
        // 能从磁盘取到与烘焙期逐字节相同的 SPV，且未知名字硬失败。
        {
            ComputeExpectation compute{};
            const std::filesystem::path compute_manifest_path(
                MakeShaderComputeProgramManifestPath(store.GetRootPath()).c_str());

            if (!LoadComputeExpectation(
                    compute_manifest_path.parent_path() / "compute_expectation.txt",
                    compute))
                return 1;

            if (!std::filesystem::exists(compute_manifest_path))
            {
                std::fprintf(stderr, "[FAIL] compute manifest missing: %s\n",
                             compute_manifest_path.string().c_str());
                return 1;
            }

            const auto compute_size_before =
                std::filesystem::file_size(compute_manifest_path);
            const auto compute_time_before =
                std::filesystem::last_write_time(compute_manifest_path);

            ValueArray<uint8> spv;
            uint32 push_constant_size = 0;
            probe.Check(LoadShaderComputeProgramArtifact(
                            store, compute.program_name.c_str(), spv,
                            &push_constant_size),
                        "release form: compute program not found in compute manifest");
            probe.Check(spv.GetCount() == static_cast<int>(compute.spv_bytes),
                        "release form: compute SPV size mismatch");

            {
                hgl::hash::FNV1aHasher64 hasher;
                if (spv.GetCount() > 0)
                    hasher.AppendBytes(spv.GetData(),
                                       static_cast<size_t>(spv.GetCount()));

                probe.Check(static_cast<uint64>(hasher) == compute.spv_digest,
                            "release form: compute SPV bytes differ from the baked artifact");
            }

            probe.Check(push_constant_size == compute.push_constant_size,
                        "release form: compute push constant contract changed");

            // 未命中的程序名：发行形态没有编译器可回落，必须硬失败。
            {
                ValueArray<uint8> miss_spv;
                probe.Check(!LoadShaderComputeProgramArtifact(
                                store, "Probe.Missing.ComputeProgram", miss_spv,
                                nullptr),
                            "release form: unknown compute program must hard-fail");
                probe.Check(miss_spv.GetCount() == 0,
                            "release form: failed compute lookup must not return data");
            }

            // ReadOnly 下不回写：清单字节与时间戳都必须原样。
            {
                probe.Check(!RecordShaderComputeProgramArtifact(
                                store, compute.program_name.c_str(),
                                spv.GetData(), static_cast<uint64>(spv.GetCount()),
                                compute.push_constant_size),
                            "release form: read-only store must refuse compute manifest writes");
                probe.Check(std::filesystem::file_size(compute_manifest_path)
                                == compute_size_before,
                            "release form: compute manifest changed size during a read-only run");
                probe.Check(std::filesystem::last_write_time(compute_manifest_path)
                                == compute_time_before,
                            "release form: compute manifest was rewritten during a read-only run");
            }
        }

        UninstallShaderProgramManifestBackend();

        if (!probe.Passed())
        {
            std::fprintf(stderr,
                         "[FAIL] runtime-form-probe: %zu check(s) failed\n",
                         probe.FailureCount());
            return 1;
        }

        std::printf("[PASS] runtime-form-probe: release form resolved '%s' from a "
                    "read-only baked cache (program=%llu)\n",
                    expected.definition_id.c_str(),
                    static_cast<unsigned long long>(expected.program_digest));
        return 0;
    }
}//namespace

int main(const int argc, char **argv)
{
#if ULRE_ENABLE_SHADERGEN
    std::fprintf(stderr,
                 "[WARN] built with ULRE_ENABLE_SHADERGEN=1; this probe is meant for "
                 "the release form (ULRE_ENABLE_SHADERGEN=OFF).\n");
#endif

    if (argc != 2)
    {
        std::fprintf(stderr,
                     "Usage: RuntimeFormProbe <release-cache-root>\n"
                     "  <release-cache-root> is the ShaderArtifactStore root; the "
                     "manifest lives in <root>/shader-cache/.\n"
                     "  It must contain program_manifest.pmf, the stage SPVs and "
                     "expectation.txt, produced by\n"
                     "  ShaderResourceSchemaRegressionGate emit-release-cache <dir>\n"
                     "Must be run with the working directory containing ShaderLibrary/ "
                     "(material definitions are release-form data).\n");
        return 2;
    }

    const std::filesystem::path cache_root =
        std::filesystem::absolute(std::filesystem::path(argv[1]));

    if (!std::filesystem::is_directory(cache_root))
    {
        std::fprintf(stderr, "[FAIL] cache dir not found: %s\n",
                     cache_root.string().c_str());
        return 2;
    }

    if (!std::filesystem::is_directory(
            std::filesystem::path("ShaderLibrary") / "material"))
    {
        std::fprintf(stderr,
                     "[FAIL] ShaderLibrary/material not found under the working "
                     "directory; release form ships material definitions as data.\n");
        return 2;
    }

    return Run(cache_root);
}
