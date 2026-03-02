#include <hgl/graph/module/RendererShaderGenAdapter.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/contract/ShaderGenResultBuilder.h>
#include <unordered_set>
#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <mutex>

namespace hgl::graph
{
    namespace
    {
        struct ShaderGenProfilerStorage
        {
            RendererShaderGenAdapter::ProfilerSnapshot snapshot;
            std::mutex mutex;
        };

        ShaderGenProfilerStorage &GetShaderGenProfilerStorage()
        {
            static ShaderGenProfilerStorage storage;
            return storage;
        }
    }//namespace

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

    static int BoolToInt(const bool v)
    {
        return v ? 1 : 0;
    }

    static mtl::contract::ResourceClass ToResourceClass(const VkDescriptorType desc_type)
    {
        switch(desc_type)
        {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return mtl::contract::ResourceClass::UniformBuffer;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return mtl::contract::ResourceClass::StorageBuffer;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return mtl::contract::ResourceClass::SampledImage;
            case VK_DESCRIPTOR_TYPE_SAMPLER: return mtl::contract::ResourceClass::Sampler;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return mtl::contract::ResourceClass::CombinedImageSampler;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return mtl::contract::ResourceClass::InputAttachment;
            default: return mtl::contract::ResourceClass::Unknown;
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

    static void BuildLegacyLayoutSigs(const mtl::MaterialCreateInfo &mci, std::vector<LayoutSig> &out)
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

    static void BuildMirrorLayoutSigs(const mtl::contract::ShaderGenResult &result, std::vector<LayoutSig> &out)
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

    static void BuildLegacyVertexSigs(const mtl::MaterialCreateInfo &mci, std::vector<VertexSig> &out)
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

    static void BuildMirrorVertexSigs(const mtl::contract::ShaderGenResult &result, std::vector<VertexSig> &out)
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

    static void RecordProfilerSample(const bool all_match,
                                     const bool layout_match,
                                     const bool vertex_match,
                                     const bool spv_match,
                                     const uint32_t legacy_stage_combo,
                                     const uint32_t mirror_stage_combo,
                                     const size_t legacy_layout_count,
                                     const size_t mirror_layout_count,
                                     const size_t legacy_vertex_count,
                                     const size_t mirror_vertex_count,
                                     const size_t legacy_spv_count,
                                     const size_t mirror_spv_count)
    {
        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);

        auto &snapshot = storage.snapshot;
        ++snapshot.sample_count;

        if (all_match) ++snapshot.all_match_count;
        if (layout_match) ++snapshot.layout_match_count;
        if (vertex_match) ++snapshot.vertex_match_count;
        if (spv_match) ++snapshot.spv_match_count;

        snapshot.legacy_layout_count_sum += static_cast<uint64_t>(legacy_layout_count);
        snapshot.mirror_layout_count_sum += static_cast<uint64_t>(mirror_layout_count);
        snapshot.legacy_vertex_count_sum += static_cast<uint64_t>(legacy_vertex_count);
        snapshot.mirror_vertex_count_sum += static_cast<uint64_t>(mirror_vertex_count);
        snapshot.legacy_spv_count_sum += static_cast<uint64_t>(legacy_spv_count);
        snapshot.mirror_spv_count_sum += static_cast<uint64_t>(mirror_spv_count);

        ++snapshot.legacy_stage_combo_histogram[legacy_stage_combo];
        ++snapshot.mirror_stage_combo_histogram[mirror_stage_combo];
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

    static void BuildLegacySpvSigs(const mtl::MaterialCreateInfo &mci, std::vector<SpvSig> &out)
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

    static void BuildMirrorSpvSigs(const mtl::contract::ShaderGenResult &result, std::vector<SpvSig> &out)
    {
        for (const auto &blob : result.spv_per_stage)
        {
            SpvSig sig;
            sig.stage_mask = blob.stage_mask;
            sig.words = blob.words;
            out.emplace_back(std::move(sig));
        }
    }

    static bool PrintLegacyMirrorDiff(const mtl::MaterialCreateInfo &mci, const mtl::contract::ShaderGenResult &result, const char *material_name, const RendererShaderGenAdapter::DiffLogDetail detail)
    {
        const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";

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

        const uint64_t layout_hash_legacy = HashLayout(legacy_layout);
        const uint64_t layout_hash_mirror = HashLayout(mirror_layout);
        const uint64_t vertex_hash_legacy = HashVertex(legacy_vertex);
        const uint64_t vertex_hash_mirror = HashVertex(mirror_vertex);
        const uint64_t spv_hash_legacy = HashSpv(legacy_spv);
        const uint64_t spv_hash_mirror = HashSpv(mirror_spv);

        const bool layout_match = legacy_layout.size() == mirror_layout.size() && layout_hash_legacy == layout_hash_mirror;
        const bool vertex_match = legacy_vertex.size() == mirror_vertex.size() && vertex_hash_legacy == vertex_hash_mirror;
        const bool spv_match = legacy_spv.size() == mirror_spv.size() && spv_hash_legacy == spv_hash_mirror;
        const bool all_match = layout_match && vertex_match && spv_match;

        const std::string legacy_stage_summary = BuildStageMaskSummary(legacy_spv);
        const std::string mirror_stage_summary = BuildStageMaskSummary(mirror_spv);
        const uint32_t legacy_stage_combo = BuildStageComboMask(legacy_spv);
        const uint32_t mirror_stage_combo = BuildStageComboMask(mirror_spv);

        RecordProfilerSample(all_match,
                     layout_match,
                     vertex_match,
                     spv_match,
                     legacy_stage_combo,
                     mirror_stage_combo,
                     legacy_layout.size(),
                     mirror_layout.size(),
                     legacy_vertex.size(),
                     mirror_vertex.size(),
                     legacy_spv.size(),
                     mirror_spv.size());

        if(detail == RendererShaderGenAdapter::DiffLogDetail::Full)
        {
            std::fprintf(stderr,
                "[RendererShaderGenAdapter][DiffKV] material=%s event=layout legacy_count=%u mirror_count=%u legacy_hash=0x%llx mirror_hash=0x%llx match=%d\n",
                mat_name,
                static_cast<unsigned>(legacy_layout.size()),
                static_cast<unsigned>(mirror_layout.size()),
                static_cast<unsigned long long>(layout_hash_legacy),
                static_cast<unsigned long long>(layout_hash_mirror),
                BoolToInt(layout_match));

            std::fprintf(stderr,
                "[RendererShaderGenAdapter][DiffKV] material=%s event=vertex legacy_count=%u mirror_count=%u legacy_hash=0x%llx mirror_hash=0x%llx match=%d\n",
                mat_name,
                static_cast<unsigned>(legacy_vertex.size()),
                static_cast<unsigned>(mirror_vertex.size()),
                static_cast<unsigned long long>(vertex_hash_legacy),
                static_cast<unsigned long long>(vertex_hash_mirror),
                BoolToInt(vertex_match));

            std::fprintf(stderr,
                "[RendererShaderGenAdapter][DiffKV] material=%s event=spv legacy_count=%u mirror_count=%u legacy_hash=0x%llx mirror_hash=0x%llx legacy_stages=%s mirror_stages=%s match=%d\n",
                mat_name,
                static_cast<unsigned>(legacy_spv.size()),
                static_cast<unsigned>(mirror_spv.size()),
                static_cast<unsigned long long>(spv_hash_legacy),
                static_cast<unsigned long long>(spv_hash_mirror),
                legacy_stage_summary.c_str(),
                mirror_stage_summary.c_str(),
                BoolToInt(spv_match));
        }

        std::fprintf(stderr,
            "[RendererShaderGenAdapter][DiffKV] material=%s event=summary match=%d\n",
            mat_name,
            BoolToInt(all_match));

        return all_match;
    }

    static bool ValidateBindingUniqueness(const mtl::contract::ShaderGenResult &result, const char *material_name)
    {
        std::unordered_set<uint64_t> seen;

        const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";
        bool valid = true;

        for (const auto &binding : result.layout.bindings)
        {
            const uint64_t key = (static_cast<uint64_t>(binding.set) << 32) | static_cast<uint64_t>(binding.binding);
            if (!seen.insert(key).second)
            {
                std::fprintf(stderr,
                    "[RendererShaderGenAdapter] material=%s duplicate binding in mirror layout (set=%u, binding=%u)\n",
                    mat_name,
                    binding.set,
                    binding.binding);
                valid = false;
            }
        }

        return valid;
    }

    bool RendererShaderGenAdapter::ConsumeResultReadOnly(const mtl::contract::ShaderGenResult &result, const char *material_name) const
    {
        const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";
        bool valid = true;

        if (result.contract_version != mtl::contract::kShaderGenContractVersion)
        {
            std::fprintf(stderr,
                "[RendererShaderGenAdapter] material=%s contract_version mismatch (result=%u, expected=%u)\n",
                mat_name,
                result.contract_version,
                mtl::contract::kShaderGenContractVersion);
            valid = false;
        }

        if (result.spv_per_stage.empty())
        {
            std::fprintf(stderr,
                "[RendererShaderGenAdapter] material=%s warning: mirror has no stage SPV blobs\n",
                mat_name);
        }

        for (const auto &blob : result.spv_per_stage)
        {
            if (blob.words.empty())
            {
                std::fprintf(stderr,
                    "[RendererShaderGenAdapter] material=%s empty SPV blob for stage_mask=%u\n",
                    mat_name,
                    blob.stage_mask);
                valid = false;
            }
        }

        if (!ValidateBindingUniqueness(result, mat_name))
            valid = false;

        for (const auto &warn : result.diagnostics.warnings)
            std::fprintf(stderr, "[RendererShaderGenAdapter] material=%s warning: %s\n", mat_name, warn.c_str());

        return valid;
    }

    bool RendererShaderGenAdapter::ConsumePairReadOnly(const mtl::MaterialCreateInfo &mci, const mtl::contract::ShaderGenResult &result, const char *material_name, DiffLogDetail detail) const
    {
        const bool diff_ok = PrintLegacyMirrorDiff(mci, result, material_name, detail);
        const bool validate_ok = ConsumeResultReadOnly(result, material_name);
        return diff_ok && validate_ok;
    }

    void RendererShaderGenAdapter::ResetProfiler()
    {
        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);
        storage.snapshot = ProfilerSnapshot{};
    }

    RendererShaderGenAdapter::ProfilerSnapshot RendererShaderGenAdapter::GetProfilerSnapshot()
    {
        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);
        return storage.snapshot;
    }

    bool RendererShaderGenAdapter::ConsumeMaterialReadOnly(const mtl::MaterialCreateInfo &mci, const char *material_name, DiffLogDetail detail) const
    {
        mtl::contract::ShaderGenResult result;
        if (!mtl::contract::BuildShaderGenResultFromMaterialCreateInfo(mci, result))
        {
            const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";
            std::fprintf(stderr,
                "[RendererShaderGenAdapter] material=%s failed to build mirror result\n",
                mat_name);
            return false;
        }

        return ConsumePairReadOnly(mci, result, material_name, detail);
    }
}//namespace hgl::graph
