#include <hgl/shadergen/InterstageVaryingLayout.h>
#include <cassert>
#include <cstdint>

namespace hgl::graph
{

// ──────────────────────────────────────────────────────────────────────────────
// Canonical varying table — single source of truth for all VS→FS locations.
// varying_vs.glsl and varying_fs.glsl must mirror this exactly.
// ──────────────────────────────────────────────────────────────────────────────
static const VaryingDesc kVaryingTable[] =
{
    // varying                       loc  type      name                      guard_macro              interp
    { InterstageVarying::MaterialInstanceID, 0, "uint",  "fragMaterialInstanceID", nullptr,                     VaryingInterp::Flat   },
    { InterstageVarying::WorldPos,           1, "vec3",  "fragWorldPos",           "HAS_POSITION",              VaryingInterp::Smooth },
    { InterstageVarying::WorldNormal,        2, "vec3",  "fragWorldNormal",        "HAS_NORMAL",                VaryingInterp::Smooth },
    { InterstageVarying::UV0,                3, "vec2",  "fragUV0",                "HAS_TEXCOORD",              VaryingInterp::Smooth },
    { InterstageVarying::VertexColor,        4, "vec4",  "fragVertexColor",        "HAS_COLOR",                 VaryingInterp::Smooth },
    { InterstageVarying::BillboardTexCoord,  5, "vec2",  "fragTexCoord",           "HAS_BILLBOARD_TEXCOORD",    VaryingInterp::Smooth },
    { InterstageVarying::Direction,          6, "vec3",  "fragDirection",          "HAS_DIRECTION",             VaryingInterp::Smooth },
    { InterstageVarying::Luminance,          7, "float", "fragLuminance",          "HAS_LUMINANCE",             VaryingInterp::Smooth },
    { InterstageVarying::ClipPos,            8, "vec4",  "fragClipPos",            "HAS_CLIP_POS",              VaryingInterp::Smooth },
    { InterstageVarying::WorldTangent,       9, "vec4",  "fragWorldTangent",       "HAS_TANGENT",               VaryingInterp::Smooth },
};

static_assert(sizeof(kVaryingTable)/sizeof(kVaryingTable[0]) == int(InterstageVarying::COUNT),
              "kVaryingTable size must match InterstageVarying::COUNT");

// ──────────────────────────────────────────────────────────────────────────────

const VaryingDesc *GetVaryingDesc(InterstageVarying v)
{
    int idx = int(v);
    if (idx < 0 || idx >= int(InterstageVarying::COUNT))
        return nullptr;
    return &kVaryingTable[idx];
}

// ──────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ──────────────────────────────────────────────────────────────────────────────

static std::string EmitDecl(const VaryingDesc &d, bool is_vs_out)
{
    std::string line;
    // optional guard open
    if (d.guard_macro)
    {
        line += "#ifdef ";
        line += d.guard_macro;
        line += "\n";
    }

    line += "layout(location=";
    line += std::to_string(d.location);
    line += ") ";

    // interpolation qualifier
    if (d.interp == VaryingInterp::Flat)
        line += "flat ";
    else if (d.interp == VaryingInterp::NoPerspective)
        line += "noperspective ";

    line += is_vs_out ? "out " : "in ";
    line += d.glsl_type;
    line += " ";
    line += d.name;
    line += ";\n";

    if (d.guard_macro)
        line += "#endif\n";

    return line;
}

// ──────────────────────────────────────────────────────────────────────────────

std::string EmitVSOutput(InterstageVarying v)
{
    const VaryingDesc *d = GetVaryingDesc(v);
    assert(d && "Invalid InterstageVarying value");
    if (!d) return {};
    return EmitDecl(*d, /*is_vs_out=*/true);
}

std::string EmitFSInput(InterstageVarying v)
{
    const VaryingDesc *d = GetVaryingDesc(v);
    assert(d && "Invalid InterstageVarying value");
    if (!d) return {};
    return EmitDecl(*d, /*is_vs_out=*/false);
}

std::string EmitAllVSOutputs()
{
    std::string out;
    for (const VaryingDesc &d : kVaryingTable)
        out += EmitDecl(d, true);
    return out;
}

std::string EmitAllFSInputs()
{
    std::string out;
    for (const VaryingDesc &d : kVaryingTable)
        out += EmitDecl(d, false);
    return out;
}

} // namespace hgl::graph
