/// test_MaterialShaderGen.cpp
///
/// 对所有硬编码材质的所有配置变体生成 GLSL 源码，保存到文件，
/// 然后（如果 glslangValidator 或 glslc 在 PATH 中可用）编译验证。
/// 编译失败时保存错误信息到 .err 文件。
///
/// 用法：
///   test_MaterialShaderGen [output_dir]
///
/// 输出目录默认为 ./shader_gen_output/
///
/// 对每个 (材质, 配置变体) 产生：
///   <output_dir>/<材质名>/<变体描述>/vert.glsl      — VS 源码
///   <output_dir>/<材质名>/<变体描述>/frag.glsl      — FS 源码
///   <output_dir>/<材质名>/<变体描述>/geom.glsl      — GS 源码（如有）
///   <output_dir>/<材质名>/<变体描述>/vert.err       — VS 编译错误（如有）
///   <output_dir>/<材质名>/<变体描述>/frag.err       — FS 编译错误（如有）
///   <output_dir>/<材质名>/<变体描述>/reflect.txt    — SPIR-V Reflect 摘要（如有）
///   <output_dir>/summary.txt                        — 总体报告

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <hgl/graph/mtl/StdMaterial.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/graph/mtl/Material2DCreateConfig.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/graph/data/CoordinateSystem.h>

namespace fs = std::filesystem;
using namespace hgl::graph;
using namespace hgl::graph::mtl;

// ─────────────────────────────────────────────────────────────────────────────
// Filesystem helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool WriteFile(const fs::path &path, const std::string &content)
{
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f) return false;
    f << content;
    return f.good();
}

// ─────────────────────────────────────────────────────────────────────────────
// Compiler detection & invocation
// ─────────────────────────────────────────────────────────────────────────────

static std::string s_compiler;   // filled in by FindGLSLCompiler()

static void FindGLSLCompiler()
{
    // Try glslangValidator first, then glslc (shaderc)
    for (const char *candidate : {"glslangValidator", "glslc"})
    {
#if defined(_WIN32)
        std::string cmd = std::string("where ") + candidate + " >nul 2>&1";
#else
        std::string cmd = std::string("which ") + candidate + " >/dev/null 2>&1";
#endif
        if (std::system(cmd.c_str()) == 0)
        {
            s_compiler = candidate;
            break;
        }
    }
}

static std::string StageExtension(const std::string &stage)
{
    if (stage == "vert") return ".vert";
    if (stage == "frag") return ".frag";
    if (stage == "geom") return ".geom";
    return ".glsl";
}

/// Compile a GLSL file; returns (ok, error_text)
static std::pair<bool, std::string> CompileGLSL(
    const fs::path &glsl_path,
    const std::string &stage,
    const fs::path &spv_path)
{
    if (s_compiler.empty())
        return {false, "(no compiler found)"};

    std::string cmd;
    // Build base command (no redirect yet)
    if (s_compiler == "glslangValidator")
    {
        cmd = "glslangValidator -V --target-env vulkan1.0 -S " + stage
            + " \"" + glsl_path.string() + "\""
            + " -o \"" + spv_path.string() + "\"";
    }
    else  // glslc
    {
        cmd = "glslc -fshader-stage=" + stage
            + " \"" + glsl_path.string() + "\""
            + " -o \"" + spv_path.string() + "\"";
    }

    // Redirect both stdout and stderr to a temp file for capture
    fs::path tmp = spv_path.parent_path() / (spv_path.filename().string() + ".compile_out.tmp");
    cmd += " > \"" + tmp.string() + "\" 2>&1";

    int rc = std::system(cmd.c_str());

    std::string output;
    {
        std::ifstream f(tmp);
        if (f)
        {
            std::ostringstream ss;
            ss << f.rdbuf();
            output = ss.str();
        }
    }
    fs::remove(tmp);

    return {rc == 0, output};
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-shader result
// ─────────────────────────────────────────────────────────────────────────────

struct ShaderResult
{
    std::string stage;       // "vert" / "frag" / "geom"
    bool        glsl_ok;
    bool        compiled;    // false if compiler unavailable or not attempted
    bool        compile_ok;
    std::string compile_error;
};

// ─────────────────────────────────────────────────────────────────────────────
// Generate & optionally compile one material variant
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<ShaderResult> GenerateVariant(
    const std::string &variant_label,
    MaterialCreateInfo *mci,
    const fs::path &out_dir)
{
    std::vector<ShaderResult> results;

    struct StageInfo { const ShaderCreateInfo *sc; std::string stage; };
    std::vector<StageInfo> stages;
    if (mci->GetVS()) stages.push_back({mci->GetVS(), "vert"});
    if (mci->GetGS()) stages.push_back({mci->GetGS(), "geom"});
    if (mci->GetFS()) stages.push_back({mci->GetFS(), "frag"});

    for (const auto &si : stages)
    {
        ShaderResult r;
        r.stage = si.stage;
        r.compiled = false;
        r.compile_ok = false;

        const std::string &src = si.sc->GetShaderSource();
        r.glsl_ok = !src.empty();

        if (r.glsl_ok)
        {
            fs::path glsl_path = out_dir / (si.stage + ".glsl");
            WriteFile(glsl_path, src);

            if (!s_compiler.empty())
            {
                r.compiled = true;
                fs::path spv_path = out_dir / (si.stage + ".spv");
                auto [ok, err] = CompileGLSL(glsl_path, si.stage, spv_path);
                r.compile_ok    = ok;
                r.compile_error = err;

                if (!ok && !err.empty())
                    WriteFile(out_dir / (si.stage + ".err"), err);
            }
        }

        results.push_back(r);
    }

    return results;
}

// ─────────────────────────────────────────────────────────────────────────────
// Summary accumulator
// ─────────────────────────────────────────────────────────────────────────────

struct Summary
{
    int total_variants  = 0;
    int glsl_ok         = 0;
    int glsl_fail       = 0;
    int compile_ok      = 0;
    int compile_fail    = 0;
    int compile_skip    = 0;

    std::vector<std::string> failures;

    void Add(const std::string &label, const std::vector<ShaderResult> &results)
    {
        ++total_variants;
        bool variant_glsl_ok = true;
        bool variant_compile_ok = true;
        bool any_compiled = false;

        for (const auto &r : results)
        {
            if (!r.glsl_ok)
            {
                variant_glsl_ok = false;
                failures.push_back(label + "/" + r.stage + ": GLSL generation failed");
            }
            if (r.compiled)
            {
                any_compiled = true;
                if (!r.compile_ok)
                {
                    variant_compile_ok = false;
                    failures.push_back(label + "/" + r.stage + ": compile failed\n    " + r.compile_error);
                }
            }
        }

        if (variant_glsl_ok)   ++glsl_ok;   else ++glsl_fail;
        if (!any_compiled)     ++compile_skip;
        else if (variant_compile_ok) ++compile_ok; else ++compile_fail;
    }

    void Print() const
    {
        printf("\n%s\n", std::string(80, '=').c_str());
        printf("  Test Summary — test_MaterialShaderGen\n");
        printf("%s\n", std::string(80, '=').c_str());
        printf("  Total variants  : %d\n", total_variants);
        printf("  GLSL gen OK     : %d\n", glsl_ok);
        printf("  GLSL gen FAIL   : %d\n", glsl_fail);
        if (compile_skip > 0)
            printf("  Compile (skipped): %d  (no compiler on PATH)\n", compile_skip);
        else
        {
            printf("  Compile OK      : %d\n", compile_ok);
            printf("  Compile FAIL    : %d\n", compile_fail);
        }

        if (!failures.empty())
        {
            printf("\n  Failures:\n");
            for (const auto &f : failures)
                printf("    [FAIL] %s\n", f.c_str());
        }

        const bool ok = (glsl_fail == 0) && (compile_fail == 0);
        printf("\n  Overall: %s\n\n", ok ? "✓ ALL PASSED" : "✗ SOME FAILED");
    }

    std::string ToText() const
    {
        std::ostringstream ss;
        ss << "total_variants=" << total_variants << "\n"
           << "glsl_ok=" << glsl_ok << "\n"
           << "glsl_fail=" << glsl_fail << "\n"
           << "compile_ok=" << compile_ok << "\n"
           << "compile_fail=" << compile_fail << "\n"
           << "compile_skip=" << compile_skip << "\n";
        for (const auto &f : failures)
            ss << "FAIL: " << f << "\n";
        return ss.str();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Helper: run one (config → material) entry
// ─────────────────────────────────────────────────────────────────────────────

using CreateFn2D = MaterialCreateInfo *(*)(const VulkanDevAttr *, Material2DCreateConfig *);
using CreateFn3D = MaterialCreateInfo *(*)(const VulkanDevAttr *, Material3DCreateConfig *);

static void Run2D(
    Summary &summary,
    const fs::path &base_out,
    const char *mtl_name,
    CreateFn2D create_fn,
    const std::vector<std::pair<std::string, Material2DCreateConfig>> &configs)
{
    printf("\n[2D] %s\n", mtl_name);

    for (const auto &[label, cfg] : configs)
    {
        std::string full_label = std::string(mtl_name) + "/" + label;
        printf("  variant: %s\n", label.c_str());

        Material2DCreateConfig cfg_copy = cfg;
        MaterialCreateInfo *mci = create_fn(nullptr, &cfg_copy);
        if (!mci)
        {
            printf("    [✗] create returned nullptr\n");
            summary.failures.push_back(full_label + ": create returned nullptr");
            ++summary.total_variants;
            ++summary.glsl_fail;
            continue;
        }

        fs::path out_dir = base_out / mtl_name / label;
        auto results = GenerateVariant(full_label, mci, out_dir);
        summary.Add(full_label, results);

        for (const auto &r : results)
        {
            if (r.glsl_ok && !r.compiled)
                printf("    [✓] %s.glsl generated\n", r.stage.c_str());
            else if (r.glsl_ok && r.compile_ok)
                printf("    [✓] %s OK\n", r.stage.c_str());
            else if (r.glsl_ok && r.compiled && !r.compile_ok)
                printf("    [✗] %s COMPILE FAIL — see %s\n",
                       r.stage.c_str(), (out_dir / (r.stage + ".err")).string().c_str());
            else
                printf("    [✗] %s GLSL FAIL\n", r.stage.c_str());
        }

        delete mci;
    }
}

static void Run3D(
    Summary &summary,
    const fs::path &base_out,
    const char *mtl_name,
    CreateFn3D create_fn,
    const std::vector<std::pair<std::string, Material3DCreateConfig>> &configs)
{
    printf("\n[3D] %s\n", mtl_name);

    for (const auto &[label, cfg] : configs)
    {
        std::string full_label = std::string(mtl_name) + "/" + label;
        printf("  variant: %s\n", label.c_str());

        Material3DCreateConfig cfg_copy = cfg;
        MaterialCreateInfo *mci = create_fn(nullptr, &cfg_copy);
        if (!mci)
        {
            printf("    [✗] create returned nullptr\n");
            summary.failures.push_back(full_label + ": create returned nullptr");
            ++summary.total_variants;
            ++summary.glsl_fail;
            continue;
        }

        fs::path out_dir = base_out / mtl_name / label;
        auto results = GenerateVariant(full_label, mci, out_dir);
        summary.Add(full_label, results);

        for (const auto &r : results)
        {
            if (r.glsl_ok && !r.compiled)
                printf("    [✓] %s.glsl generated\n", r.stage.c_str());
            else if (r.glsl_ok && r.compile_ok)
                printf("    [✓] %s OK\n", r.stage.c_str());
            else if (r.glsl_ok && r.compiled && !r.compile_ok)
                printf("    [✗] %s COMPILE FAIL — see %s\n",
                       r.stage.c_str(), (out_dir / (r.stage + ".err")).string().c_str());
            else
                printf("    [✗] %s GLSL FAIL\n", r.stage.c_str());
        }

        delete mci;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    const fs::path base_out = (argc > 1) ? fs::path(argv[1]) : fs::path("shader_gen_output");

    printf("Material Shader Generator — Full Variant Test\n");
    printf("==============================================\n");
    printf("Output dir: %s\n", base_out.string().c_str());

    FindGLSLCompiler();
    if (!s_compiler.empty())
        printf("GLSL compiler: %s\n", s_compiler.c_str());
    else
        printf("GLSL compiler: (none found — compile step skipped, GLSL files only)\n");

    Summary summary;

    // ─── 2D materials ────────────────────────────────────────────────────────

    // Coordinate system × L2W combinations for generic 2D materials
    std::vector<std::pair<std::string, Material2DCreateConfig>> cfg2d_generic;
    for (auto cs : {CoordinateSystem2D::NDC, CoordinateSystem2D::ZeroToOne, CoordinateSystem2D::Ortho})
    {
        for (auto l2w : {WithLocalToWorld::Without, WithLocalToWorld::With})
        {
            std::string label = std::string(GetCoordinateSystem2DName(cs))
                              + (l2w == WithLocalToWorld::With ? "_L2W" : "_noL2W");
            cfg2d_generic.push_back({label, Material2DCreateConfig(PrimitiveType::Lines, cs, l2w)});
        }
    }

    Run2D(summary, base_out, "VertexColor2D",
          (CreateFn2D)CreateVertexColor2D,    cfg2d_generic);

    Run2D(summary, base_out, "PureColor2D",
          CreatePureColor2D,                  cfg2d_generic);

    Run2D(summary, base_out, "PureTexture2D",
          (CreateFn2D)CreatePureTexture2D,    cfg2d_generic);

    // RectTexture2D uses position_format=VAT_VEC4 for Rect primitives
    {
        std::vector<std::pair<std::string, Material2DCreateConfig>> cfg_rect;
        for (auto cs : {CoordinateSystem2D::NDC, CoordinateSystem2D::Ortho})
        {
            Material2DCreateConfig c(PrimitiveType::SolidRectangles, cs, WithLocalToWorld::Without);
            cfg_rect.push_back({std::string("SolidRect_") + GetCoordinateSystem2DName(cs), c});
        }
        Run2D(summary, base_out, "RectTexture2D",      CreateRectTexture2D,      cfg_rect);
        Run2D(summary, base_out, "RectTexture2DArray", CreateRectTexture2DArray, cfg_rect);
    }

    // Text2D has a fixed config (geometry shader + instanced rect layout)
    {
        printf("\n[2D] Text2D\n");
        printf("  variant: default\n");
        Text2DMaterialCreateConfig tc;
        MaterialCreateInfo *mci = CreateText2D(nullptr, &tc);
        if (!mci)
        {
            printf("    [✗] create returned nullptr\n");
            summary.failures.push_back("Text2D/default: create returned nullptr");
            ++summary.total_variants;
            ++summary.glsl_fail;
        }
        else
        {
            fs::path out_dir = base_out / "Text2D" / "default";
            auto results = GenerateVariant("Text2D/default", mci, out_dir);
            summary.Add("Text2D/default", results);
            for (const auto &r : results)
            {
                if (r.glsl_ok && !r.compiled)
                    printf("    [✓] %s.glsl generated\n", r.stage.c_str());
                else if (r.glsl_ok && r.compile_ok)
                    printf("    [✓] %s OK\n", r.stage.c_str());
                else if (r.glsl_ok && r.compiled && !r.compile_ok)
                    printf("    [✗] %s COMPILE FAIL — see %s\n",
                           r.stage.c_str(), (out_dir / (r.stage + ".err")).string().c_str());
                else
                    printf("    [✗] %s GLSL FAIL\n", r.stage.c_str());
            }
            delete mci;
        }
    }

    // ─── 3D materials ────────────────────────────────────────────────────────

    // camera × sky combinations
    std::vector<std::pair<std::string, Material3DCreateConfig>> cfg3d_simple;
    for (auto cam : {WithCamera::Without, WithCamera::With})
    {
        std::string label = (cam == WithCamera::With) ? "cam" : "nocam";
        cfg3d_simple.push_back({label, Material3DCreateConfig(PrimitiveType::Triangles, cam)});
    }

    std::vector<std::pair<std::string, Material3DCreateConfig>> cfg3d_sky;
    for (auto cam : {WithCamera::Without, WithCamera::With})
    {
        for (auto sky : {WithSky::Without, WithSky::With})
        {
            std::string label = (cam == WithCamera::With ? "cam" : "nocam")
                              + std::string("_")
                              + (sky == WithSky::With ? "sky" : "nosky");
            cfg3d_sky.push_back({label, Material3DCreateConfig(PrimitiveType::Triangles, cam,
                                                                WithLocalToWorld::With, sky)});
        }
    }

    Run3D(summary, base_out, "PureColor3D",
          CreatePureColor3D,         cfg3d_simple);

    Run3D(summary, base_out, "VertexColor3D",
          (CreateFn3D)CreateVertexColor3D,     cfg3d_simple);

    Run3D(summary, base_out, "VertexLuminance3D",
          CreateVertexLuminance3D,   cfg3d_simple);

    Run3D(summary, base_out, "VertexPattleColor3D",
          (CreateFn3D)CreateVertexPattleColor3D, cfg3d_simple);

    Run3D(summary, base_out, "Gizmo3D",
          CreateGizmo3D,             cfg3d_simple);

    Run3D(summary, base_out, "TextureBlinnPhong",
          (CreateFn3D)CreateTextureBlinnPhong, cfg3d_sky);

    // TerrainGrid has fixed config (cam+sky+l2w)
    {
        printf("\n[3D] TerrainGrid\n");
        printf("  variant: default\n");
        TerrainGridCreateConfig tc;
        MaterialCreateInfo *mci = CreateTerrainGrid(nullptr, &tc);
        if (!mci)
        {
            printf("    [✗] create returned nullptr\n");
            summary.failures.push_back("TerrainGrid/default: create returned nullptr");
            ++summary.total_variants; ++summary.glsl_fail;
        }
        else
        {
            fs::path out_dir = base_out / "TerrainGrid" / "default";
            auto results = GenerateVariant("TerrainGrid/default", mci, out_dir);
            summary.Add("TerrainGrid/default", results);
            for (const auto &r : results)
                printf("    [%s] %s\n", r.glsl_ok ? "✓" : "✗", r.stage.c_str());
            delete mci;
        }
    }

    // SkyMinimal — camera variants
    {
        std::vector<std::pair<std::string, SkyMinimalCreateConfig>> sky_cfgs;
        sky_cfgs.push_back({"cam",   SkyMinimalCreateConfig(WithCamera::With)});
        sky_cfgs.push_back({"nocam", SkyMinimalCreateConfig(WithCamera::Without)});
        printf("\n[3D] SkyMinimal\n");
        for (const auto &[label, cfg] : sky_cfgs)
        {
            printf("  variant: %s\n", label.c_str());
            SkyMinimalCreateConfig cfg_copy = cfg;
            MaterialCreateInfo *mci = CreateSkyMinimal(nullptr, &cfg_copy);
            std::string full_label = "SkyMinimal/" + label;
            if (!mci)
            {
                printf("    [✗] create returned nullptr\n");
                summary.failures.push_back(full_label + ": create returned nullptr");
                ++summary.total_variants; ++summary.glsl_fail;
            }
            else
            {
                fs::path out_dir = base_out / "SkyMinimal" / label;
                auto results = GenerateVariant(full_label, mci, out_dir);
                summary.Add(full_label, results);
                for (const auto &r : results)
                    printf("    [%s] %s\n", r.glsl_ok ? "✓" : "✗", r.stage.c_str());
                delete mci;
            }
        }
    }

    // BasicLit — with/without IBL
    {
        std::vector<std::pair<std::string, BasicLitMaterialCreateConfig>> bl_cfgs;
        bl_cfgs.push_back({"no_ibl", BasicLitMaterialCreateConfig(false)});
        bl_cfgs.push_back({"ibl",    BasicLitMaterialCreateConfig(true)});
        printf("\n[3D] BasicLit\n");
        for (const auto &[label, cfg] : bl_cfgs)
        {
            printf("  variant: %s\n", label.c_str());
            BasicLitMaterialCreateConfig cfg_copy = cfg;
            MaterialCreateInfo *mci = CreateBasicLit(nullptr, &cfg_copy);
            std::string full_label = "BasicLit/" + label;
            if (!mci)
            {
                printf("    [✗] create returned nullptr\n");
                summary.failures.push_back(full_label + ": create returned nullptr");
                ++summary.total_variants; ++summary.glsl_fail;
            }
            else
            {
                fs::path out_dir = base_out / "BasicLit" / label;
                auto results = GenerateVariant(full_label, mci, out_dir);
                summary.Add(full_label, results);
                for (const auto &r : results)
                    printf("    [%s] %s\n", r.glsl_ok ? "✓" : "✗", r.stage.c_str());
                delete mci;
            }
        }
    }

    // Billboard2D — fixed_size × pixel_size (just two variants)
    {
        std::vector<std::pair<std::string, BillboardMaterialCreateConfig>> bb_cfgs;
        {
            BillboardMaterialCreateConfig c;
            c.fixed_size = false;
            bb_cfgs.push_back({"dynamic", c});
        }
        {
            BillboardMaterialCreateConfig c;
            c.fixed_size = true;
            c.pixel_size = {32, 32};
            bb_cfgs.push_back({"fixed32x32", c});
        }
        printf("\n[3D] Billboard2D\n");
        for (const auto &[label, cfg] : bb_cfgs)
        {
            printf("  variant: %s\n", label.c_str());
            BillboardMaterialCreateConfig cfg_copy = cfg;
            MaterialCreateInfo *mci = CreateBillboard2D(nullptr, &cfg_copy);
            std::string full_label = "Billboard2D/" + label;
            if (!mci)
            {
                printf("    [✗] create returned nullptr\n");
                summary.failures.push_back(full_label + ": create returned nullptr");
                ++summary.total_variants; ++summary.glsl_fail;
            }
            else
            {
                fs::path out_dir = base_out / "Billboard2D" / label;
                auto results = GenerateVariant(full_label, mci, out_dir);
                summary.Add(full_label, results);
                for (const auto &r : results)
                    printf("    [%s] %s\n", r.glsl_ok ? "✓" : "✗", r.stage.c_str());
                delete mci;
            }
        }
    }

    // ─── Print & save summary ─────────────────────────────────────────────────

    summary.Print();
    WriteFile(base_out / "summary.txt", summary.ToText());

    printf("Saved output to: %s\n\n", base_out.string().c_str());

    // Return non-zero exit code if any GLSL generation or compile step failed
    return (summary.glsl_fail > 0 || summary.compile_fail > 0) ? 1 : 0;
}
