/// MaterialPresetExhaustiveCompile — exhaustively compile every MaterialPreset and dump GLSL.
///
/// Usage:
///   MaterialPresetExhaustiveCompile.exe [dump_dir]
///
/// If dump_dir is given, VS/FS GLSL is written to that directory as
///   <PresetName>.vert.glsl  /  <PresetName>.frag.glsl
///
/// Exit code = number of failed presets (0 means all passed).

#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/mtl/MaterialCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/MaterialPreset.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfo.h>
#include <hgl/shadergen/ShaderGenPathConfig.h>
#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <hgl/common/ShaderStageDef.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

using namespace hgl::graph;
using namespace hgl::graph::mtl;

// GLSLCompiler init/close (from GLSLCompiler.h — internal to ShaderGen)
namespace hgl::graph
{
    bool InitShaderCompiler();
    void CloseShaderCompiler();
}

namespace
{

bool WriteTextFile(const std::filesystem::path &path, const std::string &content)
{
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs)
        return false;
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    return ofs.good();
}

/// Build an appropriate MaterialCreateConfig subclass for the given preset.
/// The returned pointer is heap-allocated; caller must delete.
MaterialCreateConfig *MakeConfigForPreset(MaterialPreset preset)
{
    switch (preset)
    {
        // --- 2D presets (Material2DCreateConfig) ---
        case MaterialPreset::VertexColor2D:
        case MaterialPreset::PureColor2D:
        case MaterialPreset::PureTexture2D:
            return new Material2DCreateConfig(PrimitiveType::Triangles);

        case MaterialPreset::Text2D:
            return new Text2DMaterialCreateConfig();

        // --- Specialized 3D presets ---
        case MaterialPreset::TerrainGrid:
            return new TerrainGridCreateConfig();

        case MaterialPreset::SkyMinimal:
            return new SkyMinimalCreateConfig();

        case MaterialPreset::Billboard2DDynamic:
        {
            auto *cfg = new BillboardMaterialCreateConfig();
            cfg->fixed_size = false;
            return cfg;
        }

        case MaterialPreset::Billboard2DFixed:
        {
            auto *cfg = new BillboardMaterialCreateConfig();
            cfg->fixed_size = true;
            return cfg;
        }

        case MaterialPreset::PBRColor3D:
            return new PBRColor3DMaterialCreateConfig();

        // --- Generic 3D presets (Material3DCreateConfig) ---
        default:
            return new Material3DCreateConfig(PrimitiveType::Triangles);
    }
}

} // anonymous namespace

int main(int argc, char *argv[])
{
    std::filesystem::path dump_dir;

    if (argc >= 2)
    {
        dump_dir = argv[1];
        std::error_code ec;
        std::filesystem::create_directories(dump_dir, ec);
        if (ec)
        {
            std::fprintf(stderr, "[ExhaustiveCompile] cannot create dump dir '%s': %s\n",
                         argv[1], ec.message().c_str());
            return 255;
        }
    }

    // Initialize the GLSLCompiler plugin (loads GLSLCompiler.dll from CWD)
    if (!InitShaderCompiler())
    {
        std::fprintf(stderr, "[ExhaustiveCompile] WARNING: GLSLCompiler plugin unavailable — "
                             "GLSL assembly will still be attempted but SPV compilation will fail.\n");
    }

    // Minimal dummy profile — the 2D factories reject null profile.
    contract::PhysicalDeviceProfileLite dummy_profile{};

    int total   = 0;
    int passed  = 0;
    int failed  = 0;

    for (uint8_t i = static_cast<uint8_t>(MaterialPreset::BEGIN_RANGE);
         i <= static_cast<uint8_t>(MaterialPreset::END_RANGE);
         ++i)
    {
        const auto preset = static_cast<MaterialPreset>(i);
        const char *name  = GetMaterialPresetName(preset);
        if (!name)
            name = "???";

        ++total;

        MaterialCreateConfig *cfg = MakeConfigForPreset(preset);

        MaterialCreateInfo *mci = CreateMaterialCreateInfo(&dummy_profile, preset, cfg);

        delete cfg;

        if (!mci)
        {
            std::fprintf(stderr, "[FAIL] %s\n", name);
            ++failed;
            continue;
        }

        std::fprintf(stderr, "[PASS] %s\n", name);
        ++passed;

        // Dump GLSL if requested
        if (!dump_dir.empty())
        {
            const ShaderCreateInfo *vs = mci->GetStageShader(ShaderStage::Vertex);
            const ShaderCreateInfo *fs = mci->GetStageShader(ShaderStage::Fragment);

            if (vs && !vs->GetFinalGLSL().empty())
                WriteTextFile(dump_dir / (std::string(name) + ".vert.glsl"), vs->GetFinalGLSL());

            if (fs && !fs->GetFinalGLSL().empty())
                WriteTextFile(dump_dir / (std::string(name) + ".frag.glsl"), fs->GetFinalGLSL());
        }

        delete mci;
    }

    CloseShaderCompiler();

    std::fprintf(stderr, "\n=== ExhaustiveCompile: %d/%d passed, %d failed ===\n",
                 passed, total, failed);

    return failed;
}
