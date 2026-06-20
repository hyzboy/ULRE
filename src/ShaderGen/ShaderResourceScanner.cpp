#include <hgl/shadergen/ShaderResourceScanner.h>
#include <hgl/mtl/DescriptorSemanticRegistry.h>
#include <hgl/shadergen/registry/ErrorCodeRegistry.h>
#include <hgl/shadergen/CompositorCompiler.h>
#include "GLSLCompiler.h"

#include "SPVParseData.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

namespace hgl::graph::mtl
{
namespace
{
    static std::string TrimASCII(const std::string &text)
    {
        size_t begin = 0;
        while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
            ++begin;

        size_t end = text.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
            --end;

        return text.substr(begin, end - begin);
    }

    static std::vector<std::string> SplitASCIIWords(const std::string &text)
    {
        std::vector<std::string> out;
        size_t pos = 0;
        while (pos < text.size())
        {
            while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
                ++pos;
            const size_t begin = pos;
            while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos])))
                ++pos;
            if (begin < pos)
                out.emplace_back(text.substr(begin, pos - begin));
        }
        return out;
    }

    static std::string ToLowerASCII(std::string text)
    {
        for (char &c : text)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    }

    static std::string JoinTokens(const std::vector<std::string> &tokens)
    {
        std::string out;
        for (size_t i = 0; i < tokens.size(); ++i)
        {
            if (i)
                out += ' ';
            out += tokens[i];
        }
        return out;
    }

    static std::string MakeTokenKey(const std::string &directive, const std::vector<std::string> &args)
    {
        std::string out = ToLowerASCII(directive);
        out += '|';
        for (size_t i = 0; i < args.size(); ++i)
        {
            if (i)
                out += ' ';
            out += ToLowerASCII(args[i]);
        }
        return out;
    }

    static void AppendIssue(SFMAnnotationScanReport &report,
                            const SFMAnnotationError error,
                            const uint32_t line,
                            const std::string &key,
                            const std::string &detail)
    {
        SFMAnnotationIssue issue;
        issue.error_code = EncodeSFMAnnotationError(error,
                                                    GetSFMAnnotationKeyIndex(key),
                                                    static_cast<uint8_t>(line & 0xFFu),
                                                    0);
        issue.line = line;
        issue.key = key;
        issue.detail = detail;
        report.issues.emplace_back(std::move(issue));
    }

    static bool IsAnnotationLine(const std::string &line, std::string &payload)
    {
        std::string trimmed = TrimASCII(line);
        if (trimmed.empty())
            return false;

        const auto pos = trimmed.find("@sfm:");
        if (pos == std::string::npos)
            return false;

        payload = trimmed.substr(pos + 5);
        payload = TrimASCII(payload);
        return !payload.empty();
    }

    struct ReflectedResource
    {
        std::string name;
        uint32_t descriptor_type = 0;
        uint32_t stage_flags = 0;
        uint32_t set = 0;
        uint32_t binding = 0;
    };

    static std::string ToUpperASCII(const std::string &text)
    {
        std::string out;
        out.reserve(text.size());

        for (const unsigned char c : text)
        {
            if (c >= 'a' && c <= 'z')
                out.push_back(char(c - ('a' - 'A')));
            else
                out.push_back(char(c));
        }

        return out;
    }

    static bool EqualsNoCase(const std::string &a, const std::string &b)
    {
        return ToUpperASCII(a) == ToUpperASCII(b);
    }

    static const char *ToTextureChannelHintName(const TextureChannelHint hint)
    {
        switch (hint)
        {
        case TextureChannelHint::RGBA: return "RGBA";
        case TextureChannelHint::Grayscale: return "Grayscale";
        default: return "Unknown";
        }
    }

    static void DumpCollectedRequirements(const MaterialResourceManifest &requirements)
    {
        if (requirements.ubos.empty())
        {
            std::fprintf(stderr, "[ShaderResourceScanner]   UBO: (none)\n");
        }
        else
        {
            for (const auto semantic : requirements.ubos)
            {
                std::fprintf(stderr,
                             "[ShaderResourceScanner]   UBO: semantic=%s\n",
                             GetUBODescriptorSemanticName(semantic));
            }
        }

        if (requirements.ssbos.empty())
        {
            std::fprintf(stderr, "[ShaderResourceScanner]   SSBO: (none)\n");
        }
        else
        {
            for (const auto semantic : requirements.ssbos)
            {
                std::fprintf(stderr,
                             "[ShaderResourceScanner]   SSBO: semantic=%s\n",
                             GetSSBODescriptorSemanticName(semantic));
            }
        }

        if (requirements.samplers.empty())
        {
            std::fprintf(stderr, "[ShaderResourceScanner]   TEX: (none)\n");
        }
        else
        {
            for (const auto &[slot, sampler] : requirements.samplers)
            {
                std::fprintf(stderr,
                             "[ShaderResourceScanner]   TEX: slot=%s, type=%s, channel=%s\n",
                             SamplerSlotNameList[size_t(slot)],
                             GetSamplerTypeName(sampler.sampler_type),
                             ToTextureChannelHintName(sampler.channel_hint));
            }
        }
    }

    static bool TryMapReflectedUBO(const std::string &name, UBODescriptorSemantic &semantic)
    {
        for (size_t i = 0; i < UBODescriptorSemanticCount; ++i)
        {
            const UBODescriptorSemantic candidate = static_cast<UBODescriptorSemantic>(i);
            if (!RangeCheck(candidate))
                continue;

            const DescriptorSemanticMeta &meta = GetDescriptorSemanticMeta(candidate);
            if ((meta.name && EqualsNoCase(name, meta.name))
             || EqualsNoCase(name, GetUBODescriptorSemanticName(candidate)))
            {
                semantic = candidate;
                return true;
            }
        }

        return false;
    }

    static bool TryMapReflectedSSBO(const std::string &name, SSBODescriptorSemantic &semantic)
    {
        for (size_t i = 0; i < SSBODescriptorSemanticCount; ++i)
        {
            const SSBODescriptorSemantic candidate = static_cast<SSBODescriptorSemantic>(i);
            if (!RangeCheck(candidate))
                continue;

            const DescriptorSemanticMeta &meta = GetDescriptorSemanticMeta(candidate);
            if ((meta.name && EqualsNoCase(name, meta.name))
             || EqualsNoCase(name, GetSSBODescriptorSemanticName(candidate)))
            {
                semantic = candidate;
                return true;
            }
        }

        return false;
    }

    static bool TryMapReflectedSampler(const std::string &name, SamplerSlot &slot)
    {
        for (size_t i = 0; i < SamplerSlotCount; ++i)
        {
            const SamplerSlot candidate = static_cast<SamplerSlot>(i);
            if (EqualsNoCase(name, SamplerSlotNameList[i])
             || EqualsNoCase(name, ToDescriptorName(candidate))
             || EqualsNoCase(name, ToGLSLSamplerSymbol(candidate)))
            {
                slot = candidate;
                return true;
            }
        }

        return false;
    }

    static const StaticTextureSamplerDescriptor *FindBaseSamplerDescriptor(const StaticMaterialDef &base_def,
                                                                          const SamplerSlot slot)
    {
        if (!base_def.texture_samplers)
            return nullptr;

        auto iter = base_def.texture_samplers->find(slot);
        if (iter == base_def.texture_samplers->end())
            return nullptr;

        return &iter->second;
    }

    static void BuildReflectionSeedDescriptors(const UBOSemanticSet *source,
                                               UBOSemanticSet &out_descriptors)
    {
        if (source)
            out_descriptors = *source;
        else
            out_descriptors.clear();
    }

    static void BuildReflectionSeedDescriptors(const SSBOSemanticSet *source,
                                               SSBOSemanticSet &out_descriptors)
    {
        if (source)
            out_descriptors = *source;
        else
            out_descriptors.clear();
    }

    static void BuildReflectionSeedDescriptors(const StaticTextureSamplerDescriptors *source,
                                               StaticTextureSamplerDescriptors &out_descriptors)
    {
        out_descriptors.clear();

        if (!source)
            return;

        out_descriptors = *source;
    }

    static void AppendReflectedStageResources(const uint32_t descriptor_type,
                                              const uint32_t stage_flags,
                                              const ShaderResourceData<Descriptor> &bucket,
                                              std::vector<ReflectedResource> &out_resources)
    {
        for (uint32_t i = 0; i < bucket.count; ++i)
        {
            const Descriptor &item = bucket.items[i];

            ReflectedResource resource;
            resource.name = item.name;
            resource.descriptor_type = descriptor_type;
            resource.stage_flags = stage_flags;
            resource.set = item.set;
            resource.binding = item.binding;
            out_resources.emplace_back(std::move(resource));
        }
    }

    static bool CollectReflectedStageResources(const uint32_t stage_flags,
                                               const std::string &source,
                                               std::vector<ReflectedResource> &out_resources,
                                               std::string *diagnostics)
    {
        if (source.empty())
            return true;

        std::string debug_context = "ShaderResourceScanner::CollectReflectedStageResources stage="
                      + std::to_string(stage_flags);
        SPVData *spv = CompileShader(stage_flags, source.c_str(), debug_context.c_str());
        if (!spv)
        {
            if (diagnostics)
                *diagnostics += "CompileShader() failed for stage " + std::to_string(stage_flags) + "\n";
            return false;
        }

        SPVParseData *parse_data = ParseShaderSPV(spv);
        if (!parse_data)
        {
            if (diagnostics)
                *diagnostics += "ParseShaderSPV() returned null for stage " + std::to_string(stage_flags) + "\n";
            FreeSPVData(spv);
            return false;
        }

        for (uint32_t descriptor_type = 0; descriptor_type < VK_DESCRIPTOR_TYPE_COUNT; ++descriptor_type)
            AppendReflectedStageResources(descriptor_type, stage_flags, parse_data->resource[descriptor_type], out_resources);

        FreeShaderSPVParseData(parse_data);
        FreeSPVData(spv);
        return true;
    }

    static bool BuildReflectedRequirements(const StaticMaterialDef &base_def,
                                           const std::vector<ReflectedResource> &resources,
                                           MaterialResourceManifest &out_requirements,
                                           std::string *diagnostics)
    {
        for (const ReflectedResource &resource : resources)
        {
            if (resource.name.empty())
                continue;

            if (resource.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
             || resource.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
            {
                UBODescriptorSemantic semantic = UBODescriptorSemantic::Unknown;
                if (!TryMapReflectedUBO(resource.name, semantic))
                {
                    if (diagnostics)
                        *diagnostics += "Unknown reflected UBO: " + resource.name + "\n";
                    return false;
                }

                AddUBODescriptor(out_requirements.ubos, semantic);
                continue;
            }

            if (resource.descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
             || resource.descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
            {
                SSBODescriptorSemantic semantic = SSBODescriptorSemantic::Unknown;
                if (!TryMapReflectedSSBO(resource.name, semantic))
                {
                    if (diagnostics)
                        *diagnostics += "Unknown reflected SSBO: " + resource.name + "\n";
                    return false;
                }

                AddSSBODescriptor(out_requirements.ssbos, semantic);
                continue;
            }

            if (resource.descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
             || resource.descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
             || resource.descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLER)
            {
                SamplerSlot slot = SamplerSlot::BaseColor;
                if (!TryMapReflectedSampler(resource.name, slot))
                {
                    if (diagnostics)
                        *diagnostics += "Unknown reflected sampler: " + resource.name + "\n";
                    return false;
                }

                const StaticTextureSamplerDescriptor *base_sampler = FindBaseSamplerDescriptor(base_def, slot);
                if (!base_sampler)
                {
                    if (diagnostics)
                        *diagnostics += "Reflected sampler missing in base_def: " + resource.name + "\n";
                    return false;
                }

                AddTextureSampler(out_requirements.samplers,
                                       slot,
                                       base_sampler->sampler_type,
                                       base_sampler->atlas_cols,
                                       base_sampler->atlas_rows,
                                       base_sampler->channel_hint);
            }
        }

        return true;
    }

    static bool ParseSFMAnnotationLine(const std::string &payload,
                                       const uint32_t line_no,
                                       SFMAnnotationScanReport &report,
                                       std::string *diagnostics)
    {
        const auto words = SplitASCIIWords(payload);
        if (words.empty())
        {
            AppendIssue(report, SFMAnnotationError::InvalidDirective, line_no, "", "empty annotation payload");
            if (diagnostics)
                *diagnostics += "SFM annotation line " + std::to_string(line_no) + ": empty payload\n";
            return false;
        }

        const std::string directive = ToLowerASCII(words[0]);
        const std::vector<std::string> args(words.begin() + 1, words.end());

        if (!IsKnownSFMAnnotationKey(directive))
        {
            AppendIssue(report, SFMAnnotationError::UnknownKey, line_no, directive,
                        "unknown SFM directive key");
            if (diagnostics)
                *diagnostics += "SFM annotation line " + std::to_string(line_no) + ": unknown key '" + directive + "'\n";
            return false;
        }

        const std::string dedup_key = MakeTokenKey(directive, args);
        for (const auto &record : report.records)
        {
            if (MakeTokenKey(record.key, record.args) == dedup_key)
            {
                AppendIssue(report, SFMAnnotationError::DuplicateKey, line_no, directive,
                            "duplicate annotation line");
                if (diagnostics)
                    *diagnostics += "SFM annotation line " + std::to_string(line_no) + ": duplicate '" + JoinTokens(words) + "'\n";
                return false;
            }
        }

        if (directive == "surface_type")
        {
            if (args.size() != 1)
            {
                AppendIssue(report, SFMAnnotationError::InvalidDirective, line_no, directive,
                            "surface_type requires exactly 1 value");
                if (diagnostics)
                    *diagnostics += "SFM annotation line " + std::to_string(line_no) + ": surface_type requires 1 arg\n";
                return false;
            }
        }
        else if (directive == "supports_phase")
        {
            if (args.empty())
            {
                AppendIssue(report, SFMAnnotationError::InvalidDirective, line_no, directive,
                            "supports_phase requires at least 1 value");
                if (diagnostics)
                    *diagnostics += "SFM annotation line " + std::to_string(line_no) + ": supports_phase requires args\n";
                return false;
            }
        }
        else if (directive == "require" || directive == "optional" || directive == "derive")
        {
            if (args.size() < 2)
            {
                AppendIssue(report, SFMAnnotationError::InvalidDirective, line_no, directive,
                            directive + " requires <kind> <value...>");
                if (diagnostics)
                    *diagnostics += "SFM annotation line " + std::to_string(line_no) + ": " + directive + " requires kind+values\n";
                return false;
            }
        }

        SFMAnnotationRecord record;
        record.key = directive;
        record.args = args;
        record.line = line_no;
        report.records.emplace_back(std::move(record));
        return true;
    }
}

bool ParseSFMAnnotationsFromGLSL(const std::string &source,
                                 SFMAnnotationScanReport &out_report,
                                 std::string *diagnostics) noexcept
{
    out_report.records.clear();
    out_report.issues.clear();

    if (diagnostics)
        diagnostics->clear();

    std::set<std::string> surface_types;
    std::set<std::string> phases;
    std::set<std::string> required_tokens;
    std::set<std::string> optional_tokens;
    std::set<std::string> derived_tokens;
    std::map<std::string, SFMAnnotationRecord> first_singleton_record_by_key;

    size_t cursor = 0;
    uint32_t line_no = 1;
    while (cursor <= source.size())
    {
        const size_t next = source.find('\n', cursor);
        const std::string line = source.substr(cursor, next == std::string::npos ? std::string::npos : next - cursor);

        std::string payload;
        if (IsAnnotationLine(line, payload))
            ParseSFMAnnotationLine(payload, line_no, out_report, diagnostics);

        if (next == std::string::npos)
            break;
        cursor = next + 1;
        ++line_no;
    }

    // Validate collected records for duplicate/conflict/derive-range semantics.
    for (const auto &record : out_report.records)
    {
        const bool is_singleton_key = (record.key == "surface_type"
                                    || record.key == "supports_phase");

        if (is_singleton_key)
        {
            auto seen = first_singleton_record_by_key.find(record.key);
            if (seen != first_singleton_record_by_key.end())
            {
                const bool same_payload = (seen->second.args == record.args);
                AppendIssue(out_report,
                            same_payload ? SFMAnnotationError::DuplicateKey : SFMAnnotationError::ConflictingKey,
                            record.line,
                            record.key,
                            same_payload ? "duplicate annotation directive" : "conflicting annotation directive");
                continue;
            }

            first_singleton_record_by_key.emplace(record.key, record);
        }

        if (record.key == "surface_type")
        {
            surface_types.insert(ToLowerASCII(record.args[0]));
            continue;
        }

        if (record.key == "supports_phase")
        {
            for (const auto &phase : record.args)
                phases.insert(ToLowerASCII(phase));
            continue;
        }

        auto &target = (record.key == "optional") ? optional_tokens :
                       (record.key == "derive") ? derived_tokens : required_tokens;

        const std::string kind = ToLowerASCII(record.args[0]);
        for (size_t i = 1; i < record.args.size(); ++i)
        {
            const std::string token = kind + ":" + ToLowerASCII(record.args[i]);
            target.insert(token);
        }
    }

    for (const auto &token : derived_tokens)
    {
        if (required_tokens.find(token) == required_tokens.end()
         && optional_tokens.find(token) == optional_tokens.end())
        {
            AppendIssue(out_report, SFMAnnotationError::DeriveOutOfRange, 0, token,
                        "derive token is not covered by require/optional");
        }
    }

    return !out_report.HasErrors();
}

bool CollectShaderAutoRequirements(const StaticMaterialDef &base_def,
                                   const std::string &shader_library_path,
                                   const std::string &vertex_glsl,
                                   const std::string &fragment_glsl,
                                   MaterialResourceManifest &out_requirements,
                                   std::string *diagnostics)
{
    out_requirements.clear();

    if (diagnostics)
        diagnostics->clear();

    UBOSemanticSet reflection_seed_ubos;
    SSBOSemanticSet reflection_seed_ssbos;
    StaticTextureSamplerDescriptors reflection_seed_samplers;

    BuildReflectionSeedDescriptors(base_def.ubo_descriptors, reflection_seed_ubos);
    BuildReflectionSeedDescriptors(base_def.ssbo_descriptors, reflection_seed_ssbos);
    BuildReflectionSeedDescriptors(base_def.texture_samplers, reflection_seed_samplers);

    StaticMaterialDef reflection_seed_def = base_def;
    reflection_seed_def.ubo_descriptors = reflection_seed_ubos.empty() ? nullptr : &reflection_seed_ubos;
    reflection_seed_def.ssbo_descriptors = reflection_seed_ssbos.empty() ? nullptr : &reflection_seed_ssbos;
    reflection_seed_def.texture_samplers = reflection_seed_samplers.empty() ? nullptr : &reflection_seed_samplers;

    std::string prepared_vs;
    std::string prepared_fs;
    if (!PrepareCompositorGLSLForReflection(reflection_seed_def,
                                            vertex_glsl,
                                            fragment_glsl,
                                            prepared_vs,
                                            prepared_fs,
                                            diagnostics))
    {
        return false;
    }

    // Phase2: comment-based SFM annotation pre-scan. This is a no-op for shader
    // sources that have not yet been annotated, but it gives us the actual entry
    // point for Week1-2 validation.
    SFMAnnotationScanReport sfm_report;
    if (!ParseSFMAnnotationsFromGLSL(prepared_vs, sfm_report, diagnostics))
        return false;
    if (!ParseSFMAnnotationsFromGLSL(prepared_fs, sfm_report, diagnostics))
        return false;

    std::vector<ReflectedResource> resources;
    if (!CollectReflectedStageResources(uint32_t(VK_SHADER_STAGE_VERTEX_BIT), prepared_vs, resources, diagnostics))
        return false;

    if (!CollectReflectedStageResources(uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), prepared_fs, resources, diagnostics))
        return false;

    if (!BuildReflectedRequirements(base_def, resources, out_requirements, diagnostics))
        return false;

    std::fprintf(stderr,
                 "[ShaderResourceScanner] reflection collection: shader_lib='%s', vs_bytes=%zu, fs_bytes=%zu\n",
                 shader_library_path.c_str(),
                 prepared_vs.size(),
                 prepared_fs.size());
    DumpCollectedRequirements(out_requirements);

    return true;
}

MaterialResourceManifest MergeManifestWithAutoRequirements(
    const StaticMaterialDef &base_def,
    const MaterialResourceManifest &auto_requirements)
{
    MaterialResourceManifest result = MaterialResourceManifest::FromStaticDef(base_def);
    result.MergeKeepFirst(auto_requirements);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 5: manifest ↔ effective-policy operations
// ─────────────────────────────────────────────────────────────────────────────

MaterialResourceManifest PruneManifestByPolicy(
    const MaterialResourceManifest     &manifest,
    const MaterialResourceRequirements &effective_policy)
{
    MaterialResourceManifest pruned = manifest;

    // Sky: SkyInfo UBO
    if (!effective_policy.needs_sky)
    {
        pruned.ubos.erase(UBODescriptorSemantic::SkyInfo);
    }

    // Lighting: no separate UBO semantic, but if lighting is pruned then sky
    // typically also goes (handled above). Nothing else to remove from the manifest
    // at this level—lighting determines which shader code path is chosen, not
    // which *distinct* UBO resource is bound separately from CameraInfo.

    // MaterialInstance / MI: handled by the SSBO remove below.
    if (!effective_policy.needs_material_instance)
    {
        pruned.ssbos.erase(SSBODescriptorSemantic::MaterialBindingInstanceID);
        pruned.ssbos.erase(SSBODescriptorSemantic::MaterialBindingInstanceData);
    }

    if (!effective_policy.needs_material_texture_index)
    {
        pruned.ssbos.erase(SSBODescriptorSemantic::MaterialBindingInstanceTexture);
    }

    // Camera / Viewport: rarely pruned in practice (depth pass still needs camera),
    // but honour the policy flags so the table is authoritative.
    if (!effective_policy.needs_camera)
    {
        pruned.ubos.erase(UBODescriptorSemantic::CameraInfo);
    }

    if (!effective_policy.needs_viewport)
    {
        pruned.ubos.erase(UBODescriptorSemantic::ViewportInfo);
    }

    return pruned;
}

bool ValidateManifestAgainstPolicy(
    const MaterialResourceManifest     &manifest,
    const MaterialResourceRequirements &effective_policy,
    const char                         *material_name,
    std::string                        *diagnostics)
{
    const char *name = material_name ? material_name : "<unnamed>";
    bool ok = true;

    // Helper: append a message and mark violation.
    auto forbids = [&](const char *resource)
    {
        std::string msg;
        msg += "[ShaderResourceScanner] Phase5 PolicyForbids: material='";
        msg += name;
        msg += "' manifest contains '";
        msg += resource;
        msg += "' but effective policy disallows it";
        std::fprintf(stderr, "%s\n", msg.c_str());
        if (diagnostics)
        {
            *diagnostics += msg;
            *diagnostics += "\n";
        }
        ok = false;
    };

    auto pruned_info = [&](const char *resource)
    {
        std::fprintf(stderr,
            "[ShaderResourceScanner] Phase5 PolicyRequires: material='%s'"
            " policy allows '%s' but manifest omits it (pruned by reflection — OK)\n",
            name, resource);
    };

    // SkyInfo UBO
    {
        const bool has_sky = manifest.ubos.count(UBODescriptorSemantic::SkyInfo) > 0;
        const PolicyManifestCheckResult r = CheckUBOAgainstPolicy(has_sky, effective_policy.needs_sky);
        if (r == PolicyManifestCheckResult::PolicyForbids)
            forbids("SkyInfo");
        else if (r == PolicyManifestCheckResult::PolicyRequires)
            pruned_info("SkyInfo");
    }

    // CameraInfo UBO
    {
        const bool has_cam = manifest.ubos.count(UBODescriptorSemantic::CameraInfo) > 0;
        const PolicyManifestCheckResult r = CheckUBOAgainstPolicy(has_cam, effective_policy.needs_camera);
        if (r == PolicyManifestCheckResult::PolicyForbids)
            forbids("CameraInfo");
    }

    // ViewportInfo UBO
    {
        const bool has_vp = manifest.ubos.count(UBODescriptorSemantic::ViewportInfo) > 0;
        const PolicyManifestCheckResult r = CheckUBOAgainstPolicy(has_vp, effective_policy.needs_viewport);
        if (r == PolicyManifestCheckResult::PolicyForbids)
            forbids("ViewportInfo");
    }

    // MaterialBindingInstanceID / Data SSBOs
    {
        const bool has_mi = manifest.ssbos.count(SSBODescriptorSemantic::MaterialBindingInstanceID) > 0
                         || manifest.ssbos.count(SSBODescriptorSemantic::MaterialBindingInstanceData) > 0;
        const PolicyManifestCheckResult r = CheckUBOAgainstPolicy(has_mi, effective_policy.needs_material_instance);
        if (r == PolicyManifestCheckResult::PolicyForbids)
            forbids("MaterialBindingInstance");
    }

    // MaterialBindingInstanceTexture SSBO
    {
        const bool has_mitex = manifest.ssbos.count(SSBODescriptorSemantic::MaterialBindingInstanceTexture) > 0;
        const PolicyManifestCheckResult r = CheckUBOAgainstPolicy(has_mitex, effective_policy.needs_material_texture_index);
        if (r == PolicyManifestCheckResult::PolicyForbids)
            forbids("MaterialBindingInstanceTexture");
    }

    return ok;
}

} // end anonymous namespace guard — file namespace hgl::graph::mtl
