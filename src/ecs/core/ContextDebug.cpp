#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/vk/VKMaterial.h>
#include<algorithm>
#include<set>

namespace hgl
{
    namespace ecs
    {
        bool ECSContext::GetDescriptorContractDiagnostics(uint32_t &materials_checked,
                                                          uint32_t &materials_unresolved,
                                                          uint32_t &required_missing,
                                                          uint32_t &optional_missing,
                                                          uint32_t &fallback_hits) const
        {
            materials_checked = 0;
            materials_unresolved = 0;
            required_missing = 0;
            optional_missing = 0;
            fallback_hits = 0;

        #if !ULRE_ECS_DEBUG_API
            return false;
        #else
            auto descriptor_system = GetSystem<RenderDescriptorBindingSystem>();
            if (!descriptor_system)
                return false;

            return descriptor_system->GetContractDiagnosticsStats(materials_checked,
                                                                  materials_unresolved,
                                                                  required_missing,
                                                                  optional_missing,
                                                                  fallback_hits);
        #endif
        }

        bool ECSContext::GetDescriptorContractDiagnosticsExtended(uint32_t &materials_checked,
                                                                  uint32_t &materials_unresolved,
                                                                  uint32_t &required_missing,
                                                                  uint32_t &optional_missing,
                                                                  uint32_t &fallback_hits,
                                                                  uint32_t &materials_registered,
                                                                  uint32_t &binding_entries) const
        {
            materials_checked = 0;
            materials_unresolved = 0;
            required_missing = 0;
            optional_missing = 0;
            fallback_hits = 0;
            materials_registered = 0;
            binding_entries = 0;

        #if !ULRE_ECS_DEBUG_API
            return false;
        #else
            auto descriptor_system = GetSystem<RenderDescriptorBindingSystem>();
            if (!descriptor_system)
                return false;

            if (!descriptor_system->GetContractDiagnosticsStats(materials_checked,
                                                                materials_unresolved,
                                                                required_missing,
                                                                optional_missing,
                                                                fallback_hits))
                return false;

            descriptor_system->GetMaterialBindingRegistryStats(materials_registered,
                                                               binding_entries);
            return true;
        #endif
        }

        bool ECSContext::GetMaterialBindingRegistryStats(uint32_t &materials_registered,
                                                         uint32_t &binding_entries) const
        {
            materials_registered = 0;
            binding_entries = 0;

        #if !ULRE_ECS_DEBUG_API
            return false;
        #else
            auto descriptor_system = GetSystem<RenderDescriptorBindingSystem>();
            if (!descriptor_system)
                return false;

            return descriptor_system->GetMaterialBindingRegistryStats(materials_registered,
                                                                      binding_entries);
        #endif
        }

        bool ECSContext::GetMaterialBindingKeys(const hgl::graph::Material *material,
                                                std::vector<std::string> &out_keys) const
        {
            out_keys.clear();

        #if !ULRE_ECS_DEBUG_API
            return false;
        #else
            auto descriptor_system = GetSystem<RenderDescriptorBindingSystem>();
            if (!descriptor_system)
                return false;

            return descriptor_system->GetMaterialBindingKeys(material, out_keys);
        #endif
        }

        bool ECSContext::GetMaterialBindingKeysByName(const AnsiString &material_name,
                                                      std::vector<std::string> &out_keys) const
        {
            out_keys.clear();

            if (material_name.IsEmpty())
                return false;

        #if !ULRE_ECS_DEBUG_API
            return false;
        #else
            const hgl::graph::Material *matched_material = nullptr;

            for (const auto &pair : render_frame_cache.materialBatches)
            {
                const hgl::graph::Material *material = pair.first.material;
                if (!material)
                    continue;

                if (material->GetName() == material_name)
                {
                    matched_material = material;
                    break;
                }
            }

            if (!matched_material)
            {
                if (material_binding_query_log_enabled)
                {
                    std::vector<std::string> active_material_names;
                    active_material_names.reserve(render_frame_cache.materialBatches.GetCount());

                    for (const auto &pair : render_frame_cache.materialBatches)
                    {
                        const hgl::graph::Material *material = pair.first.material;
                        if (!material)
                            continue;

                        const AnsiString &name = material->GetName();
                        if (name.IsEmpty())
                            continue;

                        active_material_names.emplace_back(name.c_str());
                    }

                    std::sort(active_material_names.begin(), active_material_names.end());
                    active_material_names.erase(std::unique(active_material_names.begin(), active_material_names.end()), active_material_names.end());

                    std::string preview;
                    constexpr size_t preview_limit = 8;
                    for (size_t i = 0; i < active_material_names.size() && i < preview_limit; ++i)
                    {
                        if (!preview.empty())
                            preview += ", ";

                        preview += active_material_names[i];
                    }

                    GLogInfo("[DescriptorBinding][ECSContext] material lookup failed: name='%s', active_count=%zu, preview=[%s]",
                             material_name.c_str() ? material_name.c_str() : "",
                             active_material_names.size(),
                             preview.c_str());
                }

                return false;
            }

            return GetMaterialBindingKeys(matched_material, out_keys);
        #endif
        }

        bool ECSContext::GetMaterialBatchSpecHashStats(uint32_t &active_batch_count,
                                                       uint32_t &total_item_count,
                                                       uint32_t &unique_material_pipeline_count,
                                                       uint32_t &unique_spec_hash_count,
                                                       uint32_t &split_material_pipeline_count) const
        {
            active_batch_count = 0;
            total_item_count = 0;
            unique_material_pipeline_count = 0;
            unique_spec_hash_count = 0;
            split_material_pipeline_count = 0;

        #if !ULRE_ECS_DEBUG_API
            return false;
        #else
            using MaterialPipelinePair = std::pair<const hgl::graph::Material *, const hgl::graph::Pipeline *>;

            std::set<MaterialPipelinePair> unique_pairs;
            std::set<uint64_t> unique_spec_hashes;
            std::map<MaterialPipelinePair, std::set<uint64_t>> pair_spec_hashes;

            for (const auto &entry : render_frame_cache.materialBatches)
            {
                const MaterialPipelineKey &key = entry.first;
                const MaterialBatch *batch = entry.second.get();

                if (!key.material || !key.pipeline || !batch || batch->items.empty())
                    continue;

                ++active_batch_count;
                total_item_count += static_cast<uint32_t>(batch->items.size());

                const MaterialPipelinePair pair_key{key.material, key.pipeline};
                unique_pairs.insert(pair_key);
                unique_spec_hashes.insert(key.materialization_spec_hash);
                pair_spec_hashes[pair_key].insert(key.materialization_spec_hash);
            }

            unique_material_pipeline_count = static_cast<uint32_t>(unique_pairs.size());
            unique_spec_hash_count = static_cast<uint32_t>(unique_spec_hashes.size());

            for (const auto &entry : pair_spec_hashes)
            {
                if (entry.second.size() > 1)
                    ++split_material_pipeline_count;
            }

            return true;
        #endif
        }

        bool ECSContext::GetShaderGenValidationCategoryHistogram(std::map<std::string, uint32_t> &out_histogram,
                                                                 uint32_t max_count) const
        {
            out_histogram.clear();
            (void)max_count;

            return false;
        }

        bool ECSContext::GetShaderGenValidationMaterialCategoryMatrix(std::map<std::string, std::map<std::string, uint32_t>> &out_matrix,
                                                                      uint32_t max_count) const
        {
            out_matrix.clear();
            (void)max_count;

            return false;
        }
    }
}
