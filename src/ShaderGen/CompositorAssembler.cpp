#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/ShaderWriter.h>
#include <hgl/shadergen/ShaderLibraryPath.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/LightingModel.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace
{
    // -----------------------------------------------------------------------
    // StageManifest: data-driven replacement for 16 hand-written builder fns
    // -----------------------------------------------------------------------

    struct SurfaceFunctionRoute
    {
        hgl::graph::SurfaceType surface;
        const char             *path;
    };

    /// Describes a (vertex attrib → #define) conditional mapping.
    struct AttribDefineMapping
    {
        hgl::graph::VertexAttrib attrib;
        const char              *define_name;
    };

    /// Declarative description of a generated VS or FS preamble.
    /// Replaces the per-entry builder functions that used to live here.
    struct StageManifest
    {
        const char                       *template_path         = nullptr;
        std::vector<const char*>          always_defines;          ///< Emitted unconditionally
        std::vector<AttribDefineMapping>  attrib_defines;          ///< Emitted when mesh supplies the attrib
        bool                              emit_alpha_mode        = false; ///< FS: emits ALPHA_MODE_MASKED/DITHER
        std::vector<const char*>          pre_special_includes;   ///< Includes before sky/lighting/surface
        bool                              uses_vert_forward_main = false;  ///< VS: emit ULRE_Decode* helpers for Normal/Tangent/Color
        bool                              needs_sky              = false; ///< FS: #include SKYLIGHT_FUNCTION_FILE
        bool                              needs_lighting         = false; ///< FS: #include LIGHTING_FUNCTION_FILE
        bool                              needs_surface          = false; ///< FS: #include surface_path
        std::vector<const char*>          post_special_includes; ///< Includes after sky/lighting/surface
    };

    // -----------------------------------------------------------------------
    // Surface requirement routing (no fallback chain / no ultimate fallback)
    // -----------------------------------------------------------------------

    struct SurfaceRequirementPlan
    {
        hgl::graph::SurfaceType               surface;
        std::vector<hgl::graph::VertexAttrib> required_attribs;
        std::string                           diagnostic_surface;
    };

    static const SurfaceRequirementPlan kSurfaceRequirementPlans[] =
    {
        {
            hgl::graph::SurfaceType::Standard,
            {
                hgl::graph::VertexAttrib::Position,
                hgl::graph::VertexAttrib::Normal,
            },
            "diagnostic/missing.surface.glsl",
        },
    };

    static bool TryParseSurfaceType(const std::string &name, hgl::graph::SurfaceType &out)
    {
        for (uint8_t i = 0; i < static_cast<uint8_t>(hgl::graph::SurfaceType::RANGE_SIZE); ++i)
        {
            const auto st = static_cast<hgl::graph::SurfaceType>(i);
            const char *st_name = hgl::graph::GetSurfaceTypeName(st);
            if (st_name && name == st_name)
            {
                out = st;
                return true;
            }
        }
        return false;
    }

    static bool TryParseVertexAttrib(const std::string &name, hgl::graph::VertexAttrib &out)
    {
        const hgl::graph::VertexAttrib attrib = hgl::graph::GetVertexAttribByName(name.c_str());
        if (attrib == hgl::graph::VertexAttrib::RANGE_SIZE)
            return false;

        out = attrib;
        return true;
    }

    static bool LoadSurfaceRequirementPlansFromJson(const std::string &json_file,
                                                    std::vector<SurfaceRequirementPlan> &out_plans,
                                                    std::string &error)
    {
        const auto AppendError = [](std::vector<std::string> &errors,
                                    const std::string &path,
                                    const std::string &msg)
        {
            errors.push_back(path + ": " + msg);
        };

        std::ifstream ifs(json_file, std::ios::in);
        if (!ifs.is_open())
        {
            error = "cannot open json: " + json_file;
            return false;
        }

        nlohmann::json root;
        try
        {
            ifs >> root;
        }
        catch (const std::exception &e)
        {
            error = std::string("json parse failed: ") + e.what();
            return false;
        }

        std::vector<std::string> schema_errors;

        if (!root.is_object())
        {
            error = "schema validation failed: root must be an object";
            return false;
        }

        if (!root.contains("surfaces"))
        {
            error = "schema validation failed: missing required field 'surfaces'";
            return false;
        }

        if (!root["surfaces"].is_array())
        {
            error = "schema validation failed: 'surfaces' must be an array";
            return false;
        }

        std::vector<SurfaceRequirementPlan> parsed;
        for (size_t i = 0; i < root["surfaces"].size(); ++i)
        {
            const auto &m = root["surfaces"][i];
            const std::string base_path = "surfaces[" + std::to_string(i) + "]";

            if (!m.is_object())
            {
                AppendError(schema_errors, base_path, "must be an object");
                continue;
            }

            if (!m.contains("surface"))
            {
                AppendError(schema_errors, base_path + ".surface", "missing required field");
                continue;
            }

            if (!m["surface"].is_string())
            {
                AppendError(schema_errors, base_path + ".surface", "must be a string");
                continue;
            }

            hgl::graph::SurfaceType st = hgl::graph::SurfaceType::Unlit;
            if (!TryParseSurfaceType(m["surface"].get<std::string>(), st))
            {
                AppendError(schema_errors,
                            base_path + ".surface",
                            "unknown surface name '" + m["surface"].get<std::string>() + "'");
                continue;
            }

            SurfaceRequirementPlan plan{};
            plan.surface = st;

            if (!m.contains("required_attribs"))
            {
                AppendError(schema_errors, base_path + ".required_attribs", "missing required field");
                continue;
            }

            if (!m["required_attribs"].is_array())
            {
                AppendError(schema_errors, base_path + ".required_attribs", "must be an array");
                continue;
            }

            if (!m.contains("diagnostic_surface"))
            {
                AppendError(schema_errors, base_path + ".diagnostic_surface", "missing required field");
                continue;
            }

            if (!m["diagnostic_surface"].is_string())
            {
                AppendError(schema_errors, base_path + ".diagnostic_surface", "must be a string");
                continue;
            }

            plan.diagnostic_surface = m["diagnostic_surface"].get<std::string>();

            for (size_t j = 0; j < m["required_attribs"].size(); ++j)
            {
                const auto &a = m["required_attribs"][j];
                const std::string attr_path = base_path + ".required_attribs[" + std::to_string(j) + "]";

                if (!a.is_string())
                {
                    AppendError(schema_errors, attr_path, "must be a string");
                    continue;
                }

                hgl::graph::VertexAttrib attrib = hgl::graph::VertexAttrib::RANGE_SIZE;
                const std::string attrib_name = a.get<std::string>();
                if (!TryParseVertexAttrib(attrib_name, attrib))
                {
                    AppendError(schema_errors, attr_path, "unknown vertex attrib '" + attrib_name + "'");
                    continue;
                }

                plan.required_attribs.push_back(attrib);
            }

            if (plan.required_attribs.empty())
            {
                AppendError(schema_errors, base_path + ".required_attribs", "must contain at least one valid attribute");
                continue;
            }

            parsed.push_back(std::move(plan));
        }

        if (!schema_errors.empty())
        {
            std::ostringstream oss;
            oss << "schema validation failed:";
            for (const auto &item : schema_errors)
                oss << "\n  - " << item;
            error = oss.str();
            return false;
        }

        if (parsed.empty())
        {
            error = "schema validation failed: surfaces parsed as empty";
            return false;
        }

        out_plans = std::move(parsed);
        return true;
    }

    static const std::vector<SurfaceRequirementPlan> &GetSurfaceRequirementPlans(const std::string &shader_lib_path)
    {
        static std::string cached_shader_lib_path;
        static std::vector<SurfaceRequirementPlan> cached_plans;

        if (cached_shader_lib_path == shader_lib_path && !cached_plans.empty())
            return cached_plans;

        const std::string json_file = shader_lib_path + "/compositor/material_fallbacks.json";
        std::vector<SurfaceRequirementPlan> loaded;
        std::string error;

        if (LoadSurfaceRequirementPlansFromJson(json_file, loaded, error))
        {
            std::fprintf(stdout,
                         "[CompositorAssembler][INFO] loaded surface requirement plans from %s\n",
                         json_file.c_str());
            cached_plans = std::move(loaded);
            cached_shader_lib_path = shader_lib_path;
            return cached_plans;
        }

        std::fprintf(stderr,
                     "[CompositorAssembler][WARN] requirement json unavailable (%s), using built-in plans\n",
                     error.c_str());

        cached_plans.assign(std::begin(kSurfaceRequirementPlans), std::end(kSurfaceRequirementPlans));
        cached_shader_lib_path = shader_lib_path;
        return cached_plans;
    }

    static const SurfaceRequirementPlan *FindSurfaceRequirementPlan(const hgl::graph::SurfaceType surface,
                                                                    const std::vector<SurfaceRequirementPlan> &plans)
    {
        for (const auto &plan : plans)
            if (plan.surface == surface)
                return &plan;
        return nullptr;
    }

    static bool IsAnyRequiredAttribMissing(const hgl::graph::mtl::MaterialVariantKey &key,
                                           const std::vector<hgl::graph::VertexAttrib> &required_attribs)
    {
        for (const hgl::graph::VertexAttrib attrib : required_attribs)
            if (!key.HasVertexAttrib(attrib))
                return true;

        return false;
    }

    static std::string ResolveSurfacePathByRequirements(const hgl::graph::SurfaceType surface,
                                                        const std::string &shader_lib_path,
                                                        const hgl::graph::mtl::MaterialVariantKey &key,
                                                        const std::string &default_surface_path)
    {
        const auto &plans = GetSurfaceRequirementPlans(shader_lib_path);
        const SurfaceRequirementPlan *plan = FindSurfaceRequirementPlan(surface, plans);
        if (!plan)
            return default_surface_path;

        if (IsAnyRequiredAttribMissing(key, plan->required_attribs))
        {
            std::fprintf(stderr,
                         "[CompositorAssembler][ERROR] surface=%s required_attrib_missing -> diagnostic_surface=%s\n",
                         hgl::graph::GetSurfaceTypeName(surface),
                         plan->diagnostic_surface.c_str());
            return plan->diagnostic_surface;
        }

        return default_surface_path;
    }

    // -----------------------------------------------------------------------
    // Attribute semantic registry + decode-helper emission (Step 3)
    // -----------------------------------------------------------------------

    struct AttribSemanticDef
    {
        hgl::graph::VertexAttrib attrib;
        const char *location_macro;     ///< e.g. "NORMAL_LOCATION"
        const char *input_var_name;     ///< e.g. "inNormal"
        const char *logical_type;       ///< return type of decode fn, e.g. "vec3"
        const char *custom_define;      ///< e.g. "ULRE_CUSTOM_NORMAL_ATTRIB"
        const char *decode_fn_sig;      ///< e.g. "vec3 ULRE_DecodeNormal()"

        struct Encoding
        {
            const char *input_glsl;     ///< GLSL input type ("vec3", "vec4", ...)
            const char *decode_expr;    ///< decode expression; '$' = input variable
            bool        needs_oct;      ///< true when OctDecode() is required
        };
        Encoding encodings[6];          ///< index 0 = best/default; {nullptr,...} terminates
    };

    static const AttribSemanticDef kAttribSemanticRegistry[] =
    {
        // ----- Normal (logical vec3) -----
        {
            hgl::graph::VertexAttrib::Normal,
            "NORMAL_LOCATION", "inNormal", "vec3",
            "ULRE_CUSTOM_NORMAL_ATTRIB", "vec3 ULRE_DecodeNormal()",
            {
                { "vec3", "$",                           false },  // 0: R32G32B32_SFLOAT (default)
                { "vec4", "normalize($.xyz)",            false },  // 1: A2B10G10R10_SNORM_PACK32
                { "vec3", "normalize($ * 2.0 - 1.0)",   false },  // 2: B10G11R11_UFLOAT_PACK32
                { "vec2", "OctDecode($)",                true  },  // 3: R16G16_SFLOAT oct
                { "vec2", "OctDecode($ * 2.0 - 1.0)",   true  },  // 4: R8G8_UNORM oct
                { nullptr, nullptr, false }
            },
        },
        // ----- Tangent (logical vec4: xyz=direction, w=handedness) -----
        {
            hgl::graph::VertexAttrib::Tangent,
            "TANGENT_LOCATION", "inTangent", "vec4",
            "ULRE_CUSTOM_TANGENT_ATTRIB", "vec4 ULRE_DecodeTangent()",
            {
                { "vec4", "$",                                        false },  // 0: R32G32B32A32_SFLOAT (default)
                { "vec4", "vec4(normalize($.xyz), sign($.w))",        false },  // 1: A2B10G10R10_SNORM_PACK32
                { "vec3", "vec4(normalize($), 1.0)",                  false },  // 2: R16G16B16_SFLOAT (no handedness)
                { nullptr, nullptr, false }
            },
        },
        // ----- Color (logical vec4) -----
        {
            hgl::graph::VertexAttrib::Color,
            "COLOR_LOCATION", "inColor", "vec4",
            "ULRE_CUSTOM_COLOR_ATTRIB", "vec4 ULRE_DecodeColor()",
            {
                { "vec4", "$",             false },  // 0: R32G32B32A32_SFLOAT or R8G8B8A8_UNORM (default)
                { "vec3", "vec4($, 1.0)",  false },  // 1: B10G11R11_UFLOAT_PACK32
                { nullptr, nullptr, false }
            },
        },
    };

    // -----------------------------------------------------------------------
    // VkFormat → encoding index map (Step 4)
    // Each row corresponds to a kAttribSemanticRegistry entry (same order).
    // Each column is an encoding index (matching Encoding[] in that entry).
    // VkFormat values that map to an encoding index are listed explicitly;
    // anything unrecognised falls back to index 0 (highest quality / raw float).
    // -----------------------------------------------------------------------

    struct AttribFormatEntry
    {
        uint32_t vk_format;   ///< VkFormat cast to uint32_t (avoids Vulkan header in .h)
        uint32_t encoding;    ///< index into kAttribSemanticRegistry[i].encodings
    };

    // Normal attrib VkFormat → encoding index
    static constexpr AttribFormatEntry kNormalFormatMap[] =
    {
        { 106, 0 },  // VK_FORMAT_R32G32B32_SFLOAT        (= 106) → 0 (raw vec3)
        {  65, 1 },  // VK_FORMAT_A2B10G10R10_SNORM_PACK32 (=  65) → 1 (normalize xyz)
        { 122, 2 },  // VK_FORMAT_B10G11R11_UFLOAT_PACK32  (= 122) → 2 (unorm remap)
        {  83, 3 },  // VK_FORMAT_R16G16_SFLOAT             (=  83) → 3 (oct sfloat)
        {  16, 4 },  // VK_FORMAT_R8G8_UNORM                (=  16) → 4 (oct unorm)
    };

    // Tangent attrib VkFormat → encoding index
    static constexpr AttribFormatEntry kTangentFormatMap[] =
    {
        { 109, 0 },  // VK_FORMAT_R32G32B32A32_SFLOAT      (= 109) → 0 (raw vec4)
        {  65, 1 },  // VK_FORMAT_A2B10G10R10_SNORM_PACK32 (=  65) → 1 (normalize xyz + sign w)
        {  90, 2 },  // VK_FORMAT_R16G16B16_SFLOAT          (=  90) → 2 (vec3, no handedness)
    };

    // Color attrib VkFormat → encoding index
    static constexpr AttribFormatEntry kColorFormatMap[] =
    {
        { 109, 0 },  // VK_FORMAT_R32G32B32A32_SFLOAT → 0 (raw vec4)
        {  37, 0 },  // VK_FORMAT_R8G8B8A8_UNORM      → 0 (raw vec4, driver converts)
        { 122, 1 },  // VK_FORMAT_B10G11R11_UFLOAT_PACK32 → 1 (vec3 → vec4)
    };

    /// Per-attrib format map descriptor (parallel to kAttribSemanticRegistry).
    struct AttribFormatMapDesc
    {
        hgl::graph::VertexAttrib   attrib;
        const AttribFormatEntry   *entries;
        uint32_t                   count;
    };

    static constexpr AttribFormatMapDesc kAttribFormatMaps[] =
    {
        { hgl::graph::VertexAttrib::Normal,
          kNormalFormatMap,
          static_cast<uint32_t>(std::size(kNormalFormatMap)) },
        { hgl::graph::VertexAttrib::Tangent,
          kTangentFormatMap,
          static_cast<uint32_t>(std::size(kTangentFormatMap)) },
        { hgl::graph::VertexAttrib::Color,
          kColorFormatMap,
          static_cast<uint32_t>(std::size(kColorFormatMap)) },
    };

    /// Replaces every '$' in expr_template with var_name.
    static std::string ResolveDecode(const char *expr_template, const char *var_name)
    {
        std::string out;
        for (const char *p = expr_template; *p; ++p)
            if (*p == '$') out += var_name;
            else           out.push_back(*p);
        return out;
    }

    /// Emits layout declarations + ULRE_Decode*() helper functions into out/writer
    /// for every vertex attribute that (a) is present in the key AND (b) has a registry entry.
    static void EmitAttribDecodeHelpers(
        std::string                                       &out,
        hgl::graph::ShaderWriter                          &writer,
        const hgl::graph::mtl::MaterialVariantKey         &key)
    {
        // First pass: check whether OctDecode() will be needed.
        bool need_oct = false;
        for (const auto &sem : kAttribSemanticRegistry)
        {
            if (!key.HasVertexAttrib(sem.attrib)) continue;
            const uint32_t req_idx = key.GetAttribEncoding(sem.attrib);
            uint32_t actual = 0;
            for (uint32_t i = 0; i <= req_idx && sem.encodings[i].input_glsl; ++i)
                actual = i;
            if (sem.encodings[actual].needs_oct) { need_oct = true; break; }
        }
        if (need_oct)
            writer.EmitInclude("common/oct_decode.glsl");

        // Second pass: emit per-attrib declarations + decode functions.
        for (const auto &sem : kAttribSemanticRegistry)
        {
            if (!key.HasVertexAttrib(sem.attrib)) continue;

            // Clamp the requested encoding index to the last valid entry.
            const uint32_t req_idx = key.GetAttribEncoding(sem.attrib);
            uint32_t actual = 0;
            for (uint32_t i = 0; i <= req_idx && sem.encodings[i].input_glsl; ++i)
                actual = i;
            const auto &enc = sem.encodings[actual];

            // Tell the template to skip its default declaration.
            writer.EmitDefine(sem.custom_define);

            // Raw input attribute declaration.
            out += "layout(location=";
            out += sem.location_macro;
            out += ") in ";
            out += enc.input_glsl;
            out += ' ';
            out += sem.input_var_name;
            out += ";\n";

            // Decode helper function.
            out += sem.decode_fn_sig;
            out += " { return ";
            out += ResolveDecode(enc.decode_expr, sem.input_var_name);
            out += "; }\n\n";
        }
    }

    /// Generic emit function: builds the GLSL preamble from a StageManifest.
    static std::string EmitStageSource(
        const StageManifest                          &m,
        const hgl::graph::mtl::MaterialVariantKey    &key,
        hgl::graph::RenderAlphaMode                   blend,
        const std::string                            &surface_path)
    {
        std::string out = "#version 450\n\n";
        hgl::graph::ShaderWriter writer(out);

        for (const char *define : m.always_defines)
            writer.EmitDefine(define);

        for (const auto &[attrib, define] : m.attrib_defines)
            if (key.HasVertexAttrib(attrib))
                writer.EmitDefine(define);

        if (m.emit_alpha_mode)
        {
            if (blend == hgl::graph::RenderAlphaMode::Masked)
                writer.EmitDefine("ALPHA_MODE_MASKED");
            else if (blend == hgl::graph::RenderAlphaMode::Dither)
                writer.EmitDefine("ALPHA_MODE_DITHER");
        }

        if (m.uses_vert_forward_main)
        {
            EmitAttribDecodeHelpers(out, writer, key);
        }

        for (const char *inc : m.pre_special_includes)
            writer.EmitInclude(inc);

        if (m.needs_sky)
            out += "#include SKYLIGHT_FUNCTION_FILE\n";

        if (m.needs_lighting)
            out += "#include LIGHTING_FUNCTION_FILE\n";

        if (m.needs_surface)
            writer.EmitInclude(surface_path);

        for (const char *inc : m.post_special_includes)
            writer.EmitInclude(inc);

        return out;
    }

    // VS manifest table — one entry per generated vertex-shader template.
    static const StageManifest kVSManifests[] =
    {
        {
            .template_path        = "compositor/main_forward_unlit_vertexcolor.vert.glsl",
            .always_defines       = {"HAS_VERTEX_COLOR"},
            .pre_special_includes = {"compositor/vert_forward_ubo.glsl",
                                     "compositor/vert_forward_main.glsl"},
            .uses_vert_forward_main = true,
        },
        {
            .template_path        = "compositor/main_forward_unlit_luminance.vert.glsl",
            .always_defines       = {"HAS_LUMINANCE"},
            .pre_special_includes = {"compositor/vert_forward_ubo.glsl",
                                     "compositor/vert_forward_main.glsl"},
            .uses_vert_forward_main = true,
        },
        {
            .template_path        = "compositor/main_forward_unlit_luminance_2d.vert.glsl",
            .always_defines       = {"VERT_INPUT_2D", "HAS_LUMINANCE"},
            .pre_special_includes = {"compositor/vert_forward_ubo.glsl",
                                     "compositor/vert_forward_main.glsl"},
            .uses_vert_forward_main = true,
        },
        {
            .template_path        = "compositor/main_forward_unlit_normal.vert.glsl",
            .always_defines       = {"HAS_WORLD_POS", "HAS_WORLD_NORMAL"},
            .pre_special_includes = {"compositor/vert_forward_ubo.glsl",
                                     "compositor/vert_forward_main.glsl"},
            .uses_vert_forward_main = true,
        },
        {
            .template_path        = "compositor/main_forward_sky.vert.glsl",
            .always_defines       = {"HAS_DIRECTION"},
            .pre_special_includes = {"compositor/vert_forward_ubo.glsl",
                                     "compositor/vert_forward_main.glsl"},
            .uses_vert_forward_main = true,
        },
        {
            // Billboard: #version 450 + self-include (no defines needed).
            .template_path        = "compositor/main_forward_billboard_dynamic.vert.glsl",
            .pre_special_includes = {"compositor/main_forward_billboard_dynamic.vert.glsl"},
        },
        {
            .template_path        = "compositor/main_forward_billboard_fixed.vert.glsl",
            .pre_special_includes = {"compositor/main_forward_billboard_fixed.vert.glsl"},
        },
        {
            .template_path        = "compositor/main_terrain_grid.vert.glsl",
            .pre_special_includes = {"compositor/main_terrain_grid.vert.glsl"},
        },
        {
            // Forward-lit VS: HAS_UV0 and HAS_WORLD_NORMAL are mesh-conditional.
            .template_path        = "compositor/main_forward_lit.vert.glsl",
            .always_defines       = {"HAS_WORLD_POS"},
            .attrib_defines       = {{hgl::graph::VertexAttrib::TexCoord, "HAS_UV0"},
                                     {hgl::graph::VertexAttrib::Normal,   "HAS_WORLD_NORMAL"},
                                     {hgl::graph::VertexAttrib::Tangent,  "HAS_WORLD_TANGENT"}},
            .pre_special_includes = {"compositor/vert_forward_ubo.glsl",
                                     "compositor/vert_forward_main.glsl"},
            .uses_vert_forward_main = true,
        },
    };

    // FS manifest table — one entry per generated fragment-shader template.
    static const StageManifest kFSManifests[] =
    {
        {
            .template_path         = "compositor/main_forward_unlit_vertexcolor.frag.glsl",
            .always_defines        = {"HAS_VERTEX_COLOR"},
            .pre_special_includes  = {"compositor/frag_forward_ubo.glsl"},
            .needs_surface         = true,
            .post_special_includes = {"compositor/frag_forward_main.glsl"},
        },
        {
            .template_path         = "compositor/main_forward_unlit_luminance.frag.glsl",
            .always_defines        = {"HAS_LUMINANCE"},
            .pre_special_includes  = {"compositor/frag_forward_ubo.glsl"},
            .needs_surface         = true,
            .post_special_includes = {"compositor/frag_forward_main.glsl"},
        },
        {
            .template_path         = "compositor/main_forward_unlit_normal.frag.glsl",
            .always_defines        = {"HAS_WORLD_POS", "HAS_WORLD_NORMAL", "NEEDS_CAMERA"},
            .pre_special_includes  = {"compositor/frag_forward_ubo.glsl"},
            .needs_surface         = true,
            .post_special_includes = {"compositor/frag_forward_main.glsl"},
        },
        {
            // Billboard FS: alpha mode defines driven by blend_mode at emit time.
            .template_path         = "compositor/main_forward_billboard.frag.glsl",
            .always_defines        = {"HAS_TEXCOORD"},
            .emit_alpha_mode       = true,
            .pre_special_includes  = {"compositor/frag_forward_ubo.glsl"},
            .needs_surface         = true,
            .post_special_includes = {"compositor/frag_forward_main.glsl"},
        },
        {
            .template_path         = "compositor/main_forward_sky.frag.glsl",
            .always_defines        = {"HAS_DIRECTION"},
            .pre_special_includes  = {"compositor/frag_forward_ubo.glsl"},
            .needs_surface         = true,
            .post_special_includes = {"compositor/frag_forward_main.glsl"},
        },
        {
            .template_path         = "compositor/main_terrain_grid.frag.glsl",
            .always_defines        = {"HAS_WORLD_NORMAL", "HAS_CLIP_POS"},
            .pre_special_includes  = {"compositor/frag_forward_ubo.glsl"},
            .needs_surface         = true,
            .post_special_includes = {"compositor/frag_forward_main.glsl"},
        },
        {
            // Forward-lit FS: HAS_WORLD_NORMAL and HAS_UV0 are mesh-conditional.
            .template_path         = "compositor/main_forward_lit.frag.glsl",
            .always_defines        = {"ENABLE_LIGHTING", "NEEDS_CAMERA", "NEEDS_SKY",
                                      "HAS_WORLD_POS"},
            .attrib_defines        = {{hgl::graph::VertexAttrib::Normal,   "HAS_WORLD_NORMAL"},
                                      {hgl::graph::VertexAttrib::TexCoord, "HAS_UV0"}},
            .pre_special_includes  = {"compositor/frag_forward_ubo.glsl"},
            .needs_sky             = true,
            .needs_lighting        = true,
            .needs_surface         = true,
            .post_special_includes = {"compositor/frag_forward_main.glsl"},
        },
    };

    static const SurfaceFunctionRoute kSurfaceFunctionRoutes[] = {
        {hgl::graph::SurfaceType::PureColor2D,  "surface/unlit_color3d_surface.glsl"},
        {hgl::graph::SurfaceType::VertexColor2D,"surface/unlit_vertexcolor_surface.glsl"},
        {hgl::graph::SurfaceType::PureTexture2D,"surface/2d/puretexture2d_surface.glsl"},
        {hgl::graph::SurfaceType::Text2D,       "surface/2d/text2d_surface.glsl"},
        {hgl::graph::SurfaceType::Standard,     "surface/standard_surface.glsl"},
        {hgl::graph::SurfaceType::Unlit,        "surface/unlit_color3d_surface.glsl"},
        {hgl::graph::SurfaceType::Skin,         "surface/skin_surface.glsl"},
        {hgl::graph::SurfaceType::Hair,         "surface/hair_surface.glsl"},
        {hgl::graph::SurfaceType::Cloth,        "surface/cloth_surface.glsl"},
        {hgl::graph::SurfaceType::Eye,          "surface/eye_surface.glsl"},
        {hgl::graph::SurfaceType::Foliage,      "surface/foliage_surface.glsl"},
        {hgl::graph::SurfaceType::ClearCoat,    "surface/clearcoat_surface.glsl"},
        {hgl::graph::SurfaceType::Water,        "surface/water_surface.glsl"},
        {hgl::graph::SurfaceType::Terrain,      "surface/terrain_surface.glsl"},
        {hgl::graph::SurfaceType::Sky,          "surface/sky_surface.glsl"},
    };

    template<typename Route, size_t N>
    const Route *FindRouteByTemplatePath(const Route (&routes)[N], const std::string &template_path)
    {
        for(const auto &route : routes)
        {
            if(template_path == route.template_path)
                return &route;
        }

        return nullptr;
    }

    template<size_t N>
    const SurfaceFunctionRoute *FindSurfaceFunctionRoute(const SurfaceFunctionRoute (&routes)[N], const hgl::graph::SurfaceType surface)
    {
        for(const auto &route : routes)
        {
            if(route.surface == surface)
                return &route;
        }

        return nullptr;
    }

    size_t FindVersionDirectiveLineEnd(const std::string &source)
    {
        if(source.empty())
            return std::string::npos;

        size_t begin = 0;

        // UTF-8 BOM support
        if(source.size() >= 3
        && static_cast<unsigned char>(source[0]) == 0xEF
        && static_cast<unsigned char>(source[1]) == 0xBB
        && static_cast<unsigned char>(source[2]) == 0xBF)
        {
            begin = 3;
        }

        while(begin < source.size())
        {
            const char ch = source[begin];
            if(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
            {
                ++begin;
                continue;
            }
            break;
        }

        if(begin + 8 <= source.size() && source.compare(begin, 8, "#version") == 0)
        {
            const size_t eol = source.find('\n', begin);
            return eol == std::string::npos ? source.size() : (eol + 1);
        }

        return std::string::npos;
    }

    std::string BuildGLSLPreviewFirstLines(const std::string &source, const size_t max_lines)
    {
        if(source.empty() || max_lines == 0)
            return std::string();

        size_t line_count = 0;
        size_t pos = 0;

        while(pos < source.size() && line_count < max_lines)
        {
            const size_t eol = source.find('\n', pos);
            ++line_count;

            if(eol == std::string::npos)
                return source;

            pos = eol + 1;
        }

        if(pos >= source.size())
            return source;

        return source.substr(0, pos);
    }

    std::string BuildAssembleReadFailureMessage(const char *stage,
                                                const std::string &template_path,
                                                const std::string &file_path,
                                                const std::string &reason)
    {
        std::string msg;
        msg.reserve(256 + reason.size());
        msg += "[CompositorAssembler] ";
        msg += stage;
        msg += " template load failed. template=";
        msg += template_path.empty() ? "<auto-route>" : template_path;
        msg += " file=";
        msg += file_path;
        msg += " reason=";
        msg += reason;
        return msg;
    }

    std::string BuildAssemblePreprocessFailureMessage(const char *stage,
                                                      const std::string &template_path,
                                                      const std::string &detail,
                                                      const std::string &glsl_source)
    {
        std::string msg;
        msg.reserve(384 + detail.size() + std::min<size_t>(glsl_source.size(), 2048));
        msg += "[CompositorAssembler] ";
        msg += stage;
        msg += " preprocess failed. template=";
        msg += template_path.empty() ? "<auto-route>" : template_path;
        msg += " detail=";
        msg += detail;
        msg += "\n[";
        msg += stage;
        msg += " GLSL first 80 lines]\n";
        msg += BuildGLSLPreviewFirstLines(glsl_source, 80);
        return msg;
    }
}

namespace hgl::graph
{
    // -----------------------------------------------------------------------
    // VkFormatToAttribEncoding — Step 4 public utility
    // -----------------------------------------------------------------------

    uint32_t VkFormatToAttribEncoding(VertexAttrib attrib, uint32_t vk_format) noexcept
    {
        for (const auto &desc : kAttribFormatMaps)
        {
            if (desc.attrib != attrib)
                continue;
            for (uint32_t i = 0; i < desc.count; ++i)
                if (desc.entries[i].vk_format == vk_format)
                    return desc.entries[i].encoding;
            return 0u; // attrib found but format unknown → default encoding
        }
        return 0u; // attrib not in registry
    }

    CompositorAssembler::CompositorAssembler()
        : CompositorAssembler(GetShaderLibraryPath())
    {}

    CompositorAssembler::CompositorAssembler(const std::string &shader_library_path)
        : shader_lib_path_(shader_library_path)
    {}

    bool CompositorAssembler::ReadFile(const std::string &path, std::string &out_content, std::string &out_error) const
    {
        std::ifstream ifs(path, std::ios::in);
        if (!ifs.is_open())
        {
            out_error = "Failed to open file: " + path;
            return false;
        }
        std::ostringstream ss;
        ss << ifs.rdbuf();
        out_content = ss.str();
        return true;
    }

    bool CompositorAssembler::TryBuildGeneratedVSTemplatePath(const std::string &template_path, const mtl::MaterialVariantKey &key, std::string &out_source) const
    {
        if (const StageManifest *m = FindRouteByTemplatePath(kVSManifests, template_path))
        {
            out_source = EmitStageSource(*m, key, RenderAlphaMode::Opaque, std::string());
            return true;
        }

        return false;
    }

    bool CompositorAssembler::TryBuildGeneratedFSTemplatePath(const std::string &template_path, const mtl::MaterialVariantKey &key, RenderAlphaMode blend, const std::string &surface_path, std::string &out_source) const
    {
        if (const StageManifest *m = FindRouteByTemplatePath(kFSManifests, template_path))
        {
            out_source = EmitStageSource(*m, key, blend, surface_path);
            return true;
        }

        return false;
    }

    std::string CompositorAssembler::GetCompositorVSPath(SurfaceType surface, PassType pass) const
    {
        // 2D Materials — reuse vert_forward_main.glsl via VERT_INPUT_2D
        if (Is2DSurfaceType(surface))
        {
            switch (surface)
            {
            case SurfaceType::PureColor2D:
                // No texture, no vertex color — minimal VS
                return shader_lib_path_ + "/compositor/main_forward_2d_common.vert.glsl";

            case SurfaceType::PureTexture2D:
            case SurfaceType::Text2D:
                // Needs UV0 for texture sampling
                return shader_lib_path_ + "/compositor/main_forward_2d_texcoord.vert.glsl";

            case SurfaceType::VertexColor2D:
                // Needs vertex color varying
                return shader_lib_path_ + "/compositor/main_forward_2d_vertexcolor.vert.glsl";

            default:
                break;
            }
        }

        // Unlit 表面使用精简 VS（仅 Position + 双 ID，无 Normal/UV）
        if (surface == SurfaceType::Unlit)
        {
            switch (pass)
            {
            case PassType::ForwardOpaque:
            case PassType::ForwardMasked:
            case PassType::ForwardTransparent:
                return shader_lib_path_ + "/compositor/main_forward_unlit.vert.glsl";
            default:
                return shader_lib_path_ + "/compositor/main_forward_unlit.vert.glsl";
            }
        }

        // Lit 表面走完整 VS（Position + Normal + UV + 双 ID）
        switch (pass)
        {
        case PassType::ForwardOpaque:
        case PassType::ForwardMasked:
        case PassType::ForwardTransparent:
        case PassType::ForwardDither:
        case PassType::ForwardA2C:
            return shader_lib_path_ + "/compositor/main_forward_opaque.vert.glsl";

        case PassType::ShadowOpaque:
        case PassType::ShadowMasked:
            return shader_lib_path_ + "/compositor/main_shadow.vert.glsl"; // 后续实现

        case PassType::EarlyZSolid:
        case PassType::EarlyZMasked:
            return shader_lib_path_ + "/compositor/main_earlyz.vert.glsl"; // 后续实现

        default:
            return shader_lib_path_ + "/compositor/main_forward_opaque.vert.glsl";
        }
    }

    std::string CompositorAssembler::GetCompositorFSPath(SurfaceType surface, RenderAlphaMode blend, PassType pass) const
    {
        // 2D Materials — reuse frag_forward_main.glsl, routed by pass type
        if (Is2DSurfaceType(surface))
        {
            switch (pass)
            {
            case PassType::ForwardOpaque:
                return shader_lib_path_ + "/compositor/main_forward_2d_opaque.frag.glsl";

            case PassType::ForwardMasked:
                return shader_lib_path_ + "/compositor/main_forward_2d_masked.frag.glsl";

            case PassType::ForwardDither:
                return shader_lib_path_ + "/compositor/main_forward_2d_dither.frag.glsl";

            case PassType::ForwardTransparent:
            default:
                return shader_lib_path_ + "/compositor/main_forward_2d_transparent.frag.glsl";
            }
        }

        // Unlit 表面类型使用专用 FS 模板（无光照）
        if (surface == SurfaceType::Unlit)
        {
            switch (pass)
            {
            case PassType::ForwardOpaque:
            case PassType::ForwardMasked:
            case PassType::ForwardTransparent:
                return shader_lib_path_ + "/compositor/main_forward_unlit.frag.glsl";

            case PassType::ShadowOpaque:
            case PassType::ShadowMasked:
                return shader_lib_path_ + "/compositor/main_shadow.frag.glsl"; // 后续实现

            case PassType::EarlyZSolid:
            case PassType::EarlyZMasked:
                return shader_lib_path_ + "/compositor/main_earlyz.frag.glsl"; // 后续实现

            default:
                return shader_lib_path_ + "/compositor/main_forward_unlit.frag.glsl";
            }
        }

        // 非 Unlit 走 Lit 路径
        switch (pass)
        {
        case PassType::ForwardOpaque:
            return shader_lib_path_ + "/compositor/main_forward_opaque.frag.glsl";

        case PassType::ForwardMasked:
            return shader_lib_path_ + "/compositor/main_forward_masked.frag.glsl";

        case PassType::ForwardTransparent:
            return shader_lib_path_ + "/compositor/main_forward_transparent.frag.glsl";

        case PassType::ForwardDither:
            return shader_lib_path_ + "/compositor/main_forward_dither.frag.glsl";

        case PassType::ForwardA2C:
            return shader_lib_path_ + "/compositor/main_forward_a2c.frag.glsl";

        case PassType::ShadowOpaque:
        case PassType::ShadowMasked:
            return shader_lib_path_ + "/compositor/main_shadow.frag.glsl"; // 后续实现

        case PassType::EarlyZSolid:
        case PassType::EarlyZMasked:
            return shader_lib_path_ + "/compositor/main_earlyz.frag.glsl"; // 后续实现

        default:
            return shader_lib_path_ + "/compositor/main_forward_opaque.frag.glsl";
        }
    }

    std::string CompositorAssembler::GetSurfaceFunctionPath(SurfaceType surface) const
    {
        if(const SurfaceFunctionRoute *route = FindSurfaceFunctionRoute(kSurfaceFunctionRoutes, surface))
            return route->path;

        return "surface/standard_surface.glsl";
    }

    std::string CompositorAssembler::InjectDefines(const std::string &source, const mtl::MaterialVariantKey &key) const
    {
        std::string defines;
        {
            char buf[128] = {};
            const uint32 shadow_mode = 0u;
            std::snprintf(buf,
                          sizeof(buf),
                          "#define SURFACE_TYPE %d\n"
                          "#define SHADOW_MODE %u\n",
                          static_cast<int>(key.surface_type),
                          shadow_mode);
            defines += buf;
        }

        for (uint8 i = 0; i < uint8(mtl::SamplerSlotCount); ++i)
        {
            const mtl::SamplerSlot slot = static_cast<mtl::SamplerSlot>(i);
            if (key.GetTextureSourceMode(slot) == mtl::TextureSourceMode::Array)
            {
                defines += "#define TEXTURE_ARRAY_MODE\n";
                break;
            }
        }

        if(defines.empty())
            return source;

        const size_t insert_pos = FindVersionDirectiveLineEnd(source);
        if(insert_pos != std::string::npos)
        {
            std::string result;
            result.reserve(source.size() + defines.size() + 2);
            result.append(source, 0, insert_pos);
            result.append("\n");
            result.append(defines);
            result.append("\n");
            result.append(source, insert_pos, std::string::npos);
            return result;
        }

        // 没有 #version 行则直接在头部插入
        return defines + "\n" + source;
    }

    std::string CompositorAssembler::ReplaceSurfaceInclude(const std::string &source, const std::string &surface_path) const
    {
        const std::string marker = "#include SURFACE_FUNCTION_FILE";
        auto pos = source.find(marker);
        if (pos == std::string::npos)
            return source;

        std::string replacement = "#include \"" + surface_path + "\"";

        std::string result;
        result.reserve(source.size() + replacement.size());
        result.append(source, 0, pos);
        result.append(replacement);
        result.append(source, pos + marker.size(), std::string::npos);
        return result;
    }

    static const char *GetSkyLightGLSLPath(mtl::SkyLightAmbientModel model)
    {
        using mtl::SkyLightAmbientModel;
        switch (model)
        {
            case SkyLightAmbientModel::Simple:              return "common/skylight_simple.glsl";
            case SkyLightAmbientModel::FakeAtmosphere:      return "common/skylight_fake_atm.glsl";
            case SkyLightAmbientModel::CubeMap:             return "common/skylight_cubemap.glsl";
            case SkyLightAmbientModel::SphericalHarmonics:  return "common/skylight_sh.glsl";
            case SkyLightAmbientModel::IBL:                 return "common/skylight_ibl.glsl";
            default:                                        return "common/skylight_simple.glsl";
        }
    }

    std::string CompositorAssembler::ReplaceSkyLightInclude(const std::string &source, mtl::SkyLightAmbientModel sky_model) const
    {
        const std::string marker = "#include SKYLIGHT_FUNCTION_FILE";
        auto pos = source.find(marker);
        if (pos == std::string::npos)
            return source;

        std::string replacement = "#include \"" + std::string(GetSkyLightGLSLPath(sky_model)) + "\"";

        std::string result;
        result.reserve(source.size() + replacement.size());
        result.append(source, 0, pos);
        result.append(replacement);
        result.append(source, pos + marker.size(), std::string::npos);
        return result;
    }

    std::string CompositorAssembler::ReplaceLightingInclude(const std::string &source, mtl::LightingModel lighting_model) const
    {
        const std::string marker = "#include LIGHTING_FUNCTION_FILE";
        auto pos = source.find(marker);
        if (pos == std::string::npos)
            return source;

        std::string replacement = "#include \"" + std::string(mtl::GetLightingModelGLSLPath(lighting_model)) + "\"";

        std::string result;
        result.reserve(source.size() + replacement.size());
        result.append(source, 0, pos);
        result.append(replacement);
        result.append(source, pos + marker.size(), std::string::npos);
        return result;
    }

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        SurfaceType     surface,
        RenderAlphaMode       blend,
        PassType        pass,
        const char     *vs_template_override,
        const char     *fs_template_override,
        const char     *surface_function_override,
        mtl::SkyLightAmbientModel sky_model,
        mtl::LightingModel lighting_model
    ) const
    {
        AssembleResult result{};

        // 1. Build key used for macro define injection.
        mtl::MaterialVariantKey key{};
        key.surface_type = surface;
        key.blend_mode = blend;
        key.pass_hint = pass;
        key.sky_ambient_model = sky_model;
        key.lighting_model = lighting_model;

        // 2. 获取模板文件路径（支持覆盖）
        std::string vs_path = (vs_template_override && vs_template_override[0])
            ? shader_lib_path_ + "/" + vs_template_override
            : GetCompositorVSPath(surface, pass);
        std::string fs_path = (fs_template_override && fs_template_override[0])
            ? shader_lib_path_ + "/" + fs_template_override
            : GetCompositorFSPath(surface, blend, pass);
        std::string surface_rel = (surface_function_override && surface_function_override[0])
            ? surface_function_override
            : GetSurfaceFunctionPath(surface);

        // 3. VS 模板
        std::string vs_source;
        if (vs_template_override && vs_template_override[0])
        {
            // Template override: Generated-first, disk fallback
            std::string read_error;
            if (!TryBuildGeneratedVSTemplatePath(vs_template_override, key, vs_source)
             && !ReadFile(vs_path, vs_source, read_error))
            {
                result.error_message = BuildAssembleReadFailureMessage("VS", vs_template_override, vs_path, read_error);
                result.success = false;
                return result;
            }
        }
        else
        {
            std::string read_error;
            if(!ReadFile(vs_path, vs_source, read_error))
            {
                result.error_message = BuildAssembleReadFailureMessage("VS", std::string(), vs_path, read_error);
                result.success = false;
                return result;
            }
        }

        // 4. FS 模板
        std::string fs_source;
        if (fs_template_override && fs_template_override[0])
        {
            // Template override: Generated-first, disk fallback
            std::string read_error;
            if (!TryBuildGeneratedFSTemplatePath(fs_template_override, key, blend, surface_rel, fs_source)
             && !ReadFile(fs_path, fs_source, read_error))
            {
                result.error_message = BuildAssembleReadFailureMessage("FS", fs_template_override, fs_path, read_error);
                result.success = false;
                return result;
            }
        }
        else
        {
            std::string read_error;
            if(!ReadFile(fs_path, fs_source, read_error))
            {
                result.error_message = BuildAssembleReadFailureMessage("FS", std::string(), fs_path, read_error);
                result.success = false;
                return result;
            }
        }

        // 5. 注入 #define
        vs_source = InjectDefines(vs_source, key);
        fs_source = InjectDefines(fs_source, key);

        if(vs_source.empty())
        {
            result.error_message = BuildAssemblePreprocessFailureMessage("VS",
                                                                          std::string(vs_template_override ? vs_template_override : ""),
                                                                          "InjectDefines produced empty source",
                                                                          vs_source);
            result.success = false;
            return result;
        }

        if(fs_source.empty())
        {
            result.error_message = BuildAssemblePreprocessFailureMessage("FS",
                                                                          fs_template_override ? fs_template_override : std::string(),
                                                                          "InjectDefines produced empty source",
                                                                          fs_source);
            result.success = false;
            return result;
        }

        // 6. 替换 FS 中的 SURFACE_FUNCTION_FILE（自定义/遗留文件模板仍支持）
        fs_source = ReplaceSurfaceInclude(fs_source, surface_rel);

        // 7. 替换 FS 中的 SKYLIGHT_FUNCTION_FILE（按天光模型选择实现文件）
        fs_source = ReplaceSkyLightInclude(fs_source, key.sky_ambient_model);

        // 8. 替换 FS 中的 LIGHTING_FUNCTION_FILE（按光照模型选择实现文件）
        fs_source = ReplaceLightingInclude(fs_source, key.lighting_model);

        result.vertex_glsl   = std::move(vs_source);
        result.fragment_glsl = std::move(fs_source);
        result.success       = true;
        return result;
    }

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc
    ) const
    {
        AssembleResult result{};

        std::string vs_path = desc.vs_template_path.empty()
            ? GetCompositorVSPath(key.surface_type, key.pass_hint)
            : shader_lib_path_ + "/" + desc.vs_template_path;
        std::string fs_path = desc.fs_template_path.empty()
            ? GetCompositorFSPath(key.surface_type, key.blend_mode, key.pass_hint)
            : shader_lib_path_ + "/" + desc.fs_template_path;
        std::string surface_rel = desc.surface_function_path.empty()
            ? ResolveSurfacePathByRequirements(key.surface_type,
                                               shader_lib_path_,
                                               key,
                                               GetSurfaceFunctionPath(key.surface_type))
            : desc.surface_function_path;

        std::string vs_source;
        if (!desc.vs_template_path.empty())
        {
            std::string read_error;
            if (!TryBuildGeneratedVSTemplatePath(desc.vs_template_path, key, vs_source)
             && !ReadFile(vs_path, vs_source, read_error))
            {
                result.error_message = BuildAssembleReadFailureMessage("VS", desc.vs_template_path, vs_path, read_error);
                result.success = false;
                return result;
            }
        }
        else
        {
            std::string read_error;
            if(!ReadFile(vs_path, vs_source, read_error))
            {
                result.error_message = BuildAssembleReadFailureMessage("VS", std::string(), vs_path, read_error);
                result.success = false;
                return result;
            }
        }

        std::string fs_source;
        if (!desc.fs_template_path.empty())
        {
            std::string read_error;
            if (!TryBuildGeneratedFSTemplatePath(desc.fs_template_path, key, key.blend_mode, surface_rel, fs_source)
             && !ReadFile(fs_path, fs_source, read_error))
            {
                result.error_message = BuildAssembleReadFailureMessage("FS", desc.fs_template_path, fs_path, read_error);
                result.success = false;
                return result;
            }
        }
        else
        {
            std::string read_error;
            if(!ReadFile(fs_path, fs_source, read_error))
            {
                result.error_message = BuildAssembleReadFailureMessage("FS", std::string(), fs_path, read_error);
                result.success = false;
                return result;
            }
        }

        vs_source = InjectDefines(vs_source, key);
        fs_source = InjectDefines(fs_source, key);

        if(vs_source.empty())
        {
            result.error_message = BuildAssemblePreprocessFailureMessage("VS",
                                                                          desc.vs_template_path,
                                                                          "InjectDefines produced empty source",
                                                                          vs_source);
            result.success = false;
            return result;
        }

        if(fs_source.empty())
        {
            result.error_message = BuildAssemblePreprocessFailureMessage("FS",
                                                                          desc.fs_template_path,
                                                                          "InjectDefines produced empty source",
                                                                          fs_source);
            result.success = false;
            return result;
        }

        fs_source = ReplaceSurfaceInclude(fs_source, surface_rel);

        fs_source = ReplaceSkyLightInclude(fs_source, key.sky_ambient_model);

        // 替换 FS 中的 LIGHTING_FUNCTION_FILE（按光照模型选择实现文件）
        fs_source = ReplaceLightingInclude(fs_source, key.lighting_model);

        result.vertex_glsl   = std::move(vs_source);
        result.fragment_glsl = std::move(fs_source);
        result.success       = true;
        return result;
    }

    std::vector<PassType> CompositorAssembler::GetPassTypesForBlendMode(RenderAlphaMode blend)
    {
        switch (blend)
        {
        case RenderAlphaMode::Opaque:
            return { PassType::ForwardOpaque, PassType::ShadowOpaque, PassType::EarlyZSolid };

        case RenderAlphaMode::Masked:
            return { PassType::ForwardMasked, PassType::ShadowMasked, PassType::EarlyZMasked };

        case RenderAlphaMode::Transparent:
            // 透明物体无阴影、无 EarlyZ（从后往前排序第 8 Pass 渲染）
            return { PassType::ForwardTransparent };

        case RenderAlphaMode::Dither:
            // Dither 小批目使用 ShadowOpaque（不需要 alpha 阴影）
            return { PassType::ForwardDither, PassType::ShadowOpaque };

        case RenderAlphaMode::AlphaToCoverage:
            // A2C 阴影和 Masked 相同——需要 alpha discard 避免阴影漏光
            return { PassType::ForwardA2C, PassType::ShadowMasked };

        default:
            return { PassType::ForwardOpaque };
        }
    }
}
