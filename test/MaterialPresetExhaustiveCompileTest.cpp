#include <hgl/graph/mtl/MaterialLibrary.h>
#include <hgl/graph/mtl/Material2DCreateConfig.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/contract/ShaderGenRequestBuilder.h>
#include <hgl/shadergen/contract/ShaderGenResultBuilder.h>
#include <hgl/shadergen/contract/ShaderGenMirrorDiff.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <deque>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace hgl::graph;
using namespace hgl::graph::mtl;


namespace hgl::graph
{
    bool InitShaderCompiler();
    void CloseShaderCompiler();
}

namespace
{
    struct PreflightResult
    {
        bool ok = false;
        std::string reason;
        bool glsl_compiler_missing = false;
        std::string stderr_log_path;
        std::string stderr_tail;
    };

    struct CompileCase
    {
        MaterialPreset preset;
        std::string preset_name;
        std::string variant_name;
        std::function<MaterialCreateInfo *()> create;
    };

    struct CaseReport
    {
        std::string preset_name;
        std::string variant_name;
        bool create_ok = false;
        bool compile_ok = false;
        bool request_ok = false;
        bool result_ok = false;
        bool diff_ok = false;
        bool all_match = false;
        bool layout_match = false;
        bool vertex_match = false;
        bool spv_match = false;
        uint32_t spv_stage_count = 0;
        uint32_t warning_count = 0;
        uint32_t error_count = 0;
        uint32_t legacy_stage_combo = 0;
        uint32_t mirror_stage_combo = 0;
        std::string stderr_log_path;
        std::string stderr_tail;
    };

    class ScopedStderrCapture
    {
    private:
        int saved_fd = -1;
        bool active = false;

    public:
        ScopedStderrCapture(const std::filesystem::path &log_path)
        {
            std::error_code ec;
            std::filesystem::create_directories(log_path.parent_path(), ec);

            std::fflush(stderr);

        #if defined(_WIN32)
            saved_fd = _dup(_fileno(stderr));
        #else
            saved_fd = dup(fileno(stderr));
        #endif

            if (saved_fd < 0)
                return;

            if (!std::freopen(log_path.string().c_str(), "w", stderr))
            {
            #if defined(_WIN32)
                _close(saved_fd);
            #else
                close(saved_fd);
            #endif
                saved_fd = -1;
                return;
            }

            active = true;
        }

        ~ScopedStderrCapture()
        {
            if (!active)
                return;

            std::fflush(stderr);

        #if defined(_WIN32)
            _dup2(saved_fd, _fileno(stderr));
            _close(saved_fd);
        #else
            dup2(saved_fd, fileno(stderr));
            close(saved_fd);
        #endif
        }
    };

    static std::string ReadTailLines(const std::filesystem::path &file_path, const size_t max_lines)
    {
        std::ifstream in(file_path);
        if (!in.is_open())
            return {};

        std::deque<std::string> tail;
        std::string line;
        while (std::getline(in, line))
        {
            if (tail.size() >= max_lines)
                tail.pop_front();

            tail.push_back(line);
        }

        std::ostringstream oss;
        for (const auto &item : tail)
            oss << item << "\n";

        return oss.str();
    }

    static std::string LastNonEmptyLine(const std::string &text)
    {
        std::istringstream iss(text);
        std::string line;
        std::string last;
        while (std::getline(iss, line))
        {
            if (!line.empty())
                last = line;
        }
        return last;
    }

    static bool ContainsAnyToken(const std::string &text, const std::vector<std::string> &tokens)
    {
        for (const std::string &token : tokens)
        {
            if (!token.empty() && text.find(token) != std::string::npos)
                return true;
        }

        return false;
    }

    static std::string SanitizeForPath(const std::string &text)
    {
        std::string out;
        out.reserve(text.size());

        for (char ch : text)
        {
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_')
                out.push_back(ch);
            else
                out.push_back('_');
        }

        if (out.empty())
            out = "unnamed";

        return out;
    }

    template<typename ConfigType>
    static void AddCase(std::vector<CompileCase> &cases,
                        const MaterialPreset preset,
                        const std::string &preset_name,
                        const std::string &variant_name,
                        const ConfigType &cfg)
    {
        ConfigType cfg_copy = cfg;

        cases.push_back(CompileCase{
            preset,
            preset_name,
            variant_name,
            [preset, cfg_copy]() mutable -> MaterialCreateInfo *
            {
                return CreateMaterialCreateInfo(nullptr, preset, &cfg_copy);
            }
        });
    }

    static std::vector<SkyLightAmbientModel> AllSkyModels()
    {
        return {
            SkyLightAmbientModel::Simple,
            SkyLightAmbientModel::FakeAtmosphere,
            SkyLightAmbientModel::CubeMap,
            SkyLightAmbientModel::SphericalHarmonics,
            SkyLightAmbientModel::IBL,
        };
    }

    static const char *SkyModelName(const SkyLightAmbientModel model)
    {
        switch (model)
        {
            case SkyLightAmbientModel::Simple: return "Simple";
            case SkyLightAmbientModel::FakeAtmosphere: return "FakeAtmosphere";
            case SkyLightAmbientModel::CubeMap: return "CubeMap";
            case SkyLightAmbientModel::SphericalHarmonics: return "SphericalHarmonics";
            case SkyLightAmbientModel::IBL: return "IBL";
            default: return "Unknown";
        }
    }

    static void Build2DCases(std::vector<CompileCase> &cases, const MaterialPreset preset, const std::string &preset_name, const bool rect_only)
    {
        const std::vector<PrimitiveType> primitive_list = rect_only
            ? std::vector<PrimitiveType>{PrimitiveType::SolidRectangles, PrimitiveType::WireRectangles}
            : std::vector<PrimitiveType>{PrimitiveType::Lines, PrimitiveType::Triangles};

        for (const PrimitiveType prim : primitive_list)
        {
            for (const CoordinateSystem2D cs : {CoordinateSystem2D::NDC, CoordinateSystem2D::Ortho})
            {
                for (const bool l2w : {false, true})
                {
                    Material2DCreateConfig cfg(prim, cs, l2w ? WithLocalToWorld::With : WithLocalToWorld::Without);

                    std::ostringstream oss;
                    oss << "prim=" << int(prim)
                        << ",cs=" << int(cs)
                        << ",l2w=" << (l2w ? 1 : 0);

                    AddCase(cases, preset, preset_name, oss.str(), cfg);
                }
            }
        }
    }

    static void BuildText2DCases(std::vector<CompileCase> &cases, const MaterialPreset preset, const std::string &preset_name)
    {
        for (const bool l2w : {false, true})
        {
            Text2DMaterialCreateConfig cfg;
            cfg.local_to_world = l2w;

            std::ostringstream oss;
            oss << "text2d,l2w=" << (l2w ? 1 : 0);

            AddCase(cases, preset, preset_name, oss.str(), cfg);
        }
    }

    static void Build3DCases(std::vector<CompileCase> &cases, const MaterialPreset preset, const std::string &preset_name)
    {
        for (const SkyLightAmbientModel ambient : AllSkyModels())
        {
            for (const bool camera : {false, true})
            {
                for (const bool sky : {false, true})
                {
                    for (const bool l2w : {false, true})
                    {
                        Material3DCreateConfig cfg(
                            PrimitiveType::Triangles,
                            camera ? WithCamera::With : WithCamera::Without,
                            l2w ? WithLocalToWorld::With : WithLocalToWorld::Without,
                            sky ? WithSky::With : WithSky::Without);

                        cfg.sky_ambient_model = ambient;

                        std::ostringstream oss;
                        oss << "ambient=" << SkyModelName(ambient)
                            << ",camera=" << (camera ? 1 : 0)
                            << ",sky=" << (sky ? 1 : 0)
                            << ",l2w=" << (l2w ? 1 : 0);

                        AddCase(cases, preset, preset_name, oss.str(), cfg);
                    }
                }
            }
        }
    }

    static void BuildTerrainGridCases(std::vector<CompileCase> &cases, const MaterialPreset preset, const std::string &preset_name)
    {
        for (const SkyLightAmbientModel ambient : AllSkyModels())
        {
            TerrainGridCreateConfig cfg;
            cfg.sky_ambient_model = ambient;

            std::ostringstream oss;
            oss << "terrain,ambient=" << SkyModelName(ambient);

            AddCase(cases, preset, preset_name, oss.str(), cfg);
        }
    }

    static void BuildSkyMinimalCases(std::vector<CompileCase> &cases, const MaterialPreset preset, const std::string &preset_name)
    {
        for (const SkyLightAmbientModel ambient : AllSkyModels())
        {
            for (const bool camera : {false, true})
            {
                SkyMinimalCreateConfig cfg(camera ? WithCamera::With : WithCamera::Without);
                cfg.sky_ambient_model = ambient;

                std::ostringstream oss;
                oss << "skyminimal,ambient=" << SkyModelName(ambient)
                    << ",camera=" << (camera ? 1 : 0);

                AddCase(cases, preset, preset_name, oss.str(), cfg);
            }
        }
    }

    static void BuildBillboardCases(std::vector<CompileCase> &cases, const MaterialPreset preset, const std::string &preset_name)
    {
        for (const bool fixed_size : {false, true})
        {
            for (const SkyLightAmbientModel ambient : AllSkyModels())
            {
                BillboardMaterialCreateConfig cfg;
                cfg.fixed_size = fixed_size;
                cfg.sky_ambient_model = ambient;

                std::ostringstream oss;
                oss << "billboard,fixed=" << (fixed_size ? 1 : 0)
                    << ",ambient=" << SkyModelName(ambient);

                AddCase(cases, preset, preset_name, oss.str(), cfg);
            }
        }
    }

    static void BuildBasicLitCases(std::vector<CompileCase> &cases, const MaterialPreset preset, const std::string &preset_name)
    {
        for (const bool use_ibl : {false, true})
        {
            for (const SkyLightAmbientModel ambient : AllSkyModels())
            {
                BasicLitMaterialCreateConfig cfg(use_ibl);
                cfg.sky_ambient_model = ambient;

                std::ostringstream oss;
                oss << "basiclit,ibl=" << (use_ibl ? 1 : 0)
                    << ",ambient=" << SkyModelName(ambient);

                AddCase(cases, preset, preset_name, oss.str(), cfg);
            }
        }
    }

    static std::vector<CompileCase> BuildAllCases()
    {
        std::vector<CompileCase> cases;

        const std::vector<MaterialPreset> all_presets = {
            MaterialPreset::VertexColor2D,
            MaterialPreset::PureColor2D,
            MaterialPreset::PureTexture2D,
            MaterialPreset::RectTexture2D,
            MaterialPreset::RectTexture2DArray,
            MaterialPreset::Text2D,
            MaterialPreset::PureColor3D,
            MaterialPreset::VertexColor3D,
            MaterialPreset::VertexLuminance3D,
            MaterialPreset::VertexPattleColor3D,
            MaterialPreset::Gizmo3D,
            MaterialPreset::TextureBlinnPhong,
            MaterialPreset::TerrainGrid,
            MaterialPreset::SkyMinimal,
            MaterialPreset::Billboard2D,
            MaterialPreset::BasicLit,
        };

        for (const MaterialPreset preset : all_presets)
        {
            const char *name_cstr = GetInlineMaterialName(preset);
            const std::string preset_name = name_cstr ? name_cstr : "UnknownPreset";

            switch (preset)
            {
                case MaterialPreset::VertexColor2D:
                case MaterialPreset::PureColor2D:
                case MaterialPreset::PureTexture2D:
                    Build2DCases(cases, preset, preset_name, false);
                    break;

                case MaterialPreset::RectTexture2D:
                case MaterialPreset::RectTexture2DArray:
                    Build2DCases(cases, preset, preset_name, true);
                    break;

                case MaterialPreset::Text2D:
                    BuildText2DCases(cases, preset, preset_name);
                    break;

                case MaterialPreset::PureColor3D:
                case MaterialPreset::VertexColor3D:
                case MaterialPreset::VertexLuminance3D:
                case MaterialPreset::VertexPattleColor3D:
                case MaterialPreset::Gizmo3D:
                case MaterialPreset::TextureBlinnPhong:
                    Build3DCases(cases, preset, preset_name);
                    break;

                case MaterialPreset::TerrainGrid:
                    BuildTerrainGridCases(cases, preset, preset_name);
                    break;

                case MaterialPreset::SkyMinimal:
                    BuildSkyMinimalCases(cases, preset, preset_name);
                    break;

                case MaterialPreset::Billboard2D:
                    BuildBillboardCases(cases, preset, preset_name);
                    break;

                case MaterialPreset::BasicLit:
                    BuildBasicLitCases(cases, preset, preset_name);
                    break;

                default:
                    break;
            }
        }

        std::sort(cases.begin(), cases.end(), [](const CompileCase &lhs, const CompileCase &rhs)
        {
            if (lhs.preset_name != rhs.preset_name)
                return lhs.preset_name < rhs.preset_name;

            return lhs.variant_name < rhs.variant_name;
        });

        return cases;
    }

    static bool WriteTextFile(const std::filesystem::path &file_path, const std::string &text)
    {
        std::ofstream out(file_path, std::ios::binary);
        if (!out.is_open())
            return false;

        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(out);
    }

    static bool WriteSpvFile(const std::filesystem::path &file_path, const std::vector<uint32_t> &words)
    {
        std::ofstream out(file_path, std::ios::binary);
        if (!out.is_open())
            return false;

        out.write(reinterpret_cast<const char *>(words.data()), static_cast<std::streamsize>(words.size() * sizeof(uint32_t)));
        return static_cast<bool>(out);
    }

    static std::string BuildCaseDetailText(const CaseReport &rep,
                                           const contract::ShaderGenRequest *request,
                                           const contract::ShaderGenResult *result,
                                           const contract::ShaderGenMirrorDiffSummary *diff)
    {
        std::ostringstream oss;

        oss << "preset=" << rep.preset_name << "\n";
        oss << "variant=" << rep.variant_name << "\n";
        oss << "create_ok=" << (rep.create_ok ? 1 : 0) << "\n";
        oss << "compile_ok=" << (rep.compile_ok ? 1 : 0) << "\n";
        oss << "request_ok=" << (rep.request_ok ? 1 : 0) << "\n";
        oss << "result_ok=" << (rep.result_ok ? 1 : 0) << "\n";
        oss << "diff_ok=" << (rep.diff_ok ? 1 : 0) << "\n";
        oss << "all_match=" << (rep.all_match ? 1 : 0) << "\n";
        oss << "layout_match=" << (rep.layout_match ? 1 : 0) << "\n";
        oss << "vertex_match=" << (rep.vertex_match ? 1 : 0) << "\n";
        oss << "spv_match=" << (rep.spv_match ? 1 : 0) << "\n";
        oss << "spv_stage_count=" << rep.spv_stage_count << "\n";
        oss << "warning_count=" << rep.warning_count << "\n";
        oss << "error_count=" << rep.error_count << "\n";
        oss << "legacy_stage_combo=0x" << std::hex << rep.legacy_stage_combo << std::dec << "\n";
        oss << "mirror_stage_combo=0x" << std::hex << rep.mirror_stage_combo << std::dec << "\n";
        if (!rep.stderr_log_path.empty())
            oss << "stderr_log=" << rep.stderr_log_path << "\n";
        if (!rep.stderr_tail.empty())
            oss << "stderr_tail:\n" << rep.stderr_tail;

        if (request)
        {
            oss << "request.required_resources=" << request->required_resources.size() << "\n";
            oss << "request.vertex_requirements=" << request->vertex_requirements.size() << "\n";
        }

        if (result)
        {
            oss << "result.layout.bindings=" << result->layout.bindings.size() << "\n";
            oss << "result.vertex.attributes=" << result->vertex_layout.attributes.size() << "\n";
            oss << "result.buffer_structs=" << result->buffer_structs.size() << "\n";
            oss << "result.diagnostics.has_error=" << (result->diagnostics.has_error ? 1 : 0) << "\n";
            oss << "result.diagnostics.errors=" << result->diagnostics.errors.size() << "\n";
            oss << "result.diagnostics.warnings=" << result->diagnostics.warnings.size() << "\n";
        }

        if (diff)
        {
            oss << "diff.layout_hash_legacy=" << diff->layout_hash_legacy << "\n";
            oss << "diff.layout_hash_mirror=" << diff->layout_hash_mirror << "\n";
            oss << "diff.vertex_hash_legacy=" << diff->vertex_hash_legacy << "\n";
            oss << "diff.vertex_hash_mirror=" << diff->vertex_hash_mirror << "\n";
            oss << "diff.spv_hash_legacy=" << diff->spv_hash_legacy << "\n";
            oss << "diff.spv_hash_mirror=" << diff->spv_hash_mirror << "\n";
            oss << "diff.legacy_stage_summary=" << diff->legacy_stage_summary << "\n";
            oss << "diff.mirror_stage_summary=" << diff->mirror_stage_summary << "\n";
        }

        return oss.str();
    }

    static std::string BuildCsvHeader()
    {
        return "preset,variant,create_ok,compile_ok,request_ok,result_ok,diff_ok,all_match,layout_match,vertex_match,spv_match,spv_stage_count,warning_count,error_count,legacy_stage_combo,mirror_stage_combo\n";
    }

    static std::string BuildCsvRow(const CaseReport &rep)
    {
        std::ostringstream oss;
        oss
            << rep.preset_name << ','
            << '"' << rep.variant_name << '"' << ','
            << (rep.create_ok ? 1 : 0) << ','
            << (rep.compile_ok ? 1 : 0) << ','
            << (rep.request_ok ? 1 : 0) << ','
            << (rep.result_ok ? 1 : 0) << ','
            << (rep.diff_ok ? 1 : 0) << ','
            << (rep.all_match ? 1 : 0) << ','
            << (rep.layout_match ? 1 : 0) << ','
            << (rep.vertex_match ? 1 : 0) << ','
            << (rep.spv_match ? 1 : 0) << ','
            << rep.spv_stage_count << ','
            << rep.warning_count << ','
            << rep.error_count << ','
            << rep.legacy_stage_combo << ','
            << rep.mirror_stage_combo
            << '\n';

        return oss.str();
    }

    static std::string BuildSpvManifestHeader()
    {
        return "preset,variant,stage_mask,word_count,spv_file\n";
    }

    static std::string BuildSpvManifestRow(const std::string &preset,
                                           const std::string &variant,
                                           const uint32_t stage_mask,
                                           const size_t word_count,
                                           const std::filesystem::path &spv_file)
    {
        std::ostringstream oss;
        oss << preset << ','
            << '"' << variant << '"' << ','
            << stage_mask << ','
            << word_count << ','
            << '"' << spv_file.generic_string() << '"'
            << '\n';
        return oss.str();
    }

    static PreflightResult RunGLSLCompilerPreflight(const std::filesystem::path &out_root)
    {
        PreflightResult result;

        const std::filesystem::path preflight_dir = out_root / "preflight";
        std::error_code ec;
        std::filesystem::create_directories(preflight_dir, ec);

        const std::filesystem::path stderr_log = preflight_dir / "stderr.log";
        result.stderr_log_path = stderr_log.string();

        bool init_ok = false;
        {
            ScopedStderrCapture capture(stderr_log);

            init_ok = InitShaderCompiler();
        }

        result.stderr_tail = ReadTailLines(stderr_log, 40);

        const bool has_missing_glsl_msg = ContainsAnyToken(result.stderr_tail, {
            "Init failed: cannot load GLSLCompiler plugin module",
            "CompileShader failed: compiler not initialized"
        });

        result.glsl_compiler_missing = has_missing_glsl_msg;
        result.ok = init_ok;
        if (!result.ok)
        {
            if (result.glsl_compiler_missing)
                result.reason = "GLSLCompiler.dll not found or failed to load from current working directory";
            else
                result.reason = "InitShaderCompiler() failed";
        }

        return result;
    }
}

int main(int argc, char **argv)
{
    const std::filesystem::path out_root = (argc > 1 && argv[1] && argv[1][0])
        ? std::filesystem::path(argv[1])
        : std::filesystem::path("shadergen_materialpreset_dump");

    std::error_code ec;
    std::filesystem::create_directories(out_root, ec);

    const PreflightResult preflight = RunGLSLCompilerPreflight(out_root);
    if (!preflight.ok)
    {
        std::ostringstream fatal;
        fatal << "fatal=1\n"
              << "reason=" << (preflight.reason.empty() ? "InitShaderCompiler() failed" : preflight.reason) << "\n"
              << "stderr_log=" << preflight.stderr_log_path << "\n"
              << "stderr_tail:\n" << preflight.stderr_tail;

        WriteTextFile(out_root / "preflight.txt", fatal.str());

        std::ostringstream summary;
        summary << "total_cases=0\n"
                << "create_fail_count=0\n"
                << "compile_fail_count=0\n"
                << "mirror_fail_count=0\n"
                << "diff_mismatch_count=0\n"
                << "fatal=1\n"
                << "fatal_reason=" << (preflight.reason.empty() ? "InitShaderCompiler() failed" : preflight.reason) << "\n"
                << "stderr_log=" << preflight.stderr_log_path << "\n";

        WriteTextFile(out_root / "summary.txt", summary.str());
        WriteTextFile(out_root / "summary.csv", BuildCsvHeader());
        WriteTextFile(out_root / "spv_manifest.csv", BuildSpvManifestHeader());

        std::fprintf(stderr,
                     "[MaterialPresetExhaustiveCompile] fatal: GLSLCompiler load failed, abort before enumerating presets.\n");
        std::fprintf(stderr,
                     "[MaterialPresetExhaustiveCompile] details: %s\n",
                     preflight.stderr_log_path.c_str());
        CloseShaderCompiler();
        return 2;
    }

    const std::vector<CompileCase> cases = BuildAllCases();

    std::string csv = BuildCsvHeader();
    std::string spv_manifest = BuildSpvManifestHeader();

    uint32_t create_fail_count = 0;
    uint32_t compile_fail_count = 0;
    uint32_t mirror_fail_count = 0;
    uint32_t diff_mismatch_count = 0;

    std::printf("[MaterialPresetExhaustiveCompile] total_cases=%u output=%s\n",
                static_cast<unsigned>(cases.size()),
                out_root.string().c_str());

    for (size_t i = 0; i < cases.size(); ++i)
    {
        const CompileCase &c = cases[i];

        CaseReport rep;
        rep.preset_name = c.preset_name;
        rep.variant_name = c.variant_name;

        contract::ShaderGenRequest request;
        contract::ShaderGenResult result;
        contract::ShaderGenMirrorDiffSummary diff;

        contract::ShaderGenRequest *request_ptr = nullptr;
        contract::ShaderGenResult *result_ptr = nullptr;
        contract::ShaderGenMirrorDiffSummary *diff_ptr = nullptr;

        const std::filesystem::path case_dir = out_root
            / SanitizeForPath(c.preset_name)
            / SanitizeForPath(c.variant_name);
        std::filesystem::create_directories(case_dir, ec);

        const std::filesystem::path stderr_log = case_dir / "stderr.log";
        rep.stderr_log_path = stderr_log.string();

        MaterialCreateInfo *mci = nullptr;
        {
            ScopedStderrCapture capture(stderr_log);
            mci = c.create();
            rep.create_ok = (mci != nullptr);

            if (!rep.create_ok)
            {
                ++create_fail_count;
            }
            else
            {
                rep.compile_ok = mci->CreateShader();
                if (!rep.compile_ok)
                    ++compile_fail_count;

                rep.request_ok = contract::BuildShaderGenRequestFromMaterialCreateInfo(*mci, request, c.preset_name.c_str());
                if (rep.request_ok)
                    request_ptr = &request;

                rep.result_ok = contract::BuildShaderGenResultFromMaterialCreateInfo(*mci, result);
                if (rep.result_ok)
                {
                    result_ptr = &result;
                    rep.spv_stage_count = static_cast<uint32_t>(result.spv_per_stage.size());
                    rep.warning_count = static_cast<uint32_t>(result.diagnostics.warnings.size());
                    rep.error_count = static_cast<uint32_t>(result.diagnostics.errors.size());
                }
                else
                {
                    ++mirror_fail_count;
                }

                rep.diff_ok = rep.result_ok && contract::BuildShaderGenMirrorDiffSummary(*mci, result, diff);
                if (rep.diff_ok)
                {
                    diff_ptr = &diff;
                    rep.all_match = diff.all_match;
                    rep.layout_match = diff.layout_match;
                    rep.vertex_match = diff.vertex_match;
                    rep.spv_match = diff.spv_match;
                    rep.legacy_stage_combo = diff.legacy_stage_combo;
                    rep.mirror_stage_combo = diff.mirror_stage_combo;

                    if (!diff.all_match)
                        ++diff_mismatch_count;
                }

                delete mci;
            }
        }

        rep.stderr_tail = ReadTailLines(stderr_log, 20);
        const std::string stderr_last = LastNonEmptyLine(rep.stderr_tail);

        const std::string detail_text = BuildCaseDetailText(rep, request_ptr, result_ptr, diff_ptr);
        WriteTextFile(case_dir / "result.txt", detail_text);

        if (result_ptr)
        {
            std::ostringstream layout_text;
            std::vector<contract::DescriptorBindingDesc> bindings = result.layout.bindings;
            std::sort(bindings.begin(), bindings.end(), [](const auto &lhs, const auto &rhs)
            {
                if (lhs.set != rhs.set)
                    return lhs.set < rhs.set;
                return lhs.binding < rhs.binding;
            });

            for (const auto &b : bindings)
            {
                layout_text << "set=" << b.set
                            << ",binding=" << b.binding
                            << ",class=" << int(b.resource_class)
                            << ",stage_mask=0x" << std::hex << b.stage_mask << std::dec
                            << ",name=" << b.name
                            << ",type=" << b.type_name
                            << "\n";
            }

            WriteTextFile(case_dir / "layout.txt", layout_text.str());

            std::ostringstream vertex_text;
            std::vector<contract::VertexAttributeDesc> attrs = result.vertex_layout.attributes;
            std::sort(attrs.begin(), attrs.end(), [](const auto &lhs, const auto &rhs)
            {
                return lhs.location < rhs.location;
            });

            for (const auto &a : attrs)
            {
                vertex_text << "location=" << a.location
                            << ",semantic=" << a.semantic
                            << ",type=" << a.type_name
                            << ",rate=" << a.input_rate
                            << "\n";
            }

            WriteTextFile(case_dir / "vertex.txt", vertex_text.str());

            for (const auto &blob : result.spv_per_stage)
            {
                std::ostringstream spv_name;
                spv_name << "spv_stage_0x" << std::hex << blob.stage_mask << std::dec << ".spv";
                const std::filesystem::path spv_path = case_dir / spv_name.str();
                if (WriteSpvFile(spv_path, blob.words))
                {
                    std::error_code rel_ec;
                    std::filesystem::path rel_path = std::filesystem::relative(spv_path, out_root, rel_ec);
                    if (rel_ec)
                        rel_path = spv_path;

                    spv_manifest += BuildSpvManifestRow(
                        c.preset_name,
                        c.variant_name,
                        blob.stage_mask,
                        blob.words.size(),
                        rel_path);
                }
            }
        }

        csv += BuildCsvRow(rep);

        std::printf("[%4u/%4u] preset=%s variant=%s create=%d compile=%d result=%d diff=%d all_match=%d\n",
                    static_cast<unsigned>(i + 1),
                    static_cast<unsigned>(cases.size()),
                    c.preset_name.c_str(),
                    c.variant_name.c_str(),
                    rep.create_ok ? 1 : 0,
                    rep.compile_ok ? 1 : 0,
                    rep.result_ok ? 1 : 0,
                    rep.diff_ok ? 1 : 0,
                    rep.all_match ? 1 : 0);

        if ((!rep.create_ok || !rep.compile_ok || !rep.result_ok || !rep.diff_ok) && !stderr_last.empty())
        {
            std::printf("    reason=%s\n", stderr_last.c_str());
            std::printf("    stderr_log=%s\n", rep.stderr_log_path.c_str());
        }
    }

    WriteTextFile(out_root / "summary.csv", csv);
    WriteTextFile(out_root / "spv_manifest.csv", spv_manifest);

    std::ostringstream summary;
    summary << "total_cases=" << cases.size() << "\n"
            << "create_fail_count=" << create_fail_count << "\n"
            << "compile_fail_count=" << compile_fail_count << "\n"
            << "mirror_fail_count=" << mirror_fail_count << "\n"
            << "diff_mismatch_count=" << diff_mismatch_count << "\n";

    WriteTextFile(out_root / "summary.txt", summary.str());

    std::printf("[MaterialPresetExhaustiveCompile] done total=%u create_fail=%u compile_fail=%u mirror_fail=%u diff_mismatch=%u\n",
                static_cast<unsigned>(cases.size()),
                create_fail_count,
                compile_fail_count,
                mirror_fail_count,
                diff_mismatch_count);

    CloseShaderCompiler();
    return (create_fail_count == 0 && compile_fail_count == 0 && mirror_fail_count == 0) ? 0 : 1;
}
