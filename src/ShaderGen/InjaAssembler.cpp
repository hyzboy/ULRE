#include <hgl/shadergen/InjaAssembler.h>
#include <hgl/shadergen/CompositorFeatureFlags.h>
#include <hgl/shadergen/ShaderGenPathConfig.h>
#include <hgl/mtl/new/SurfaceType.h>
#include <hgl/mtl/new/BlendMode.h>
#include <hgl/mtl/new/PassType.h>
#include <hgl/mtl/new/MaterialVariantKey.h>   // GeometryMode
#include <hgl/common/VertexAttribDef.h>        // VertexAttrib enum
#include <hgl/common/DescriptorSemantic.h>     // UBO/SSBO semantic enums
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace hgl::graph
{

namespace
{
    // -------------------------------------------------------------------------
    // File I/O helper
    // -------------------------------------------------------------------------
    bool ReadTextFile(const std::string &path, std::string &out)
    {
        std::ifstream ifs(path, std::ios::in);
        if (!ifs.is_open())
            return false;
        std::ostringstream ss;
        ss << ifs.rdbuf();
        out = ss.str();
        return true;
    }

    // -------------------------------------------------------------------------
    // Surface function path lookup  (mirrors CompositorAssembler::GetSurfaceFunctionPath)
    // -------------------------------------------------------------------------
    std::string GetSurfaceFunctionPath(SurfaceType surface)
    {
        if (Is2DSurfaceType(surface))
        {
            switch (surface)
            {
            case SurfaceType::PureColor2D:   return "surface/unlit_color3d_surface.glsl";
            case SurfaceType::VertexColor2D: return "surface/unlit_vertexcolor_surface.glsl";
            case SurfaceType::PureTexture2D: return "surface/2d/puretexture2d_surface.glsl";
            case SurfaceType::Text2D:        return "surface/2d/text2d_surface.glsl";
            default: break;
            }
        }

        switch (surface)
        {
        case SurfaceType::Standard:  return "surface/standard_surface.glsl";
        case SurfaceType::Unlit:     return "surface/unlit_color3d_surface.glsl";
        case SurfaceType::Skin:      return "surface/skin_surface.glsl";
        case SurfaceType::Hair:      return "surface/hair_surface.glsl";
        case SurfaceType::Cloth:     return "surface/cloth_surface.glsl";
        case SurfaceType::Eye:       return "surface/eye_surface.glsl";
        case SurfaceType::Foliage:   return "surface/foliage_surface.glsl";
        case SurfaceType::ClearCoat: return "surface/clearcoat_surface.glsl";
        case SurfaceType::Water:     return "surface/water_surface.glsl";
        case SurfaceType::Terrain:   return "surface/terrain_surface.glsl";
        case SurfaceType::Sky:       return "surface/sky_surface.glsl";
        default:                     return "surface/standard_surface.glsl";
        }
    }

    // -------------------------------------------------------------------------
    // Recursively expand all #include "path" directives into inline content.
    // A per-call visited set prevents re-inlining files already emitted.
    // Only double-quoted includes are expanded; angle-bracket forms are left as-is.
    // -------------------------------------------------------------------------
    std::string FlattenIncludes(const std::string &source,
                                const std::string &base_path,
                                std::unordered_set<std::string> &visited)
    {
        std::string result;
        result.reserve(source.size() * 2);
        std::istringstream iss(source);
        std::string line;
        while (std::getline(iss, line))
        {
            const size_t first = line.find_first_not_of(" \t");
            if (first == std::string::npos || line[first] != '#')
            {
                result += line;
                result += '\n';
                continue;
            }

            const std::string_view sv(line.c_str() + first, line.size() - first);
            if (sv.substr(0, 8) != "#include")
            {
                result += line;
                result += '\n';
                continue;
            }

            const size_t q1 = line.find('"', first + 8);
            const size_t q2 = (q1 != std::string::npos) ? line.find('"', q1 + 1) : std::string::npos;
            if (q1 == std::string::npos || q2 == std::string::npos)
            {
                result += line;
                result += '\n';
                continue;
            }

            const std::string rel = line.substr(q1 + 1, q2 - q1 - 1);
            const std::string abs = base_path + "/" + rel;

            if (!visited.insert(abs).second)
                continue;

            std::string content;
            if (!ReadTextFile(abs, content))
            {
                result += "// [FlattenIncludes: cannot open " + rel + "]\n";
                result += line;
                result += '\n';
                continue;
            }

            result += "// ---- " + rel + " ----\n";
            result += FlattenIncludes(content, base_path, visited);
            result += "// ---- end " + rel + " ----\n";
        }
        return result;
    }

    std::string FlattenIncludes(const std::string &source, const std::string &base_path)
    {
        std::unordered_set<std::string> visited;
        return FlattenIncludes(source, base_path, visited);
    }

    // -------------------------------------------------------------------------
    // Build the #define block from all active flags.
    // C++ owns the boolean logic; templates receive the pre-built string.
    // -------------------------------------------------------------------------
    std::string BuildDefinesBlock(const CompositorFeatureFlags &f)
    {
        std::string s;
        if (f.vert_input_2d)    s += "#define VERT_INPUT_2D\n";
        if (f.has_uv0)          s += "#define HAS_UV0\n";
        if (f.has_vertex_color) s += "#define HAS_VERTEX_COLOR\n";
        if (f.has_world_pos)    s += "#define HAS_WORLD_POS\n";
        if (f.has_world_normal) s += "#define HAS_WORLD_NORMAL\n";
        if (f.has_luminance)    s += "#define HAS_LUMINANCE\n";
        if (f.has_direction)    s += "#define HAS_DIRECTION\n";
        if (f.enable_lighting)  s += "#define ENABLE_LIGHTING\n";
        if (f.needs_camera)     s += "#define NEEDS_CAMERA\n";
        if (f.needs_sky)        s += "#define NEEDS_SKY\n";
        if (f.alpha_masked)     s += "#define ALPHA_MODE_MASKED\n";
        if (f.alpha_dither)     s += "#define ALPHA_MODE_DITHER\n";
        if (f.has_texcoord)     s += "#define HAS_TEXCOORD\n";
        if (f.has_clip_pos)     s += "#define HAS_CLIP_POS\n";
        return s;
    }

    // -------------------------------------------------------------------------
    // Build inja data JSON from MaterialDef
    // -------------------------------------------------------------------------
    nlohmann::json BuildInjaData(const mtl::MaterialDef &def)
    {
        using mtl::UBODescriptorSemantic;
        using mtl::SSBODescriptorSemantic;
        using mtl::GeometryMode;

        CompositorFeatureFlags flags;

        // ── vertex attrib → feature flags ────────────────────────────────
        for (const auto &entry : def.vertex_entries)
        {
            switch (entry.attrib)
            {
            case VertexAttrib::Normal:
                flags.has_world_normal = true;
                break;
            case VertexAttrib::Color:
                flags.has_vertex_color = true;
                break;
            case VertexAttrib::Luminance:
                flags.has_luminance = true;
                break;
            case VertexAttrib::TexCoord:
                flags.has_uv0     = true;
                flags.has_texcoord = true;
                break;
            default:
                break;
            }
        }

        // ── SSBO descriptors ──────────────────────────────────────────────
        for (auto sem : def.ssbo_descriptors)
        {
            if (sem == SSBODescriptorSemantic::TransformData)
                flags.has_world_pos = true;
        }

        // has_world_normal is only valid when there's a world-space transform
        flags.has_world_normal = flags.has_world_normal && flags.has_world_pos;

        // ── UBO descriptors ───────────────────────────────────────────────
        for (auto sem : def.ubo_descriptors)
        {
            if (sem == UBODescriptorSemantic::CameraInfo) flags.needs_camera = true;
            if (sem == UBODescriptorSemantic::SkyInfo)    flags.needs_sky    = true;
        }

        // ── blend mode ────────────────────────────────────────────────────
        flags.alpha_masked = (def.blend_mode == BlendMode::Masked);
        flags.alpha_dither = (def.blend_mode == BlendMode::Dither);

        // ── geometry / surface mode ───────────────────────────────────────
        flags.vert_input_2d = Is2DSurfaceType(def.surface_type)
                           || def.geometry_mode == GeometryMode::Quad2D
                           || def.geometry_mode == GeometryMode::ScreenRect;

        flags.enable_lighting = !flags.vert_input_2d
                             && (def.surface_type != SurfaceType::Unlit);

        // ── explicit bool_features override ──────────────────────────────
        for (const auto &[key, val] : def.bool_features)
        {
            if      (key == "vert_input_2d")    flags.vert_input_2d    = val;
            else if (key == "has_uv0")          flags.has_uv0          = val;
            else if (key == "has_vertex_color") flags.has_vertex_color = val;
            else if (key == "has_world_pos")    flags.has_world_pos    = val;
            else if (key == "has_world_normal") flags.has_world_normal = val;
            else if (key == "has_luminance")    flags.has_luminance    = val;
            else if (key == "has_direction")    flags.has_direction    = val;
            else if (key == "enable_lighting")  flags.enable_lighting  = val;
            else if (key == "needs_camera")     flags.needs_camera     = val;
            else if (key == "needs_sky")        flags.needs_sky        = val;
            else if (key == "alpha_masked")     flags.alpha_masked     = val;
            else if (key == "alpha_dither")     flags.alpha_dither     = val;
            else if (key == "has_texcoord")     flags.has_texcoord     = val;
            else if (key == "has_clip_pos")     flags.has_clip_pos     = val;
        }

        flags.surface_path = GetSurfaceFunctionPath(def.surface_type);

        // ── build JSON ────────────────────────────────────────────────────
        nlohmann::json data;

        // C++ builds both blocks; templates simply output {{ defines }} / {{ extensions }}.
        data["defines"]    = BuildDefinesBlock(flags);
        data["extensions"] = std::string{};

        // Booleans still used by template conditionals (conditional #includes in frag):
        data["needs_sky"]       = flags.needs_sky;
        data["enable_lighting"] = flags.enable_lighting;

        // String variables for include paths
        data["surface_path"]           = flags.surface_path;
        data["lighting_function_file"] = mtl::GetLightingModelGLSLPath(flags.lighting_model);
        data["skylight_function_file"] = std::string("common/skylight_simple.glsl");

        // Pass through extra features from the .mat file
        // (done last so .mat string_features can override the defaults above)
        for (const auto &[k, v] : def.string_features)
            data[k] = v;
        for (const auto &[k, v] : def.int_features)
            data[k] = v;

        return data;
    }

    // -------------------------------------------------------------------------
    // Inject SPK #defines immediately after the first #version line
    // -------------------------------------------------------------------------
    std::string InjectSPKDefines(const std::string &source, const ShaderPermutationKey &spk)
    {
        std::string defines;
        spk.AppendGLSLDefines(defines);
        if (defines.empty())
            return source;

        auto pos = source.find('\n');
        if (pos != std::string::npos && source.size() >= 8
            && source.substr(0, 8) == "#version")
        {
            std::string result;
            result.reserve(source.size() + defines.size() + 2);
            result.append(source, 0, pos + 1);
            result += '\n';
            result += defines;
            result += '\n';
            result.append(source, pos + 1, std::string::npos);
            return result;
        }

        return defines + "\n" + source;
    }

} // anonymous namespace

// =============================================================================
// InjaAssembler implementation
// =============================================================================

InjaAssembler::InjaAssembler()
    : InjaAssembler(GetShaderLibraryPath())
{}

InjaAssembler::InjaAssembler(const std::string &shader_library_path)
    : shader_lib_path_(shader_library_path)
{}

bool InjaAssembler::CanAssemble(const mtl::MaterialDef &def) const
{
    return !def.vs_template.empty() && !def.fs_template.empty();
}

InjaAssembler::AssembleResult InjaAssembler::Assemble(
    const mtl::MaterialDef     &def,
    const ShaderPermutationKey &spk) const
{
    AssembleResult result;

    if (!CanAssemble(def))
    {
        result.error = "MaterialDef '" + def.name
                     + "': vs_template / fs_template not set.";
        return result;
    }

    const std::string vs_path = shader_lib_path_ + "/" + def.vs_template;
    const std::string fs_path = shader_lib_path_ + "/" + def.fs_template;

    std::string vs_src, fs_src;
    if (!ReadTextFile(vs_path, vs_src))
    {
        result.error = "Cannot open VS template: " + vs_path;
        return result;
    }
    if (!ReadTextFile(fs_path, fs_src))
    {
        result.error = "Cannot open FS template: " + fs_path;
        return result;
    }

    const nlohmann::json data = BuildInjaData(def);

    try
    {
        inja::Environment env;
        result.vertex_glsl   = env.render(vs_src, data);
        result.fragment_glsl = env.render(fs_src, data);
    }
    catch (const std::exception &ex)
    {
        result.error = std::string("inja render error: ") + ex.what();
        return result;
    }

    // Inject SPK binding-slot #defines after #version
    result.vertex_glsl   = InjectSPKDefines(result.vertex_glsl,   spk);
    result.fragment_glsl = InjectSPKDefines(result.fragment_glsl, spk);

    // Expand all #include directives into inline content
    result.vertex_glsl   = FlattenIncludes(result.vertex_glsl,   shader_lib_path_);
    result.fragment_glsl = FlattenIncludes(result.fragment_glsl, shader_lib_path_);

    result.success = true;
    return result;
}

} // namespace hgl::graph
