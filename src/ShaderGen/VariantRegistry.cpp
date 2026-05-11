#include<hgl/mtl/MaterialVariantRegistry.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include "BuiltinVariantEntry.h"
#include <hgl/mtl/MaterialVariantRow.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <initializer_list>
#include <string>

namespace hgl::graph::mtl{
namespace
{
#if defined(ULRE_SHADERGEN_VERBOSE)
constexpr bool kVariantRegistryVerbose = true;
#else
constexpr bool kVariantRegistryVerbose = false;
#endif

static MaterialVariantKey CanonicalizeRegistryLookupKey(const MaterialVariantKey &key,
                                                        const RegistryLookupOptions &options)
{
    MaterialVariantKey canon = key;

    if (!options.match_effective_feature_mask && canon.effective_feature_mask != 0)
    {
        static std::atomic_bool s_warned_effective_mask_ignored{false};
        bool expected = false;
        if (s_warned_effective_mask_ignored.compare_exchange_strong(expected, true, std::memory_order_relaxed))
        {
            std::fprintf(stderr,
                "[VariantRegistry] warning: effective_feature_mask is ignored for variant routing by default; "
                "set RegistryLookupOptions::match_effective_feature_mask = true to include it in registry lookup.\n");
        }

        canon.effective_feature_mask = 0;
    }

    return canon;
}

static std::string FormatVariantKeyForLog(const MaterialVariantKey &key)
{
    std::string text;
  text.reserve(320);

    text += "hash=";
    text += std::to_string(static_cast<unsigned long long>(key.Hash()));
    text += " ST=";
    text += std::to_string(static_cast<unsigned>(key.surface_type));
    text += " GM=";
    text += std::to_string(static_cast<unsigned>(key.geometry_mode));
    text += " blend=";
    text += std::to_string(static_cast<unsigned>(key.blend_mode));
    text += " pass=";
    text += std::to_string(static_cast<unsigned>(key.pass_hint));
    text += " sky=";
    text += std::to_string(static_cast<unsigned>(key.sky_ambient_model));
    text += " light=";
    text += std::to_string(static_cast<unsigned>(key.lighting_model));
    text += " eff=0x";

    char hex64[24] = {};
    std::snprintf(hex64, sizeof(hex64), "%016llX",
        static_cast<unsigned long long>(key.effective_feature_mask));
    text += hex64;
    text += " tex_bits=0x";

    char hex[16] = {};
    std::snprintf(hex, sizeof(hex), "%08X", key.texture_source_bits);
    text += hex;
    text += " sampler_bits=0x";
    std::snprintf(hex, sizeof(hex), "%08X", key.sampler_feature_bits);
    text += hex;
    text += " slots=[";

    for (size_t i = 0; i < SamplerSlotCount; ++i)
    {
        if (i > 0)
            text += ",";

        const SamplerSlot slot = static_cast<SamplerSlot>(i);
        text += SamplerSlotNameList[i];
        text += ":";
        text += std::to_string(static_cast<unsigned>(key.GetTextureSourceMode(slot)));
    }

    text += "]";
    text += " va_bits=0x";
    std::snprintf(hex, sizeof(hex), "%08X", key.vertex_attribute_feature_bits);
    text += hex;
    text += " extra_bits=0x";
    std::snprintf(hex, sizeof(hex), "%08X", key.extra_feature_bits);
    text += hex;
    return text;
}

static std::string FormatShaderStageFeatureDescForLog(const ShaderStageFeatureDesc &desc)
{
    std::string text;
    bool first = true;
    for (size_t i = 0; i < static_cast<size_t>(hgl::graph::VertexAttrib::RANGE_SIZE); ++i)
    {
        const auto attrib = static_cast<hgl::graph::VertexAttrib>(i);
        if (!desc.HasVertexAttrib(attrib))
            continue;

        if (!first)
            text += ",";

        text += GetVertexAttribName(attrib);
        text += "=1";
        first = false;
    }

    if (first)
        text = "None";

    text += ",dir=";
    text += desc.has_direction ? "1" : "0";
    text += ",clip=";
    text += desc.has_clip_pos ? "1" : "0";
    return text;
}

static std::string FormatMaterialResourcesForLog(const MaterialResourceRequirements &res)
{
    std::string text;
    text += "VP=";
    text += res.needs_viewport ? "1" : "0";
    text += ",Cam=";
    text += res.needs_camera ? "1" : "0";
    text += ",Sky=";
    text += res.needs_sky ? "1" : "0";
    text += ",Xf=";
    text += res.needs_transform ? "1" : "0";
    text += ",MI=";
    text += res.needs_material_instance ? "1" : "0";
    text += ",MITex=";
    text += res.needs_material_texture_index ? "1" : "0";
    text += ",Pal=";
    text += res.needs_color_palette ? "1" : "0";
    text += ",Lit=";
    text += res.enable_lighting ? "1" : "0";
    text += ",LM=";
    text += std::to_string(static_cast<unsigned>(res.lighting_model));
    text += ",SkyModel=";
    text += std::to_string(static_cast<unsigned>(res.sky_model));
    return text;
}

static std::string FormatRowTexturesForLog(const MaterialVariantRow &row)
{
    std::string text;
    if (row.texture_count == 0)
        return "None";

    for (uint32 i = 0; i < row.texture_count; ++i)
    {
        if (i > 0)
            text += ",";

        const auto &t = row.textures[i];
        text += SamplerSlotNameList[static_cast<size_t>(t.slot)];
        text += ":src=";
        text += std::to_string(static_cast<unsigned>(t.source_mode));
        text += ":smp=";
        text += std::to_string(static_cast<unsigned>(t.sampler_type));
    }

    return text;
}

static size_t CountBuiltinRowsByName(const char *name) noexcept
{
    if (!name || !name[0])
        return 0;

    size_t count = 0;
    for (size_t i = 0; i < kBuiltinVariantRowsCount; ++i)
    {
        const auto &row = kBuiltinVariantRows[i];
        if (row.name && std::string_view(row.name) == name)
            ++count;
    }

    return count;
}

static const MaterialVariantRow *FindBuiltinRowByName(const std::string &name) noexcept
{
    if (name.empty())
        return nullptr;

    for (size_t i = 0; i < kBuiltinVariantRowsCount; ++i)
    {
        const auto &row = kBuiltinVariantRows[i];
        if (row.name && name == row.name)
            return &row;
    }

    return nullptr;
}

static bool RowUsesStandardTexCoord(const MaterialVariantRow &row) noexcept
{
    return row.vs_features.HasVertexAttrib(VertexAttrib::TexCoord)
        || row.fs_features.HasVertexAttrib(VertexAttrib::TexCoord);
}

static bool RowUsesBillboardTexCoordPath(const MaterialVariantRow &row) noexcept
{
    return row.vertex_policy == VertexTransformPolicy::BillboardCameraFacing
        || row.vertex_policy == VertexTransformPolicy::BillboardAxisLocked;
}

static bool RowRequiresLightingKeyParity(const MaterialVariantRow &row) noexcept
{
    return row.resources.enable_lighting;
}

static bool RowRequiresSkyKeyParity(const MaterialVariantRow &row) noexcept
{
    return row.resources.needs_sky;
}

static TextureSourceMode GetRowTextureSourceMode(const MaterialVariantRow &row,
                                                 const SamplerSlot slot) noexcept
{
    for (uint32 i = 0; i < row.texture_count; ++i)
    {
        if (row.textures[i].slot == slot)
            return row.textures[i].source_mode;
    }

    return TextureSourceMode::None;
}

static uint32 BuildRowSamplerFeatureBits(const MaterialVariantRow &row) noexcept
{
    uint32 bits = 0;
    for (size_t i = 0; i < SamplerSlotCount; ++i)
    {
        const auto slot = static_cast<SamplerSlot>(i);
        if (GetRowTextureSourceMode(row, slot) != TextureSourceMode::None)
            bits |= SamplerFeatureBit(slot);
    }

    return bits;
}

static bool KeyUsesLegacyExtraFeatureBits(const MaterialVariantKey &key) noexcept
{
    return key.extra_feature_bits != 0;
}

static bool KeyUsesEffectiveFeatureMaskOverride(const MaterialVariantKey &key) noexcept
{
    return key.effective_feature_mask != 0;
}

static uint32 BuildRowVertexAttribFeatureBits(const MaterialVariantRow &row) noexcept
{
    uint32 bits = 0;

    for (size_t i = 0; i < static_cast<size_t>(VertexAttrib::RANGE_SIZE); ++i)
    {
        const auto attrib = static_cast<VertexAttrib>(i);
        if (row.vs_features.HasVertexAttrib(attrib) || row.fs_features.HasVertexAttrib(attrib))
            bits |= VertexAttribFeatureBit(attrib);
    }

    return bits;
}

static void ValidateBuiltinRowConsistency(const MaterialVariantRow &row,
                                          std::vector<std::string> &diagnostics)
{
    auto push = [&](const char *message)
    {
        std::string text = "Builtin row consistency failed: ";
        text += row.name ? row.name : "<unnamed>";
        text += " - ";
        text += message;
        diagnostics.emplace_back(std::move(text));
    };

    if (row.fs_features.has_direction && !row.vs_features.has_direction)
        push("fs_features.has_direction requires vs_features.has_direction");

    if (row.vs_features.has_clip_pos && !row.fs_features.has_clip_pos)
        push("vs_features.has_clip_pos is set but fs_features.has_clip_pos is not");

    if (RowUsesStandardTexCoord(row) && RowUsesBillboardTexCoordPath(row))
        push("standard TexCoord varyings must not be mixed with billboard texcoord path");

    if (row.resources.needs_material_texture_index && row.texture_count == 0)
        push("needs_material_texture_index requires at least one texture slot");

    if (row.resources.enable_lighting && !row.resources.needs_camera)
        push("enable_lighting requires needs_camera");

    if (row.resources.enable_lighting && !row.resources.needs_sky && row.surface_type != SurfaceType::Unlit)
        push("lit 3D row should declare needs_sky unless explicitly unlit");

    const bool has_textures = row.texture_count > 0;
    const bool has_standard_texcoord = RowUsesStandardTexCoord(row);
    if (has_textures && !RowUsesBillboardTexCoordPath(row) && !has_standard_texcoord)
        push("textured non-billboard row must declare TexCoord in VS/FS features");

    if (row.surface_type == SurfaceType::Sky && !row.vs_features.has_direction)
        push("sky row must emit direction from vertex stage");

    if (row.surface_type == SurfaceType::Sky && !row.fs_features.has_direction)
        push("sky row must consume direction in fragment stage");

    const size_t duplicate_name_count = CountBuiltinRowsByName(row.name);
    if (duplicate_name_count > 1)
        push("row name must be unique for exact builtin variant binding");
}

static void ValidateBuiltinVariantDescRowBinding(const MaterialVariantKey &key,
                                                 const MaterialVariantDesc &desc,
                                                 std::vector<std::string> &diagnostics)
{
    if (!desc.factory_type.has_value())
        return;

    if (desc.variant_name.empty())
    {
        diagnostics.emplace_back("Builtin variant descriptor binding failed: variant_name must not be empty when factory_type is set");
        return;
    }

    const MaterialVariantRow *row = FindBuiltinRowByName(desc.variant_name);
    if (!row)
    {
        std::string text = "Builtin variant descriptor binding failed: no MaterialVariantRow named '";
        text += desc.variant_name;
        text += "' for factory=";
        text += std::to_string(static_cast<unsigned>(*desc.factory_type));
        diagnostics.emplace_back(std::move(text));
        return;
    }

    if (row->factory_type != *desc.factory_type)
    {
        std::string text = "Builtin variant descriptor binding failed: row '";
        text += desc.variant_name;
        text += "' factory_type mismatch (row=";
        text += std::to_string(static_cast<unsigned>(row->factory_type));
        text += ", desc=";
        text += std::to_string(static_cast<unsigned>(*desc.factory_type));
        text += ")";
        diagnostics.emplace_back(std::move(text));
    }

    if (row->surface_type != key.surface_type
     || row->geometry_mode != key.geometry_mode
     || row->position_provider != key.position_provider
     || row->blend != key.blend_mode
     || row->pass != key.pass_hint)
    {
        std::string text = "Builtin variant descriptor binding failed: row '";
        text += desc.variant_name;
        text += "' structural identity mismatches registered key";
        diagnostics.emplace_back(std::move(text));
    }

    if (RowRequiresLightingKeyParity(*row) && row->resources.lighting_model != key.lighting_model)
    {
        std::string text = "Builtin variant descriptor binding failed: row '";
        text += desc.variant_name;
        text += "' lighting_model mirror mismatches key (row=";
        text += std::to_string(static_cast<unsigned>(row->resources.lighting_model));
        text += ", key=";
        text += std::to_string(static_cast<unsigned>(key.lighting_model));
        text += ")";
        diagnostics.emplace_back(std::move(text));
    }

    if (RowRequiresSkyKeyParity(*row) && row->resources.sky_model != key.sky_ambient_model)
    {
        std::string text = "Builtin variant descriptor binding failed: row '";
        text += desc.variant_name;
        text += "' sky_model mirror mismatches key (row=";
        text += std::to_string(static_cast<unsigned>(row->resources.sky_model));
        text += ", key=";
        text += std::to_string(static_cast<unsigned>(key.sky_ambient_model));
        text += ")";
        diagnostics.emplace_back(std::move(text));
    }

    for (size_t i = 0; i < SamplerSlotCount; ++i)
    {
        const auto slot = static_cast<SamplerSlot>(i);
        const TextureSourceMode row_mode = GetRowTextureSourceMode(*row, slot);
        const TextureSourceMode key_mode = key.GetTextureSourceMode(slot);
        if (row_mode == key_mode)
            continue;

        std::string text = "Builtin variant descriptor binding failed: row '";
        text += desc.variant_name;
        text += "' texture source mirror mismatches key for slot '";
        text += SamplerSlotNameList[i];
        text += "' (row=";
        text += std::to_string(static_cast<unsigned>(row_mode));
        text += ", key=";
        text += std::to_string(static_cast<unsigned>(key_mode));
        text += ")";
        diagnostics.emplace_back(std::move(text));
    }

    const uint32 row_sampler_bits = BuildRowSamplerFeatureBits(*row);
    if (row_sampler_bits != key.sampler_feature_bits)
    {
        std::string text = "Builtin variant descriptor binding failed: row '";
        text += desc.variant_name;
        text += "' sampler_feature_bits mirror mismatches key (row=";
        text += std::to_string(row_sampler_bits);
        text += ", key=";
        text += std::to_string(key.sampler_feature_bits);
        text += ")";
        diagnostics.emplace_back(std::move(text));
    }

    const uint32 row_vertex_bits = BuildRowVertexAttribFeatureBits(*row);
    const uint32 unsupported_key_vertex_bits = key.vertex_attribute_feature_bits & ~row_vertex_bits;
    if (unsupported_key_vertex_bits != 0)
    {
        std::string text = "Builtin variant descriptor binding failed: row '";
        text += desc.variant_name;
        text += "' vertex_attribute_feature_bits exceed row-declared attrib envelope (unsupported_bits=0x";

        char hex[16] = {};
        std::snprintf(hex, sizeof(hex), "%08X", unsupported_key_vertex_bits);
        text += hex;
        text += ")";
        diagnostics.emplace_back(std::move(text));
    }

    if (KeyUsesLegacyExtraFeatureBits(key))
    {
        std::string text = "Builtin variant descriptor binding failed: row '";
        text += desc.variant_name;
        text += "' carries legacy extra_feature_bits=0x";

        char hex[16] = {};
        std::snprintf(hex, sizeof(hex), "%08X", key.extra_feature_bits);
        text += hex;
        text += "; builtin row-driven paths should not depend on extra_feature_bits";
        diagnostics.emplace_back(std::move(text));
    }

    if (KeyUsesEffectiveFeatureMaskOverride(key))
    {
        std::string text = "Builtin variant descriptor binding failed: row '";
        text += desc.variant_name;
        text += "' carries effective_feature_mask override=0x";

        char hex64[24] = {};
        std::snprintf(hex64, sizeof(hex64), "%016llX", static_cast<unsigned long long>(key.effective_feature_mask));
        text += hex64;
        text += "; builtin row-driven paths should resolve through explicit row identity rather than effective_feature_mask overrides";
        diagnostics.emplace_back(std::move(text));
    }
}

static ShaderStageFeatureDesc MakeStageFeatures(std::initializer_list<hgl::graph::VertexAttrib> attribs,
                                                const bool has_direction = false,
                                                const bool has_clip_pos = false)
{
    ShaderStageFeatureDesc desc;
    for (const auto attrib : attribs)
        desc.SetVertexAttrib(attrib);
    desc.has_direction = has_direction;
    desc.has_clip_pos = has_clip_pos;
    return desc;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// VariantRegistry
// ---------------------------------------------------------------------------

void VariantRegistry::RegisterVariant(const MaterialVariantKey &key, const MaterialVariantDesc &desc)
{
    const uint64 hash = key.Hash();
    auto &bucket = variant_map[hash];

    for (const auto &existing : bucket)
    {
        if (!(existing.key == key))
            continue;

        const bool same_factory = existing.desc.factory_type == desc.factory_type;
        if (same_factory)
        {
            std::fprintf(stderr,
                "[VariantRegistry] duplicate variant ignored on RegisterVariant '%s' (hash=0x%016llx).\n",
                desc.variant_name.empty() ? "<unnamed>" : desc.variant_name.c_str(),
                static_cast<unsigned long long>(hash));
            return;
        }
    }

    bucket.push_back(VariantEntry{key, desc});
    ++variant_count;
}

const MaterialVariantDesc *VariantRegistry::QueryVariant(const MaterialVariantKey &key,
                                                         const RegistryLookupOptions &options) const
{
    const MaterialVariantKey query_key = CanonicalizeRegistryLookupKey(key, options);

    auto it = variant_map.find(query_key.Hash());
    if (it == variant_map.end())
        return nullptr;

    const auto *fallback = static_cast<const MaterialVariantDesc *>(nullptr);

    for (const auto &candidate : it->second)
    {
        if (!(candidate.key == query_key))
            continue;

        if (options.preferred_factory_type
         && candidate.desc.factory_type
         && *candidate.desc.factory_type == *options.preferred_factory_type)
        {
            return &candidate.desc;
        }

        if (!fallback)
            fallback = &candidate.desc;
    }

    return fallback;
}

const MaterialVariantDesc *VariantRegistry::QueryVariantWithCanonicalFallback(
    const MaterialVariantKey &key,
    MaterialVariantKey *resolved_key,
    const RegistryLookupOptions &options) const
{
    const MaterialVariantKey request_key = CanonicalizeRegistryLookupKey(key, options);

  if (kVariantRegistryVerbose && !(request_key == key))
  {
    std::fprintf(stderr,
      "[VariantRegistry] canonicalized lookup request={%s} canonical={%s}\n",
      FormatVariantKeyForLog(key).c_str(),
      FormatVariantKeyForLog(request_key).c_str());
  }

    if(const MaterialVariantDesc *exact=QueryVariant(request_key, options))
    {
        if (auto *s = GetGlobalVariantRegistryStatsSink())
            s->OnExactMatch(request_key, *exact);
        if (kVariantRegistryVerbose)
        {
            std::fprintf(stderr,
                "[VariantRegistry] exact-match variant=%s %s\n",
                exact->variant_name.empty() ? "<unnamed>" : exact->variant_name.c_str(),
                FormatVariantKeyForLog(request_key).c_str());
        }
        if(resolved_key)
            *resolved_key=request_key;
        return exact;
    }

    if (auto *s = GetGlobalVariantRegistryStatsSink())
        s->OnMiss(request_key);

    std::fprintf(stderr,
      "[VariantRegistry] miss request={%s}\n",
      FormatVariantKeyForLog(request_key).c_str());

    return nullptr;
}

bool VariantRegistry::ValidateBuiltinVariantTemplates(const std::string &shader_library_path,
                                                      std::vector<std::string> &diagnostics) const
{
    diagnostics.clear();

    for (size_t i = 0; i < kBuiltinVariantRowsCount; ++i)
        ValidateBuiltinRowConsistency(kBuiltinVariantRows[i], diagnostics);

    CompositorAssembler assembler(shader_library_path);

    for(const auto &[hash,bucket]:variant_map)
    {
        for(const auto &entry:bucket)
        {
            ValidateBuiltinVariantDescRowBinding(entry.key, entry.desc, diagnostics);

            const auto result=assembler.Assemble(entry.key,entry.desc);
            if(result.success)
                continue;

            std::string msg="Variant validation failed: ";
            msg += entry.desc.variant_name.empty()?"<unnamed>":entry.desc.variant_name;
            msg += " (hash=";
            msg += std::to_string(hash);
            msg += ")";

            if(!result.error_message.empty())
            {
                msg += " - ";
                msg += result.error_message;
            }

            diagnostics.emplace_back(std::move(msg));
        }
    }

    return diagnostics.empty();
}

std::string VariantRegistry::DumpSnapshot() const
{
    std::vector<std::pair<uint64, const VariantEntry *>> rows;
    rows.reserve(variant_count);

    for(const auto &[hash,bucket]:variant_map)
        for (const auto &entry : bucket)
            rows.emplace_back(hash,&entry);

    std::sort(rows.begin(),rows.end(),
        [](const auto &a,const auto &b)
        {
            return a.first<b.first;
        });

    std::string out;
    out.reserve(rows.size()*120);

    out += "# VariantRegistry Snapshot\n";
    out += "hash|name|factory|surface|geometry|tex_modes|tex_bits|sampler_bits|va_bits|extra_bits|blend|pass|row_vertex_input|row_vertex_policy|row_surface_model|row_vs_features|row_fs_features|row_resources|row_schema|row_def_hint|row_textures\n";

    for(const auto &[hash,entry_ptr]:rows)
    {
        const auto &entry=*entry_ptr;
        const auto &k=entry.key;
        const auto &d=entry.desc;

        const MaterialVariantRow *row_ptr = nullptr;
        for (size_t i = 0; i < kBuiltinVariantRowsCount; ++i)
        {
            if (d.variant_name == kBuiltinVariantRows[i].name)
            {
                row_ptr = &kBuiltinVariantRows[i];
                break;
            }
        }

        out += std::to_string(hash);
        out += "|";
        out += d.variant_name;
        out += "|";
        out += d.factory_type
            ? std::to_string(static_cast<uint32>(*d.factory_type))
            : std::string("-1");
        out += "|";
        out += std::to_string(static_cast<uint32>(k.surface_type));
        out += "|";
        out += std::to_string(static_cast<uint32>(k.geometry_mode));
        out += "|";
        {
            bool first = true;
            for (size_t i = 0; i < SamplerSlotCount; ++i)
            {
                const SamplerSlot slot = static_cast<SamplerSlot>(i);
                const TextureSourceMode mode = k.GetTextureSourceMode(slot);
                if (mode != TextureSourceMode::None)
                {
                    if (!first)
                        out += ",";
                    out += SamplerSlotNameList[i];
                    out += ":";
                    out += std::to_string(static_cast<uint32>(mode));
                    first = false;
                }
            }
            if (first)
                out += "None";
            out += "|";
        }
        out += std::to_string(k.texture_source_bits);
        out += "|";
        out += std::to_string(k.sampler_feature_bits);
        out += "|";
        out += std::to_string(k.vertex_attribute_feature_bits);
        out += "|";
        out += std::to_string(k.extra_feature_bits);
        out += "|";
        out += std::to_string(static_cast<uint32>(k.blend_mode));
        out += "|";
        out += std::to_string(static_cast<uint32>(k.pass_hint));
        out += "|";
        out += row_ptr ? GetVertexInputProfileName(row_ptr->vertex_input) : "Unknown";
        out += "|";
        out += row_ptr ? GetVertexTransformPolicyName(row_ptr->vertex_policy) : "Unknown";
        out += "|";
        out += row_ptr ? GetSurfaceShadingModelName(row_ptr->surface_model) : "Unknown";
        out += "|";
        out += row_ptr ? FormatShaderStageFeatureDescForLog(row_ptr->vs_features) : "";
        out += "|";
        out += row_ptr ? FormatShaderStageFeatureDescForLog(row_ptr->fs_features) : "";
        out += "|";
        out += row_ptr ? FormatMaterialResourcesForLog(row_ptr->resources) : "";
        out += "|";
        out += row_ptr ? std::to_string(static_cast<uint32>(row_ptr->schema)) : "0";
        out += "|";
        out += row_ptr ? GetStaticMaterialDefIdHintName(row_ptr->def_hint) : "None";
        out += "|";
        out += row_ptr ? FormatRowTexturesForLog(*row_ptr) : "None";
        out += "\n";
    }

    return out;
}

std::string VariantRegistry::DumpBuiltinRowSnapshot() const
{
    std::string out;
    out.reserve(kBuiltinVariantRowsCount * 240);

    out += "# Builtin MaterialVariantRow Snapshot\n";
    out += "name|preset|factory|primitive|surface|geometry|position_provider|vertex_input|vertex_policy|surface_model|blend|pass|vs_template|fs_template|surface_path|vs_features|fs_features|resources|schema|def_hint|textures\n";

    for (size_t i = 0; i < kBuiltinVariantRowsCount; ++i)
    {
        const MaterialVariantRow &row = kBuiltinVariantRows[i];

        out += row.name ? row.name : "";
        out += "|";
        out += std::to_string(static_cast<uint32>(row.preset));
        out += "|";
        out += std::to_string(static_cast<uint32>(row.factory_type));
        out += "|";
        out += std::to_string(static_cast<uint32>(row.primitive));
        out += "|";
        out += std::to_string(static_cast<uint32>(row.surface_type));
        out += "|";
        out += std::to_string(static_cast<uint32>(row.geometry_mode));
        out += "|";
        out += std::to_string(static_cast<uint32>(row.position_provider));
        out += "|";
        out += GetVertexInputProfileName(row.vertex_input);
        out += "|";
        out += GetVertexTransformPolicyName(row.vertex_policy);
        out += "|";
        out += GetSurfaceShadingModelName(row.surface_model);
        out += "|";
        out += std::to_string(static_cast<uint32>(row.blend));
        out += "|";
        out += std::to_string(static_cast<uint32>(row.pass));
        out += "|";
        out += row.vs_template_path ? row.vs_template_path : "";
        out += "|";
        out += row.fs_template_path ? row.fs_template_path : "";
        out += "|";
        out += row.surface_path ? row.surface_path : "";
        out += "|";
        out += FormatShaderStageFeatureDescForLog(row.vs_features);
        out += "|";
        out += FormatShaderStageFeatureDescForLog(row.fs_features);
        out += "|";
        out += FormatMaterialResourcesForLog(row.resources);
        out += "|";
        out += std::to_string(static_cast<uint32>(row.schema));
        out += "|";
        out += GetStaticMaterialDefIdHintName(row.def_hint);
        out += "|";
        out += FormatRowTexturesForLog(row);
        out += "\n";
    }

    return out;
}

void VariantRegistry::ForEachBuiltinRow(std::function<void(const MaterialVariantRow &)> cb) const
{
    for (size_t i = 0; i < kBuiltinVariantRowsCount; ++i)
    {
        cb(kBuiltinVariantRows[i]);
    }
}

void VariantRegistry::ForEach(
    std::function<void(const MaterialVariantKey &, const MaterialVariantDesc &)> cb) const
{
    for (const auto &[hash, bucket] : variant_map)
        for (const auto &entry : bucket)
            cb(entry.key, entry.desc);
}

// ---------------------------------------------------------------------------
// Built-in variant table  (B' — C++20 designated-initializer flat table)
// Struct definition and BuildKey()/BuildDesc() live in BuiltinVariantEntry.h.
// ---------------------------------------------------------------------------

// Convenience aliases (TU-local; do not pollute hgl::graph::mtl public API).
namespace {
using ST   = _BVE_ST;
using GM   = _BVE_GM;
using TSM  = _BVE_TSM;
using LM   = _BVE_LM;
using RM   = _BVE_RM;
using PT   = _BVE_PT;
using Slot = _BVE_Slot;
constexpr auto VA = _BVE_VA;
} // anonymous (aliases only)

// clang-format off
const BuiltinVariantEntry kBuiltinVariants[] =
{
    // ── 2D ──────────────────────────────────────────────────────────────────────────────────────
    { .name = "VertexColor2D",      .preset = MaterialPreset::VertexColor2D,
      .geometry_mode = GM::Quad2D,  .position_provider = PositionProviderId::VAB_Vec2, .vertex_bits = VA(VertexAttrib::Color),
      .vs_path = "2d/vertexcolor2d.vert.glsl",  .fs_path = "2d/vertexcolor2d.frag.glsl"  },

    { .name = "PureColor2D",        .preset = MaterialPreset::PureColor2D,
      .geometry_mode = GM::Quad2D,  .position_provider = PositionProviderId::VAB_Vec2,
      .vs_path = "2d/purecolor2d.vert.glsl",    .fs_path = "2d/purecolor2d.frag.glsl"    },

    { .name = "PureTexture2D",      .preset = MaterialPreset::PureTexture2D,
      .geometry_mode = GM::Quad2D,  .position_provider = PositionProviderId::VAB_Vec2, .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "2d/puretexture2d.vert.glsl",  .fs_path = "2d/puretexture2d.frag.glsl"  },

    { .name = "PureTexture2DArray", .preset = MaterialPreset::PureTexture2D,
      .geometry_mode = GM::Quad2D,  .position_provider = PositionProviderId::VAB_Vec2, .tex = {{ Slot::BaseColor, TSM::Array }},
      .vs_path = "2d/puretexture2d.vert.glsl",  .fs_path = "2d/puretexture2d.frag.glsl"  },

    { .name = "Text2D",             .preset = MaterialPreset::Text2D,
      .geometry_mode = GM::Quad2D,  .position_provider = PositionProviderId::VAB_Vec2, .tex = {{ Slot::BaseColor, TSM::Atlas }},
      .vs_path = "2d/text2d.vert.glsl",         .fs_path = "2d/text2d.frag.glsl"         },

    // ── 3D Unlit ────────────────────────────────────────────────────────────────────────────────
    { .name = "PureColor3D",        .preset = MaterialPreset::PureColor3D },

    { .name = "VertexColor3D",      .preset = MaterialPreset::VertexColor3D,
      .vertex_bits = VA(VertexAttrib::Color),
      .surface_path = "surface/unlit_vertexcolor_surface.glsl" },

    { .name = "VertexLuminance3D",  .preset = MaterialPreset::VertexLuminance3D,
      .vertex_bits = VA(VertexAttrib::Luminance),
      .surface_path = "surface/unlit_luminance_surface.glsl"   },

    { .name = "VertexLuminance2D",  .preset = MaterialPreset::VertexLuminance2D,
      .position_provider = PositionProviderId::VAB_Vec2,
      .vertex_bits = VA(VertexAttrib::Luminance),
      .surface_path = "surface/unlit_luminance_surface.glsl"   },

    { .name = "VertexPaletteColor3D", .preset = MaterialPreset::VertexPaletteColor3D,
      .vertex_bits = VA(VertexAttrib::Color),
      .vs_path = "compositor/main_forward_unlit_palette.vert.glsl",
      .surface_path = "surface/unlit_vertexcolor_surface.glsl" },

    { .name = "Gizmo3D",            .preset = MaterialPreset::Gizmo3D,
      .surface_path = "surface/gizmo3d_surface.glsl" },

    // ── Billboard  (2 geometries × 5 blend modes) ───────────────────────────────────────────────
    { .name = "Billboard2DDynamicOpaque",   .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::Opaque,          .pass = PT::ForwardOpaque,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DDynamic",         .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::Transparent,     .pass = PT::ForwardTransparent,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DDynamicMasked",   .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::Masked,          .pass = PT::ForwardMasked,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DDynamicDither",   .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::Dither,          .pass = PT::ForwardDither,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DDynamicA2C",      .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::AlphaToCoverage, .pass = PT::ForwardA2C,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixedOpaque",     .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::Opaque,          .pass = PT::ForwardOpaque,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixed",           .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::Transparent,     .pass = PT::ForwardTransparent,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixedMasked",     .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::Masked,          .pass = PT::ForwardMasked,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixedDither",     .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::Dither,          .pass = PT::ForwardDither,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixedA2C",        .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::AlphaToCoverage, .pass = PT::ForwardA2C,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    // ── Terrain / Sky ────────────────────────────────────────────────────────────────────────────
    { .name = "TerrainGrid",  .preset = MaterialPreset::TerrainGrid,
      .surface_type = ST::Terrain,
      .vs_path = "compositor/main_terrain_grid.vert.glsl",
      .surface_path = "surface/terrain_grid_surface.glsl"  },

    { .name = "SkyMinimal",   .preset = MaterialPreset::SkyMinimal,
      .surface_type = ST::Sky,
      .surface_path = "surface/sky_minimal_surface.glsl"   },

    // ── Standard 3D Lit  (texture-based, BaseColor + Normal) ────────────────────────────────────
    { .name = "Standard",              .preset = MaterialPreset::Standard,
      .surface_type = ST::Standard, .lighting = LM::Lambert,
      .tex = {{ Slot::BaseColor, TSM::Simple }, { Slot::Normal, TSM::Simple }},
      .surface_path = "surface/standard_surface.glsl"           },

    { .name = "StandardBlinnPhong",    .preset = MaterialPreset::Standard,
      .surface_type = ST::Standard, .lighting = LM::BlinnPhong,
      .tex = {{ Slot::BaseColor, TSM::Simple }, { Slot::Normal, TSM::Simple }},
      .surface_path = "surface/textureblinnphong_surface.glsl"  },

    { .name = "StandardPBR",           .preset = MaterialPreset::Standard,
      .surface_type = ST::Standard, .lighting = LM::PBR,
      .tex = {{ Slot::BaseColor, TSM::Simple }, { Slot::Normal, TSM::Simple }},
      .surface_path = "surface/standard_surface.glsl"           },

    { .name = "StandardTextureArray",  .preset = MaterialPreset::Standard,
      .surface_type = ST::Standard, .lighting = LM::Lambert,
      .tex = {{ Slot::BaseColor, TSM::Array },  { Slot::Normal, TSM::Array  }},
      .surface_path = "surface/standard_surface.glsl"           },

    { .name = "StandardBlinnPhongArray", .preset = MaterialPreset::Standard,
      .surface_type = ST::Standard, .lighting = LM::BlinnPhong,
      .tex = {{ Slot::BaseColor, TSM::Array },  { Slot::Normal, TSM::Array  }},
      .surface_path = "surface/textureblinnphong_surface.glsl"  },

    { .name = "StandardPBRArray",      .preset = MaterialPreset::Standard,
      .surface_type = ST::Standard, .lighting = LM::PBR,
      .tex = {{ Slot::BaseColor, TSM::Array },  { Slot::Normal, TSM::Array  }},
      .surface_path = "surface/standard_surface.glsl"           },

    // ── PBR color-only ───────────────────────────────────────────────────────────────────────────
    { .name = "PBRColor3D",  .preset = MaterialPreset::PBRColor3D,
      .surface_type = ST::Standard, .lighting = LM::PBR,
      .surface_path = "surface/pbrcolor3d_surface.glsl" },

    // ── PCG / Fullscreen ─────────────────────────────────────────────────────────────────────────
    { .name = "FullscreenTriangle", .preset = MaterialPreset::FullscreenTriangle,
      .position_provider = PositionProviderId::PCG_FullscreenTriangle },
};
// clang-format on

const size_t kBuiltinVariantsCount = std::size(kBuiltinVariants);

// clang-format off
const MaterialVariantRow kBuiltinVariantRows[] =
{
    { .name = "VertexColor2D", .preset = MaterialPreset::VertexColor2D, .factory_type = MaterialPreset::VertexColor2D,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::Quad2D, .position_provider = PositionProviderId::VAB_Vec2,
      .vertex_input = VertexInputProfile::Position2D, .vertex_policy = VertexTransformPolicy::Quad2D, .surface_model = SurfaceShadingModel::VertexColor,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "2d/vertexcolor2d.vert.glsl", .fs_template_path = "2d/vertexcolor2d.frag.glsl", .surface_path = "",
      .vs_features = { true, false, false, true, false, false, false, false }, .fs_features = { false, false, false, true, false, false, false, false },
      .resources = { false, false, false, false, false, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .schema = ShaderDataSchema::None, .def_hint = StaticMaterialDefIdHint::Standard2D },

    { .name = "PureColor2D", .preset = MaterialPreset::PureColor2D, .factory_type = MaterialPreset::PureColor2D,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::Quad2D, .position_provider = PositionProviderId::VAB_Vec2,
      .vertex_input = VertexInputProfile::Position2D, .vertex_policy = VertexTransformPolicy::Quad2D, .surface_model = SurfaceShadingModel::PureColor,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "2d/purecolor2d.vert.glsl", .fs_template_path = "2d/purecolor2d.frag.glsl", .surface_path = "",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position }), .fs_features = MakeStageFeatures({ VertexAttrib::Position }),
      .resources = { false, false, false, false, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .schema = ShaderDataSchema::Color4f, .def_hint = StaticMaterialDefIdHint::Standard2D },

    { .name = "PureTexture2D", .preset = MaterialPreset::PureTexture2D, .factory_type = MaterialPreset::PureTexture2D,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::Quad2D, .position_provider = PositionProviderId::VAB_Vec2,
      .vertex_input = VertexInputProfile::PositionTexCoord2D, .vertex_policy = VertexTransformPolicy::Quad2D, .surface_model = SurfaceShadingModel::Texture2D,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "2d/puretexture2d.vert.glsl", .fs_template_path = "2d/puretexture2d.frag.glsl", .surface_path = "",
      .vs_features = { true, false, false, false, false, true, false, false }, .fs_features = { false, false, false, false, false, true, false, false },
      .resources = { false, false, false, false, false, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::None, .def_hint = StaticMaterialDefIdHint::Standard2D },

    { .name = "PureTexture2DArray", .preset = MaterialPreset::PureTexture2D, .factory_type = MaterialPreset::PureTexture2D,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::Quad2D, .position_provider = PositionProviderId::VAB_Vec2,
      .vertex_input = VertexInputProfile::PositionTexCoord2D, .vertex_policy = VertexTransformPolicy::Quad2D, .surface_model = SurfaceShadingModel::Texture2D,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "2d/puretexture2d.vert.glsl", .fs_template_path = "2d/puretexture2d.frag.glsl", .surface_path = "",
      .vs_features = { true, false, false, false, false, true, false, false }, .fs_features = { false, false, false, false, false, true, false, false },
      .resources = { false, false, false, false, false, true, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Array, SamplerType::Sampler2DArray, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::None, .def_hint = StaticMaterialDefIdHint::Standard2D },

    { .name = "Text2D", .preset = MaterialPreset::Text2D, .factory_type = MaterialPreset::Text2D,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::Quad2D, .position_provider = PositionProviderId::VAB_Vec2,
      .vertex_input = VertexInputProfile::PositionTexCoord2D, .vertex_policy = VertexTransformPolicy::Text2D, .surface_model = SurfaceShadingModel::Text,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "2d/text2d.vert.glsl", .fs_template_path = "2d/text2d.frag.glsl", .surface_path = "",
      .vs_features = { true, false, false, false, false, true, false, false }, .fs_features = { false, false, false, false, false, true, false, false },
      .resources = { false, false, false, false, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::Text, TextureSourceMode::Atlas, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::TextColor, .def_hint = StaticMaterialDefIdHint::Text2D },

    { .name = "PureColor3D", .preset = MaterialPreset::PureColor3D, .factory_type = MaterialPreset::PureColor3D,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::Position3D, .vertex_policy = VertexTransformPolicy::Mesh3D, .surface_model = SurfaceShadingModel::PureColor,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "",
      .vs_features = { true, false, false, false, false, false, false, false }, .fs_features = { true, false, false, false, false, false, false, false },
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .schema = ShaderDataSchema::Color4f, .def_hint = StaticMaterialDefIdHint::PureColor3D },

    { .name = "VertexColor3D", .preset = MaterialPreset::VertexColor3D, .factory_type = MaterialPreset::VertexColor3D,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::PositionColor3D, .vertex_policy = VertexTransformPolicy::Mesh3D, .surface_model = SurfaceShadingModel::VertexColor,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "surface/unlit_vertexcolor_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Color }), .fs_features = MakeStageFeatures({ VertexAttrib::Color }),
      .resources = { true, true, false, true, false, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .schema = ShaderDataSchema::None, .def_hint = StaticMaterialDefIdHint::VertexColor3D },

    { .name = "VertexLuminance3D", .preset = MaterialPreset::VertexLuminance3D, .factory_type = MaterialPreset::VertexLuminance3D,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::PositionLuminance3D, .vertex_policy = VertexTransformPolicy::Mesh3D, .surface_model = SurfaceShadingModel::VertexLuminance,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "surface/unlit_luminance_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Luminance }), .fs_features = MakeStageFeatures({ VertexAttrib::Luminance }),
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .schema = ShaderDataSchema::Color4f, .def_hint = StaticMaterialDefIdHint::VertexLuminance3D },

    { .name = "VertexLuminance2D", .preset = MaterialPreset::VertexLuminance2D, .factory_type = MaterialPreset::VertexLuminance2D,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::VAB_Vec2,
      .vertex_input = VertexInputProfile::PositionLuminance2D, .vertex_policy = VertexTransformPolicy::Quad2D, .surface_model = SurfaceShadingModel::VertexLuminance,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "surface/unlit_luminance_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Luminance }), .fs_features = MakeStageFeatures({ VertexAttrib::Luminance }),
      .resources = { false, false, false, false, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .schema = ShaderDataSchema::Color4f, .def_hint = StaticMaterialDefIdHint::Standard2D },

    { .name = "VertexPaletteColor3D", .preset = MaterialPreset::VertexPaletteColor3D, .factory_type = MaterialPreset::VertexPaletteColor3D,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::PositionPaletteIndex3D, .vertex_policy = VertexTransformPolicy::Mesh3D, .surface_model = SurfaceShadingModel::VertexColor,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "compositor/main_forward_unlit_palette.vert.glsl", .fs_template_path = "", .surface_path = "surface/unlit_vertexcolor_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Color }), .fs_features = MakeStageFeatures({ VertexAttrib::Color }),
      .resources = { true, true, false, true, false, false, true, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .schema = ShaderDataSchema::None, .def_hint = StaticMaterialDefIdHint::VertexPaletteColor3D },

    { .name = "Gizmo3D", .preset = MaterialPreset::Gizmo3D, .factory_type = MaterialPreset::Gizmo3D,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::PositionNormal3D, .vertex_policy = VertexTransformPolicy::Mesh3D, .surface_model = SurfaceShadingModel::Gizmo,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "surface/gizmo3d_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal }), .fs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal }),
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .schema = ShaderDataSchema::Color4f, .def_hint = StaticMaterialDefIdHint::Gizmo3D },

    { .name = "Billboard2DDynamicOpaque", .preset = MaterialPreset::Billboard2DDynamic, .factory_type = MaterialPreset::Billboard2DDynamic,
      .primitive = PrimitiveType::Billboard, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::BillboardCameraFacing, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::BillboardPositionOnly3D, .vertex_policy = VertexTransformPolicy::BillboardCameraFacing, .surface_model = SurfaceShadingModel::BillboardTexture,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "compositor/main_forward_billboard_dynamic.vert.glsl", .fs_template_path = "", .surface_path = "surface/billboard_texture_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position }), .fs_features = MakeStageFeatures({ VertexAttrib::TexCoord }),
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::BillboardSizeUVec2, .def_hint = StaticMaterialDefIdHint::BillboardDynamic },

    { .name = "Billboard2DDynamic", .preset = MaterialPreset::Billboard2DDynamic, .factory_type = MaterialPreset::Billboard2DDynamic,
      .primitive = PrimitiveType::Billboard, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::BillboardCameraFacing, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::BillboardPositionOnly3D, .vertex_policy = VertexTransformPolicy::BillboardCameraFacing, .surface_model = SurfaceShadingModel::BillboardTexture,
      .blend = RenderAlphaMode::Transparent, .pass = PassType::ForwardTransparent,
      .vs_template_path = "compositor/main_forward_billboard_dynamic.vert.glsl", .fs_template_path = "", .surface_path = "surface/billboard_texture_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position }), .fs_features = MakeStageFeatures({ VertexAttrib::TexCoord }),
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::BillboardSizeUVec2, .def_hint = StaticMaterialDefIdHint::BillboardDynamic },

    { .name = "Billboard2DDynamicMasked", .preset = MaterialPreset::Billboard2DDynamic, .factory_type = MaterialPreset::Billboard2DDynamic,
      .primitive = PrimitiveType::Billboard, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::BillboardCameraFacing, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::BillboardPositionOnly3D, .vertex_policy = VertexTransformPolicy::BillboardCameraFacing, .surface_model = SurfaceShadingModel::BillboardTexture,
      .blend = RenderAlphaMode::Masked, .pass = PassType::ForwardMasked,
      .vs_template_path = "compositor/main_forward_billboard_dynamic.vert.glsl", .fs_template_path = "", .surface_path = "surface/billboard_texture_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position }), .fs_features = MakeStageFeatures({ VertexAttrib::TexCoord }),
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::BillboardSizeUVec2, .def_hint = StaticMaterialDefIdHint::BillboardDynamic },

    { .name = "Billboard2DDynamicDither", .preset = MaterialPreset::Billboard2DDynamic, .factory_type = MaterialPreset::Billboard2DDynamic,
      .primitive = PrimitiveType::Billboard, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::BillboardCameraFacing, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::BillboardPositionOnly3D, .vertex_policy = VertexTransformPolicy::BillboardCameraFacing, .surface_model = SurfaceShadingModel::BillboardTexture,
      .blend = RenderAlphaMode::Dither, .pass = PassType::ForwardDither,
      .vs_template_path = "compositor/main_forward_billboard_dynamic.vert.glsl", .fs_template_path = "", .surface_path = "surface/billboard_texture_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position }), .fs_features = MakeStageFeatures({ VertexAttrib::TexCoord }),
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::BillboardSizeUVec2, .def_hint = StaticMaterialDefIdHint::BillboardDynamic },

    { .name = "Billboard2DDynamicA2C", .preset = MaterialPreset::Billboard2DDynamic, .factory_type = MaterialPreset::Billboard2DDynamic,
      .primitive = PrimitiveType::Billboard, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::BillboardCameraFacing, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::BillboardPositionOnly3D, .vertex_policy = VertexTransformPolicy::BillboardCameraFacing, .surface_model = SurfaceShadingModel::BillboardTexture,
      .blend = RenderAlphaMode::AlphaToCoverage, .pass = PassType::ForwardA2C,
      .vs_template_path = "compositor/main_forward_billboard_dynamic.vert.glsl", .fs_template_path = "", .surface_path = "surface/billboard_texture_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position }), .fs_features = MakeStageFeatures({ VertexAttrib::TexCoord }),
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::BillboardSizeUVec2, .def_hint = StaticMaterialDefIdHint::BillboardDynamic },

    { .name = "Billboard2DFixedOpaque", .preset = MaterialPreset::Billboard2DFixed, .factory_type = MaterialPreset::Billboard2DFixed,
      .primitive = PrimitiveType::Billboard, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::BillboardAxisLocked, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::BillboardPositionOnly3D, .vertex_policy = VertexTransformPolicy::BillboardAxisLocked, .surface_model = SurfaceShadingModel::BillboardTexture,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "compositor/main_forward_billboard_fixed.vert.glsl", .fs_template_path = "", .surface_path = "surface/billboard_texture_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position }), .fs_features = MakeStageFeatures({ VertexAttrib::TexCoord }),
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::BillboardSizeUVec2, .def_hint = StaticMaterialDefIdHint::BillboardFixed },

    { .name = "Billboard2DFixed", .preset = MaterialPreset::Billboard2DFixed, .factory_type = MaterialPreset::Billboard2DFixed,
      .primitive = PrimitiveType::Billboard, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::BillboardAxisLocked, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::BillboardPositionOnly3D, .vertex_policy = VertexTransformPolicy::BillboardAxisLocked, .surface_model = SurfaceShadingModel::BillboardTexture,
      .blend = RenderAlphaMode::Transparent, .pass = PassType::ForwardTransparent,
      .vs_template_path = "compositor/main_forward_billboard_fixed.vert.glsl", .fs_template_path = "", .surface_path = "surface/billboard_texture_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position }), .fs_features = MakeStageFeatures({ VertexAttrib::TexCoord }),
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::BillboardSizeUVec2, .def_hint = StaticMaterialDefIdHint::BillboardFixed },

    { .name = "Billboard2DFixedMasked", .preset = MaterialPreset::Billboard2DFixed, .factory_type = MaterialPreset::Billboard2DFixed,
      .primitive = PrimitiveType::Billboard, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::BillboardAxisLocked, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::BillboardPositionOnly3D, .vertex_policy = VertexTransformPolicy::BillboardAxisLocked, .surface_model = SurfaceShadingModel::BillboardTexture,
      .blend = RenderAlphaMode::Masked, .pass = PassType::ForwardMasked,
      .vs_template_path = "compositor/main_forward_billboard_fixed.vert.glsl", .fs_template_path = "", .surface_path = "surface/billboard_texture_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position }), .fs_features = MakeStageFeatures({ VertexAttrib::TexCoord }),
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::BillboardSizeUVec2, .def_hint = StaticMaterialDefIdHint::BillboardFixed },

    { .name = "Billboard2DFixedDither", .preset = MaterialPreset::Billboard2DFixed, .factory_type = MaterialPreset::Billboard2DFixed,
      .primitive = PrimitiveType::Billboard, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::BillboardAxisLocked, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::BillboardPositionOnly3D, .vertex_policy = VertexTransformPolicy::BillboardAxisLocked, .surface_model = SurfaceShadingModel::BillboardTexture,
      .blend = RenderAlphaMode::Dither, .pass = PassType::ForwardDither,
      .vs_template_path = "compositor/main_forward_billboard_fixed.vert.glsl", .fs_template_path = "", .surface_path = "surface/billboard_texture_surface.glsl",
      .vs_features = { true, false, false, false, false, false, false, false }, .fs_features = { false, false, false, false, false, true, false, false },
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::BillboardSizeUVec2, .def_hint = StaticMaterialDefIdHint::BillboardFixed },

    { .name = "Billboard2DFixedA2C", .preset = MaterialPreset::Billboard2DFixed, .factory_type = MaterialPreset::Billboard2DFixed,
      .primitive = PrimitiveType::Billboard, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::BillboardAxisLocked, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::BillboardPositionOnly3D, .vertex_policy = VertexTransformPolicy::BillboardAxisLocked, .surface_model = SurfaceShadingModel::BillboardTexture,
      .blend = RenderAlphaMode::AlphaToCoverage, .pass = PassType::ForwardA2C,
      .vs_template_path = "compositor/main_forward_billboard_fixed.vert.glsl", .fs_template_path = "", .surface_path = "surface/billboard_texture_surface.glsl",
      .vs_features = { true, false, false, false, false, false, false, false }, .fs_features = { false, false, false, false, false, true, false, false },
      .resources = { true, true, false, true, true, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 1,
      .schema = ShaderDataSchema::BillboardSizeUVec2, .def_hint = StaticMaterialDefIdHint::BillboardFixed },

    { .name = "TerrainGrid", .preset = MaterialPreset::TerrainGrid, .factory_type = MaterialPreset::TerrainGrid,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Terrain, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::Position3D, .vertex_policy = VertexTransformPolicy::TerrainGrid, .surface_model = SurfaceShadingModel::TerrainGrid,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "compositor/main_terrain_grid.vert.glsl", .fs_template_path = "", .surface_path = "surface/terrain_grid_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position }), .fs_features = MakeStageFeatures({ VertexAttrib::Normal }, false, true),
      .resources = { true, true, false, true, false, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::Height, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA }, { SamplerSlot::Normal, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 2,
      .schema = ShaderDataSchema::None, .def_hint = StaticMaterialDefIdHint::TerrainGrid },

    { .name = "SkyMinimal", .preset = MaterialPreset::SkyMinimal, .factory_type = MaterialPreset::SkyMinimal,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Sky, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::Position3D, .vertex_policy = VertexTransformPolicy::Sky, .surface_model = SurfaceShadingModel::SkyMinimal,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "surface/sky_minimal_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position }, true, false), .fs_features = MakeStageFeatures({}, true, false),
      .resources = { true, true, true, true, false, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .schema = ShaderDataSchema::None, .def_hint = StaticMaterialDefIdHint::SkyMinimal },

    { .name = "Standard", .preset = MaterialPreset::Standard, .factory_type = MaterialPreset::Standard,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Standard, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::PositionNormal3D, .vertex_policy = VertexTransformPolicy::Mesh3D, .surface_model = SurfaceShadingModel::StandardLambert,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "surface/standard_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal, VertexAttrib::TexCoord }), .fs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal, VertexAttrib::TexCoord }),
      .resources = { true, true, true, true, true, false, false, true, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA }, { SamplerSlot::Normal, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 2,
      .schema = ShaderDataSchema::StandardParams, .def_hint = StaticMaterialDefIdHint::Standard3D },

    { .name = "StandardBlinnPhong", .preset = MaterialPreset::Standard, .factory_type = MaterialPreset::Standard,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Standard, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::PositionNormal3D, .vertex_policy = VertexTransformPolicy::Mesh3D, .surface_model = SurfaceShadingModel::StandardBlinnPhong,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "surface/textureblinnphong_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal, VertexAttrib::TexCoord }), .fs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal, VertexAttrib::TexCoord }),
      .resources = { true, true, true, true, true, false, false, true, LightingModel::BlinnPhong, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA }, { SamplerSlot::Normal, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 2,
      .schema = ShaderDataSchema::StandardParams, .def_hint = StaticMaterialDefIdHint::Standard3D },

    { .name = "StandardPBR", .preset = MaterialPreset::Standard, .factory_type = MaterialPreset::Standard,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Standard, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::PositionNormal3D, .vertex_policy = VertexTransformPolicy::Mesh3D, .surface_model = SurfaceShadingModel::StandardPBR,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "surface/standard_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal, VertexAttrib::TexCoord }), .fs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal, VertexAttrib::TexCoord }),
      .resources = { true, true, true, true, true, false, false, true, LightingModel::PBR, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA }, { SamplerSlot::Normal, TextureSourceMode::Simple, SamplerType::Sampler2D, TextureChannelHint::RGBA } }, .texture_count = 2,
      .schema = ShaderDataSchema::StandardParams, .def_hint = StaticMaterialDefIdHint::Standard3D },

    { .name = "StandardTextureArray", .preset = MaterialPreset::Standard, .factory_type = MaterialPreset::Standard,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Standard, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::PositionNormal3D, .vertex_policy = VertexTransformPolicy::Mesh3D, .surface_model = SurfaceShadingModel::StandardLambert,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "surface/standard_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal, VertexAttrib::TexCoord }), .fs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal, VertexAttrib::TexCoord }),
      .resources = { true, true, true, true, true, true, false, true, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Array, SamplerType::Sampler2DArray, TextureChannelHint::RGBA }, { SamplerSlot::Normal, TextureSourceMode::Array, SamplerType::Sampler2DArray, TextureChannelHint::RGBA } }, .texture_count = 2,
      .schema = ShaderDataSchema::StandardParams, .def_hint = StaticMaterialDefIdHint::Standard3D },

    { .name = "StandardBlinnPhongArray", .preset = MaterialPreset::Standard, .factory_type = MaterialPreset::Standard,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Standard, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::PositionNormal3D, .vertex_policy = VertexTransformPolicy::Mesh3D, .surface_model = SurfaceShadingModel::StandardBlinnPhong,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "surface/textureblinnphong_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal, VertexAttrib::TexCoord }), .fs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal, VertexAttrib::TexCoord }),
      .resources = { true, true, true, true, true, true, false, true, LightingModel::BlinnPhong, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Array, SamplerType::Sampler2DArray, TextureChannelHint::RGBA }, { SamplerSlot::Normal, TextureSourceMode::Array, SamplerType::Sampler2DArray, TextureChannelHint::RGBA } }, .texture_count = 2,
      .schema = ShaderDataSchema::StandardParams, .def_hint = StaticMaterialDefIdHint::Standard3D },

    { .name = "StandardPBRArray", .preset = MaterialPreset::Standard, .factory_type = MaterialPreset::Standard,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Standard, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::PositionNormal3D, .vertex_policy = VertexTransformPolicy::Mesh3D, .surface_model = SurfaceShadingModel::StandardPBR,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "surface/standard_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal, VertexAttrib::TexCoord }), .fs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal, VertexAttrib::TexCoord }),
      .resources = { true, true, true, true, true, true, false, true, LightingModel::PBR, SkyLightAmbientModel::Simple },
      .textures = { { SamplerSlot::BaseColor, TextureSourceMode::Array, SamplerType::Sampler2DArray, TextureChannelHint::RGBA }, { SamplerSlot::Normal, TextureSourceMode::Array, SamplerType::Sampler2DArray, TextureChannelHint::RGBA } }, .texture_count = 2,
      .schema = ShaderDataSchema::StandardParams, .def_hint = StaticMaterialDefIdHint::Standard3D },

    { .name = "PBRColor3D", .preset = MaterialPreset::PBRColor3D, .factory_type = MaterialPreset::PBRColor3D,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Standard, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::DirectVec3,
      .vertex_input = VertexInputProfile::PositionNormal3D, .vertex_policy = VertexTransformPolicy::Mesh3D, .surface_model = SurfaceShadingModel::PBRColor,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "surface/pbrcolor3d_surface.glsl",
      .vs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal }), .fs_features = MakeStageFeatures({ VertexAttrib::Position, VertexAttrib::Normal }),
      .resources = { true, true, true, true, true, false, false, true, LightingModel::PBR, SkyLightAmbientModel::Simple },
      .schema = ShaderDataSchema::PBRColorParams, .def_hint = StaticMaterialDefIdHint::Standard3D },

    { .name = "FullscreenTriangle", .preset = MaterialPreset::FullscreenTriangle, .factory_type = MaterialPreset::FullscreenTriangle,
      .primitive = PrimitiveType::Triangles, .surface_type = SurfaceType::Unlit, .geometry_mode = GeometryMode::Mesh3D, .position_provider = PositionProviderId::PCG_FullscreenTriangle,
      .vertex_input = VertexInputProfile::FullscreenProcedural, .vertex_policy = VertexTransformPolicy::FullscreenTriangle, .surface_model = SurfaceShadingModel::Unknown,
      .blend = RenderAlphaMode::Opaque, .pass = PassType::ForwardOpaque,
      .vs_template_path = "", .fs_template_path = "", .surface_path = "",
      .vs_features = MakeStageFeatures({}), .fs_features = MakeStageFeatures({}),
      .resources = { false, false, false, false, false, false, false, false, LightingModel::Lambert, SkyLightAmbientModel::Simple },
      .schema = ShaderDataSchema::None, .def_hint = StaticMaterialDefIdHint::FullscreenTriangle },
};
// clang-format on

const size_t kBuiltinVariantRowsCount = std::size(kBuiltinVariantRows);
static_assert(kBuiltinVariantRowsCount == kBuiltinVariantsCount,
              "kBuiltinVariantRows must cover every kBuiltinVariants entry in Phase 2");

void VariantRegistry::InitializeBuiltinVariants()
{
    for (const auto &e : kBuiltinVariants)
        RegisterVariant(BuildKey(e), BuildDesc(e));
}

// ---------------------------------------------------------------------------
// Global singleton accessor — initialised on first call, then read-only
// ---------------------------------------------------------------------------
const VariantRegistry &GetBuiltinVariantRegistry()
{
    static VariantRegistry s_registry = []()
    {
        VariantRegistry r;
        r.InitializeBuiltinVariants();
        return r;
    }();
    return s_registry;
}

} // namespace hgl::graph::mtl
