#include <hgl/shadergen/contract/ShaderGenMirrorDiff.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/graph/shared/VertexAttribDef.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace hgl::graph::mtl::contract
{
    static constexpr uint64_t kFnvOffset = 1469598103934665603ull;
    static constexpr uint64_t kFnvPrime = 1099511628211ull;

    static uint64_t HashBytes(uint64_t hash, const void *data, size_t size)
    {
        const auto *p = reinterpret_cast<const uint8_t *>(data);
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= static_cast<uint64_t>(p[i]);
            hash *= kFnvPrime;
        }
        return hash;
    }

    static uint64_t HashU32(uint64_t hash, uint32_t value)
    {
        return HashBytes(hash, &value, sizeof(value));
    }

    static uint64_t HashString(uint64_t hash, const std::string &text)
    {
        const uint32_t size = static_cast<uint32_t>(text.size());
        hash = HashU32(hash, size);
        if (!text.empty())
            hash = HashBytes(hash, text.data(), text.size());
        return hash;
    }

    static ResourceClass ToResourceClass(const VkDescriptorType desc_type)
    {
        switch(desc_type)
        {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return ResourceClass::UniformBuffer;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return ResourceClass::StorageBuffer;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return ResourceClass::SampledImage;
            case VK_DESCRIPTOR_TYPE_SAMPLER: return ResourceClass::Sampler;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return ResourceClass::CombinedImageSampler;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return ResourceClass::InputAttachment;
            default: return ResourceClass::Unknown;
        }
    }

    struct LayoutSig
    {
        uint32_t set = 0;
        uint32_t binding = 0;
        uint32_t resource_class = 0;
        uint32_t stage_mask = 0;
        std::string name;

        bool operator<(const LayoutSig &rhs) const
        {
            if (set != rhs.set) return set < rhs.set;
            if (binding != rhs.binding) return binding < rhs.binding;
            if (resource_class != rhs.resource_class) return resource_class < rhs.resource_class;
            if (stage_mask != rhs.stage_mask) return stage_mask < rhs.stage_mask;
            return name < rhs.name;
        }
    };

    static uint64_t HashLayout(std::vector<LayoutSig> sigs)
    {
        std::sort(sigs.begin(), sigs.end());
        uint64_t hash = kFnvOffset;
        for (const auto &s : sigs)
        {
            hash = HashU32(hash, s.set);
            hash = HashU32(hash, s.binding);
            hash = HashU32(hash, s.resource_class);
            hash = HashU32(hash, s.stage_mask);
            hash = HashString(hash, s.name);
        }
        return hash;
    }

    static void BuildLegacyLayoutSigs(const MaterialCreateInfo &mci, std::vector<LayoutSig> &out)
    {
        const auto &sds_array = mci.GetMDI().Get();

        for (size_t i = 0; i < DESCRIPTOR_SET_TYPE_COUNT; ++i)
        {
            std::vector<ShaderDescriptor*> values;
            sds_array[i].descriptor_map.GetValueArray(values);

            for (const auto *sd : values)
            {
                if (!sd)
                    continue;

                LayoutSig sig;
                sig.set = sd->set >= 0 ? uint32_t(sd->set) : uint32_t(i);
                sig.binding = sd->binding >= 0 ? uint32_t(sd->binding) : 0u;
                sig.resource_class = static_cast<uint32_t>(ToResourceClass(sd->desc_type));
                sig.stage_mask = sd->stage_flag;
                sig.name = sd->name;
                out.emplace_back(std::move(sig));
            }
        }
    }

    static void BuildMirrorLayoutSigs(const ShaderGenResult &result, std::vector<LayoutSig> &out)
    {
        for (const auto &binding : result.layout.bindings)
        {
            LayoutSig sig;
            sig.set = binding.set;
            sig.binding = binding.binding;
            sig.resource_class = static_cast<uint32_t>(binding.resource_class);
            sig.stage_mask = binding.stage_mask;
            sig.name = binding.name;
            out.emplace_back(std::move(sig));
        }
    }

    struct VertexSig
    {
        uint32_t location = 0;
        uint32_t input_rate = 0;
        std::string semantic;
        std::string type_name;

        bool operator<(const VertexSig &rhs) const
        {
            if (location != rhs.location) return location < rhs.location;
            if (input_rate != rhs.input_rate) return input_rate < rhs.input_rate;
            if (semantic != rhs.semantic) return semantic < rhs.semantic;
            return type_name < rhs.type_name;
        }
    };

    static uint64_t HashVertex(std::vector<VertexSig> sigs)
    {
        std::sort(sigs.begin(), sigs.end());
        uint64_t hash = kFnvOffset;
        for (const auto &s : sigs)
        {
            hash = HashU32(hash, s.location);
            hash = HashU32(hash, s.input_rate);
            hash = HashString(hash, s.semantic);
            hash = HashString(hash, s.type_name);
        }
        return hash;
    }

    static void BuildLegacyVertexSigs(const MaterialCreateInfo &mci, std::vector<VertexSig> &out)
    {
        auto *vsc = mci.GetVS();
        if (!vsc)
            return;

        const auto &inputs = vsc->GetInput();
        for (uint32_t i = 0; i < inputs.count; ++i)
        {
            const auto &via = inputs.items[i];

            VertexSig sig;
            sig.location = via.location;
            sig.input_rate = via.input_rate;
            sig.semantic = via.name;
            sig.type_name = GetVertexAttribName((VABaseType)via.basetype, via.vec_size);
            out.emplace_back(std::move(sig));
        }
    }

    static void BuildMirrorVertexSigs(const ShaderGenResult &result, std::vector<VertexSig> &out)
    {
        for (const auto &attr : result.vertex_layout.attributes)
        {
            VertexSig sig;
            sig.location = attr.location;
            sig.input_rate = attr.input_rate;
            sig.semantic = attr.semantic;
            sig.type_name = attr.type_name;
            out.emplace_back(std::move(sig));
        }
    }

    struct SpvSig
    {
        uint32_t stage_mask = 0;
        std::vector<uint32_t> words;

        bool operator<(const SpvSig &rhs) const
        {
            return stage_mask < rhs.stage_mask;
        }
    };

    static std::string BuildStageMaskSummary(const std::vector<SpvSig> &sigs)
    {
        if (sigs.empty())
            return "none";

        std::vector<uint32_t> masks;
        masks.reserve(sigs.size());
        for (const auto &s : sigs)
            masks.emplace_back(s.stage_mask);

        std::sort(masks.begin(), masks.end());
        masks.erase(std::unique(masks.begin(), masks.end()), masks.end());

        std::string out;
        for (size_t i = 0; i < masks.size(); ++i)
        {
            if (!out.empty())
                out += '|';

            char buf[32] = {};
            std::snprintf(buf, sizeof(buf), "0x%X", static_cast<unsigned>(masks[i]));
            out += buf;
        }

        return out;
    }

    static uint32_t BuildStageComboMask(const std::vector<SpvSig> &sigs)
    {
        uint32_t combo = 0;
        for (const auto &s : sigs)
            combo |= s.stage_mask;
        return combo;
    }

    static uint64_t HashSpv(std::vector<SpvSig> sigs)
    {
        std::sort(sigs.begin(), sigs.end());
        uint64_t hash = kFnvOffset;
        for (const auto &s : sigs)
        {
            hash = HashU32(hash, s.stage_mask);
            hash = HashU32(hash, static_cast<uint32_t>(s.words.size()));
            if (!s.words.empty())
                hash = HashBytes(hash, s.words.data(), s.words.size() * sizeof(uint32_t));
        }
        return hash;
    }

    static void BuildLegacySpvSigs(const MaterialCreateInfo &mci, std::vector<SpvSig> &out)
    {
        const auto &shader_map = mci.GetShaderMap();
        for (const auto &kv : shader_map)
        {
            const ShaderCreateInfo *sc = kv.second;
            if (!sc)
                continue;

            const uint32_t *spv_data = sc->GetSPVData();
            const size_t spv_length = sc->GetSPVSize();
            if (!spv_data || spv_length == 0)
                continue;

            SpvSig sig;
            sig.stage_mask = static_cast<uint32_t>(sc->GetShaderStage());
            sig.words.assign(spv_data, spv_data + (spv_length / sizeof(uint32_t)));
            out.emplace_back(std::move(sig));
        }
    }

    static void BuildMirrorSpvSigs(const ShaderGenResult &result, std::vector<SpvSig> &out)
    {
        for (const auto &blob : result.spv_per_stage)
        {
            SpvSig sig;
            sig.stage_mask = blob.stage_mask;
            sig.words = blob.words;
            out.emplace_back(std::move(sig));
        }
    }

    bool BuildShaderGenMirrorDiffSummary(const MaterialCreateInfo &mci,
                                         const ShaderGenResult &result,
                                         ShaderGenMirrorDiffSummary &summary)
    {
        summary = ShaderGenMirrorDiffSummary{};

        std::vector<LayoutSig> legacy_layout;
        std::vector<LayoutSig> mirror_layout;
        BuildLegacyLayoutSigs(mci, legacy_layout);
        BuildMirrorLayoutSigs(result, mirror_layout);

        std::vector<VertexSig> legacy_vertex;
        std::vector<VertexSig> mirror_vertex;
        BuildLegacyVertexSigs(mci, legacy_vertex);
        BuildMirrorVertexSigs(result, mirror_vertex);

        std::vector<SpvSig> legacy_spv;
        std::vector<SpvSig> mirror_spv;
        BuildLegacySpvSigs(mci, legacy_spv);
        BuildMirrorSpvSigs(result, mirror_spv);

        summary.layout_hash_legacy = HashLayout(legacy_layout);
        summary.layout_hash_mirror = HashLayout(mirror_layout);
        summary.vertex_hash_legacy = HashVertex(legacy_vertex);
        summary.vertex_hash_mirror = HashVertex(mirror_vertex);
        summary.spv_hash_legacy = HashSpv(legacy_spv);
        summary.spv_hash_mirror = HashSpv(mirror_spv);

        summary.legacy_layout_count = static_cast<uint32_t>(legacy_layout.size());
        summary.mirror_layout_count = static_cast<uint32_t>(mirror_layout.size());
        summary.legacy_vertex_count = static_cast<uint32_t>(legacy_vertex.size());
        summary.mirror_vertex_count = static_cast<uint32_t>(mirror_vertex.size());
        summary.legacy_spv_count = static_cast<uint32_t>(legacy_spv.size());
        summary.mirror_spv_count = static_cast<uint32_t>(mirror_spv.size());

        summary.layout_match = summary.legacy_layout_count == summary.mirror_layout_count
                            && summary.layout_hash_legacy == summary.layout_hash_mirror;
        summary.vertex_match = summary.legacy_vertex_count == summary.mirror_vertex_count
                            && summary.vertex_hash_legacy == summary.vertex_hash_mirror;
        summary.spv_match = summary.legacy_spv_count == summary.mirror_spv_count
                         && summary.spv_hash_legacy == summary.spv_hash_mirror;
        summary.all_match = summary.layout_match && summary.vertex_match && summary.spv_match;

        summary.legacy_stage_summary = BuildStageMaskSummary(legacy_spv);
        summary.mirror_stage_summary = BuildStageMaskSummary(mirror_spv);
        summary.legacy_stage_combo = BuildStageComboMask(legacy_spv);
        summary.mirror_stage_combo = BuildStageComboMask(mirror_spv);

        return true;
    }
}
