#include <hgl/graph/module/RendererShaderGenAdapter.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/contract/ShaderGenContractValidator.h>
#include <hgl/shadergen/contract/ShaderGenResultBuilder.h>
#include <unordered_set>
#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <deque>

namespace hgl::graph
{
    namespace
    {
        struct ShaderGenProfilerStorage
        {
            RendererShaderGenAdapter::ProfilerSnapshot snapshot;
            RendererShaderGenAdapter::ValidationReport last_report;
            std::string last_report_material_name;
            bool has_last_report = false;
            uint64_t report_sequence = 0;
            std::deque<RendererShaderGenAdapter::ValidationReportRecord> history_reports;
            std::mutex mutex;
        };

        ShaderGenProfilerStorage &GetShaderGenProfilerStorage()
        {
            static ShaderGenProfilerStorage storage;
            return storage;
        }

        void AddWarning(RendererShaderGenAdapter::ValidationReport &report, const std::string &message)
        {
            report.warnings.emplace_back(message);
            ++report.warning_count;
        }

        void AddError(RendererShaderGenAdapter::ValidationReport &report, const std::string &message)
        {
            report.errors.emplace_back(message);
            ++report.error_count;
            report.overall_valid = false;
        }

        void StoreValidationReport(const char *material_name, const RendererShaderGenAdapter::ValidationReport &report)
        {
            auto &storage = GetShaderGenProfilerStorage();
            std::lock_guard<std::mutex> lock(storage.mutex);

            const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";

            storage.last_report = report;
            storage.last_report_material_name = mat_name;
            storage.has_last_report = true;

            RendererShaderGenAdapter::ValidationReportRecord rec;
            rec.sequence = ++storage.report_sequence;
            rec.material_name = mat_name;
            rec.report = report;

            storage.history_reports.emplace_back(std::move(rec));

            constexpr size_t kMaxValidationHistory = 512;
            while (storage.history_reports.size() > kMaxValidationHistory)
                storage.history_reports.pop_front();
        }

        void MergeValidationReport(RendererShaderGenAdapter::ValidationReport &dst, const RendererShaderGenAdapter::ValidationReport &src)
        {
            dst.overall_valid = dst.overall_valid && src.overall_valid;
            dst.warning_count += src.warning_count;
            dst.error_count += src.error_count;

            dst.warnings.insert(dst.warnings.end(), src.warnings.begin(), src.warnings.end());
            dst.errors.insert(dst.errors.end(), src.errors.begin(), src.errors.end());
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

    static bool PrintLegacyMirrorDiff(const mtl::MaterialCreateInfo &mci,
                                      const mtl::contract::ShaderGenResult &result,
                                      const char *material_name,
                                      const RendererShaderGenAdapter::DiffLogDetail detail,
                                      RendererShaderGenAdapter::ValidationReport *out_report)
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

        if (!all_match && out_report)
        {
            char msg[512] = {};
            std::snprintf(msg,
                sizeof(msg),
                "material=%s legacy/mirror diff mismatch: layout=%d vertex=%d spv=%d legacy_stages=%s mirror_stages=%s",
                mat_name,
                BoolToInt(layout_match),
                BoolToInt(vertex_match),
                BoolToInt(spv_match),
                legacy_stage_summary.c_str(),
                mirror_stage_summary.c_str());
            AddError(*out_report, msg);
            out_report->diff_valid = false;
        }

        return all_match;
    }

    static bool ValidateBindingUniqueness(const mtl::contract::ShaderGenResult &result,
                                          const char *material_name,
                                          RendererShaderGenAdapter::ValidationReport *out_report)
    {
        std::unordered_set<uint64_t> seen;

        const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";
        bool valid = true;

        for (const auto &binding : result.layout.bindings)
        {
            const uint64_t key = (static_cast<uint64_t>(binding.set) << 32) | static_cast<uint64_t>(binding.binding);
            if (!seen.insert(key).second)
            {
                if (out_report)
                {
                    char msg[256] = {};
                    std::snprintf(msg,
                        sizeof(msg),
                        "material=%s duplicate binding in mirror layout (set=%u, binding=%u)",
                        mat_name,
                        binding.set,
                        binding.binding);
                    AddError(*out_report, msg);
                }
                valid = false;
            }
        }

        return valid;
    }

    RendererShaderGenAdapter::ValidationReport RendererShaderGenAdapter::ValidateResultReadOnly(const mtl::contract::ShaderGenResult &result, const char *material_name) const
    {
        const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";
        ValidationReport report;
        report.result_valid = true;

        if (result.contract_version != mtl::contract::kShaderGenContractVersion)
        {
            char msg[256] = {};
            std::snprintf(msg,
                sizeof(msg),
                "material=%s contract_version mismatch (result=%u, expected=%u)",
                mat_name,
                result.contract_version,
                mtl::contract::kShaderGenContractVersion);
            AddError(report, msg);
            report.result_valid = false;
        }

        if (result.spv_per_stage.empty())
        {
            char msg[256] = {};
            std::snprintf(msg,
                sizeof(msg),
                "material=%s mirror has no stage SPV blobs",
                mat_name);
            AddWarning(report, msg);
        }

        for (const auto &blob : result.spv_per_stage)
        {
            if (blob.words.empty())
            {
                char msg[256] = {};
                std::snprintf(msg,
                    sizeof(msg),
                    "material=%s empty SPV blob for stage_mask=%u",
                    mat_name,
                    blob.stage_mask);
                AddError(report, msg);
                report.result_valid = false;
            }
        }

        if (!ValidateBindingUniqueness(result, mat_name, &report))
            report.result_valid = false;

        for (const auto &warn : result.diagnostics.warnings)
            AddWarning(report, std::string("material=") + mat_name + " shader diagnostics warning: " + warn);

        for (const auto &err : result.diagnostics.errors)
        {
            AddError(report, std::string("material=") + mat_name + " shader diagnostics error: " + err);
            report.result_valid = false;
        }

        return report;
    }

    RendererShaderGenAdapter::ValidationReport RendererShaderGenAdapter::ValidatePairReadOnly(const mtl::MaterialCreateInfo &mci, const mtl::contract::ShaderGenResult &result, const char *material_name, DiffLogDetail detail) const
    {
        ValidationReport report;
        report.diff_valid = true;
        report.result_valid = true;

        const bool diff_ok = PrintLegacyMirrorDiff(mci, result, material_name, detail, &report);
        if (!diff_ok)
            report.diff_valid = false;

        const ValidationReport result_report = ValidateResultReadOnly(result, material_name);
        report.result_valid = result_report.result_valid;
        MergeValidationReport(report, result_report);

        report.overall_valid = report.diff_valid && report.result_valid && report.request_result_valid && report.error_count == 0;

        StoreValidationReport(material_name, report);
        return report;
    }

    RendererShaderGenAdapter::ValidationReport RendererShaderGenAdapter::ValidateRequestResultReadOnly(const mtl::contract::ShaderGenRequest &request, const mtl::contract::ShaderGenResult &result, const char *material_name) const
    {
        ValidationReport report;
        report.request_result_valid = true;

        const auto contract_check = mtl::contract::ValidateShaderGenRequestResult(request, result, material_name);
        report.request_result_valid = contract_check.valid;

        report.warning_count += contract_check.warning_count;
        report.error_count += contract_check.error_count;
        report.warnings.insert(report.warnings.end(), contract_check.warnings.begin(), contract_check.warnings.end());
        report.errors.insert(report.errors.end(), contract_check.errors.begin(), contract_check.errors.end());

        if (!contract_check.valid)
            report.overall_valid = false;

        report.overall_valid = report.diff_valid && report.result_valid && report.request_result_valid && report.error_count == 0;
        StoreValidationReport(material_name, report);
        return report;
    }

    bool RendererShaderGenAdapter::ConsumeResultReadOnly(const mtl::contract::ShaderGenResult &result, const char *material_name) const
    {
        const ValidationReport report = ValidateResultReadOnly(result, material_name);
        StoreValidationReport(material_name, report);
        return report.overall_valid;
    }

    bool RendererShaderGenAdapter::ConsumePairReadOnly(const mtl::MaterialCreateInfo &mci, const mtl::contract::ShaderGenResult &result, const char *material_name, DiffLogDetail detail) const
    {
        const ValidationReport report = ValidatePairReadOnly(mci, result, material_name, detail);
        return report.overall_valid;
    }

    bool RendererShaderGenAdapter::ConsumeRequestResultReadOnly(const mtl::contract::ShaderGenRequest &request, const mtl::contract::ShaderGenResult &result, const char *material_name) const
    {
        const ValidationReport report = ValidateRequestResultReadOnly(request, result, material_name);
        return report.overall_valid;
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

    bool RendererShaderGenAdapter::GetLastValidationReport(ValidationReport &out_report, std::string *out_material_name)
    {
        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);

        if (!storage.has_last_report)
            return false;

        out_report = storage.last_report;

        if (out_material_name)
            *out_material_name = storage.last_report_material_name;

        return true;
    }

    std::vector<RendererShaderGenAdapter::ValidationReportRecord> RendererShaderGenAdapter::GetRecentValidationReports(uint32_t max_count)
    {
        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);

        std::vector<ValidationReportRecord> out;
        if (max_count == 0 || storage.history_reports.empty())
            return out;

        const size_t take = std::min<size_t>(max_count, storage.history_reports.size());
        out.reserve(take);

        auto it = storage.history_reports.rbegin();
        for (size_t i = 0; i < take && it != storage.history_reports.rend(); ++i, ++it)
            out.emplace_back(*it);

        return out;
    }

    std::map<std::string, std::vector<RendererShaderGenAdapter::ValidationReportRecord>> RendererShaderGenAdapter::GetRecentValidationReportsByMaterial(uint32_t max_per_material, uint32_t max_total)
    {
        std::map<std::string, std::vector<ValidationReportRecord>> grouped;

        if (max_per_material == 0 || max_total == 0)
            return grouped;

        const auto recent = GetRecentValidationReports(max_total);

        for (const auto &record : recent)
        {
            auto &bucket = grouped[record.material_name];
            if (bucket.size() >= max_per_material)
                continue;

            bucket.emplace_back(record);
        }

        return grouped;
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
