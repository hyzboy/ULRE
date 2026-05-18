#include <hgl/graph/module/ShaderMaterialProgramStats.h>
#include <hgl/vk/VKShaderMaterialProgram.h>

namespace hgl::graph
{
    void ShaderMaterialProgramStats::RecordMaterialKeyAxisMismatch(uint32_t mismatch_mask) noexcept
    {
        if(mismatch_mask==0)
            return;

        key_axis_mismatch_total.fetch_add(1, std::memory_order_relaxed);

        if(mismatch_mask & MATERIAL_KEY_MISMATCH_DEF_ID)
            key_axis_mismatch_def_id.fetch_add(1, std::memory_order_relaxed);
        if(mismatch_mask & MATERIAL_KEY_MISMATCH_SCHEMA)
            key_axis_mismatch_schema.fetch_add(1, std::memory_order_relaxed);
        if(mismatch_mask & MATERIAL_KEY_MISMATCH_GLSL_VERSION)
            key_axis_mismatch_glsl.fetch_add(1, std::memory_order_relaxed);
        if(mismatch_mask & MATERIAL_KEY_MISMATCH_VK_VERSION)
            key_axis_mismatch_vk.fetch_add(1, std::memory_order_relaxed);
        if(mismatch_mask & MATERIAL_KEY_MISMATCH_SPV_VERSION)
            key_axis_mismatch_spv.fetch_add(1, std::memory_order_relaxed);
    }

    void ShaderMaterialProgramStats::RecordShaderProgramKeyCoverage(const mtl::VertexProgramKey &vkey,
                                                                    const mtl::FragmentProgramKey &fkey)
    {
        vertex_program_key_seen.fetch_add(1, std::memory_order_relaxed);
        fragment_program_key_seen.fetch_add(1, std::memory_order_relaxed);

        const uint64_t vhash = vkey.Hash();
        const uint64_t fhash = fkey.Hash();

        std::lock_guard<std::mutex> lock(mutex);
        vertex_program_key_unique_hashes.insert(vhash);
        fragment_program_key_unique_hashes.insert(fhash);
    }

    void ShaderMaterialProgramStats::RecordShaderProgramShadowCacheLookup(const mtl::VertexProgramKey &vkey,
                                                                          const mtl::FragmentProgramKey &fkey)
    {
        const uint64_t vhash = vkey.Hash();
        const uint64_t fhash = fkey.Hash();

        bool vhit = false;
        bool fhit = false;
        ShaderMaterialProgram *vprog = nullptr;
        ShaderMaterialProgram *fprog = nullptr;

        {
            std::lock_guard<std::mutex> lock(mutex);

            auto vit = vertex_program_shadow_cache.find(vhash);
            if(vit != vertex_program_shadow_cache.end())
            {
                vhit = true;
                vprog = vit->second;
            }

            auto fit = fragment_program_shadow_cache.find(fhash);
            if(fit != fragment_program_shadow_cache.end())
            {
                fhit = true;
                fprog = fit->second;
            }
        }

        if(vhit) shadow_vertex_hits.fetch_add(1, std::memory_order_relaxed);
        else     shadow_vertex_misses.fetch_add(1, std::memory_order_relaxed);

        if(fhit) shadow_fragment_hits.fetch_add(1, std::memory_order_relaxed);
        else     shadow_fragment_misses.fetch_add(1, std::memory_order_relaxed);

        if(vhit && fhit)
        {
            shadow_combined_hits.fetch_add(1, std::memory_order_relaxed);

            if(vprog == fprog && vprog != nullptr)
                shadow_combined_ptr_match_hits.fetch_add(1, std::memory_order_relaxed);
            else
                shadow_combined_ptr_mismatch_hits.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            shadow_combined_misses.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void ShaderMaterialProgramStats::RecordShaderProgramShadowCacheInsert(const mtl::VertexProgramKey &vkey,
                                                                          const mtl::FragmentProgramKey &fkey,
                                                                          ShaderMaterialProgram *program)
    {
        if(!program)
            return;

        const uint64_t vhash = vkey.Hash();
        const uint64_t fhash = fkey.Hash();

        std::lock_guard<std::mutex> lock(mutex);
        vertex_program_shadow_cache[vhash] = program;
        fragment_program_shadow_cache[fhash] = program;
    }

    MaterialAcquireStats ShaderMaterialProgramStats::GetMaterialAcquireStats() const noexcept
    {
        MaterialAcquireStats s;
        s.requests = acquire_material_requests.load(std::memory_order_relaxed);
        s.cache_lookups = acquire_material_cache_lookups.load(std::memory_order_relaxed);
        s.cache_hits = acquire_material_cache_hits.load(std::memory_order_relaxed);
        s.cache_misses = acquire_material_cache_misses.load(std::memory_order_relaxed);
        s.created = acquire_material_created.load(std::memory_order_relaxed);
        s.fallback_used = acquire_fallback_used.load(std::memory_order_relaxed);
        return s;
    }

    MaterialInstanceAcquireStats ShaderMaterialProgramStats::GetMaterialInstanceAcquireStats() const noexcept
    {
        MaterialInstanceAcquireStats s;
        s.requests = acquire_mi_requests.load(std::memory_order_relaxed);
        s.created = acquire_mi_created.load(std::memory_order_relaxed);
        return s;
    }

    MaterialKeyAxisMismatchStats ShaderMaterialProgramStats::GetMaterialKeyAxisMismatchStats() const noexcept
    {
        MaterialKeyAxisMismatchStats s;
        s.total = key_axis_mismatch_total.load(std::memory_order_relaxed);
        s.def_id = key_axis_mismatch_def_id.load(std::memory_order_relaxed);
        s.schema = key_axis_mismatch_schema.load(std::memory_order_relaxed);
        s.glsl_version = key_axis_mismatch_glsl.load(std::memory_order_relaxed);
        s.vk_version = key_axis_mismatch_vk.load(std::memory_order_relaxed);
        s.spv_version = key_axis_mismatch_spv.load(std::memory_order_relaxed);
        return s;
    }

    ShaderProgramKeyCoverageStats ShaderMaterialProgramStats::GetShaderProgramKeyCoverageStats() const
    {
        ShaderProgramKeyCoverageStats s;
        s.vertex_seen = vertex_program_key_seen.load(std::memory_order_relaxed);
        s.fragment_seen = fragment_program_key_seen.load(std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(mutex);
        s.vertex_unique = static_cast<uint64_t>(vertex_program_key_unique_hashes.size());
        s.fragment_unique = static_cast<uint64_t>(fragment_program_key_unique_hashes.size());
        return s;
    }

    ShaderProgramKeyShadowCacheStats ShaderMaterialProgramStats::GetShaderProgramKeyShadowCacheStats() const
    {
        ShaderProgramKeyShadowCacheStats s;
        s.vertex_hits = shadow_vertex_hits.load(std::memory_order_relaxed);
        s.vertex_misses = shadow_vertex_misses.load(std::memory_order_relaxed);
        s.fragment_hits = shadow_fragment_hits.load(std::memory_order_relaxed);
        s.fragment_misses = shadow_fragment_misses.load(std::memory_order_relaxed);
        s.combined_hits = shadow_combined_hits.load(std::memory_order_relaxed);
        s.combined_misses = shadow_combined_misses.load(std::memory_order_relaxed);
        s.combined_pointer_match_hits = shadow_combined_ptr_match_hits.load(std::memory_order_relaxed);
        s.combined_pointer_mismatch_hits = shadow_combined_ptr_mismatch_hits.load(std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(mutex);
        s.vertex_entries = static_cast<uint64_t>(vertex_program_shadow_cache.size());
        s.fragment_entries = static_cast<uint64_t>(fragment_program_shadow_cache.size());
        return s;
    }

    void ShaderMaterialProgramStats::Reset()
    {
        acquire_material_requests.store(0, std::memory_order_relaxed);
        acquire_material_cache_lookups.store(0, std::memory_order_relaxed);
        acquire_material_cache_hits.store(0, std::memory_order_relaxed);
        acquire_material_cache_misses.store(0, std::memory_order_relaxed);
        acquire_material_created.store(0, std::memory_order_relaxed);
        acquire_fallback_used.store(0, std::memory_order_relaxed);
        acquire_mi_requests.store(0, std::memory_order_relaxed);
        acquire_mi_created.store(0, std::memory_order_relaxed);
        by_key_hits.store(0, std::memory_order_relaxed);

        key_axis_mismatch_total.store(0, std::memory_order_relaxed);
        key_axis_mismatch_def_id.store(0, std::memory_order_relaxed);
        key_axis_mismatch_schema.store(0, std::memory_order_relaxed);
        key_axis_mismatch_glsl.store(0, std::memory_order_relaxed);
        key_axis_mismatch_vk.store(0, std::memory_order_relaxed);
        key_axis_mismatch_spv.store(0, std::memory_order_relaxed);

        vertex_program_key_seen.store(0, std::memory_order_relaxed);
        fragment_program_key_seen.store(0, std::memory_order_relaxed);

        {
            std::lock_guard<std::mutex> lock(mutex);
            vertex_program_key_unique_hashes.clear();
            fragment_program_key_unique_hashes.clear();
            vertex_program_shadow_cache.clear();
            fragment_program_shadow_cache.clear();
        }

        shadow_vertex_hits.store(0, std::memory_order_relaxed);
        shadow_vertex_misses.store(0, std::memory_order_relaxed);
        shadow_fragment_hits.store(0, std::memory_order_relaxed);
        shadow_fragment_misses.store(0, std::memory_order_relaxed);
        shadow_combined_hits.store(0, std::memory_order_relaxed);
        shadow_combined_misses.store(0, std::memory_order_relaxed);
        shadow_combined_ptr_match_hits.store(0, std::memory_order_relaxed);
        shadow_combined_ptr_mismatch_hits.store(0, std::memory_order_relaxed);
    }

    void ShaderMaterialProgramStats::DumpKeyMapDiagnostics(size_t material_by_key_size) const
    {
        const auto key_stats = GetShaderProgramKeyCoverageStats();
        const auto shadow_stats = GetShaderProgramKeyShadowCacheStats();

        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] KeyMap: by_key=%zu hits=%llu "
            "vkey(seen=%llu unique=%llu) fkey(seen=%llu unique=%llu) "
            "shadow(vhit=%llu vmiss=%llu fhit=%llu fmiss=%llu chit=%llu cmiss=%llu "
            "cptr_match=%llu cptr_mismatch=%llu ventry=%llu fentry=%llu)\n",
            material_by_key_size,
            static_cast<unsigned long long>(GetByKeyHits()),
            static_cast<unsigned long long>(key_stats.vertex_seen),
            static_cast<unsigned long long>(key_stats.vertex_unique),
            static_cast<unsigned long long>(key_stats.fragment_seen),
            static_cast<unsigned long long>(key_stats.fragment_unique),
            static_cast<unsigned long long>(shadow_stats.vertex_hits),
            static_cast<unsigned long long>(shadow_stats.vertex_misses),
            static_cast<unsigned long long>(shadow_stats.fragment_hits),
            static_cast<unsigned long long>(shadow_stats.fragment_misses),
            static_cast<unsigned long long>(shadow_stats.combined_hits),
            static_cast<unsigned long long>(shadow_stats.combined_misses),
            static_cast<unsigned long long>(shadow_stats.combined_pointer_match_hits),
            static_cast<unsigned long long>(shadow_stats.combined_pointer_mismatch_hits),
            static_cast<unsigned long long>(shadow_stats.vertex_entries),
            static_cast<unsigned long long>(shadow_stats.fragment_entries));

        const uint64_t combined_hits = shadow_stats.combined_hits;
        const uint64_t mismatch_hits = shadow_stats.combined_pointer_mismatch_hits;
        const bool has_shadow_sample = combined_hits >= 64;
        const double mismatch_ratio = combined_hits > 0
            ? (double(mismatch_hits) / double(combined_hits))
            : 1.0;

        const bool ready_for_shadow_cache_rollout = has_shadow_sample && mismatch_ratio <= 0.02;

        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] ShadowCacheHint: sample=%llu mismatch_ratio=%.4f ready=%s "
            "(rule: combined_hits>=64 && mismatch_ratio<=0.02)\n",
            static_cast<unsigned long long>(combined_hits),
            mismatch_ratio,
            ready_for_shadow_cache_rollout ? "true" : "false");
    }

    void ShaderMaterialProgramStats::LogProgramKeyTriplet(const char *phase,
                                                          const mtl::MaterialKey &material_key,
                                                          const PrimitiveType primitive,
                                                          const bool has_local_to_world) const noexcept
    {
        const mtl::VertexProgramKey vkey = mtl::BuildVertexProgramKey(material_key.variant,
                                                                       has_local_to_world);
        const mtl::FragmentProgramKey fkey = mtl::BuildFragmentProgramKey(material_key.variant);

        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] key_triplet(%s): material=0x%016llx vertex=0x%016llx fragment=0x%016llx\n",
            phase ? phase : "unknown",
            static_cast<unsigned long long>(material_key.Hash()),
            static_cast<unsigned long long>(vkey.Hash()),
            static_cast<unsigned long long>(fkey.Hash()));
    }

    void ShaderMaterialProgramStats::LogEffectiveFeatureMaskConsistency(const ShaderMaterialProgram *prog,
                                                                        const mtl::MaterialKey &request_key,
                                                                        const char *phase) const
    {
        if(!prog)
            return;

        const uint64_t program_mask = prog->GetEffectiveFeatureMask();
        const uint64_t request_mask = request_key.variant.effective_feature_mask;

        if(program_mask != request_mask)
        {
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] effective_feature_mask drift (%s): program=0x%016llx request=0x%016llx material='%s'\n",
                phase ? phase : "unknown",
                static_cast<unsigned long long>(program_mask),
                static_cast<unsigned long long>(request_mask),
                prog->GetName().c_str());
        }
    }

    void ShaderMaterialProgramStats::LogMaterialKeyAxisMismatchDetails(const mtl::MaterialKey &cached_key,
                                                                       const mtl::MaterialKey &request_key,
                                                                       const uint32_t mismatch_mask) const
    {
        if(mismatch_mask == 0)
            return;

        if (mismatch_mask & MATERIAL_KEY_MISMATCH_DEF_ID)
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] MaterialKey axis mismatch: def_id cached=%u request=%u\n",
                static_cast<unsigned>(cached_key.def_id),
                static_cast<unsigned>(request_key.def_id));

        if (mismatch_mask & MATERIAL_KEY_MISMATCH_SCHEMA)
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] MaterialKey axis mismatch: schema cached=%u request=%u\n",
                static_cast<unsigned>(cached_key.schema),
                static_cast<unsigned>(request_key.schema));

        if (mismatch_mask & MATERIAL_KEY_MISMATCH_GLSL_VERSION)
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] MaterialKey axis mismatch: glsl_version cached=%u request=%u\n",
                static_cast<unsigned>(cached_key.glsl_version),
                static_cast<unsigned>(request_key.glsl_version));

        if (mismatch_mask & MATERIAL_KEY_MISMATCH_VK_VERSION)
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] MaterialKey axis mismatch: vk_version cached=%u request=%u\n",
                static_cast<unsigned>(cached_key.vk_version),
                static_cast<unsigned>(request_key.vk_version));

        if (mismatch_mask & MATERIAL_KEY_MISMATCH_SPV_VERSION)
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] MaterialKey axis mismatch: spv_version cached=%u request=%u\n",
                static_cast<unsigned>(cached_key.spv_version),
                static_cast<unsigned>(request_key.spv_version));
    }

    void ShaderMaterialProgramStats::LogGetOrCreateProgramByKeyRequest(const uint64_t key_hash,
                                                                        const uint32_t recipe_prim,
                                                                        const uint32_t recipe_preset,
                                                                        const uint32_t recipe_pipeline) const
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] GetOrCreateProgramByKey: key_hash=0x%llx recipe_prim=%u preset=%u pipeline=%u\n",
            static_cast<unsigned long long>(key_hash),
            static_cast<unsigned>(recipe_prim),
            static_cast<unsigned>(recipe_preset),
            static_cast<unsigned>(recipe_pipeline));
    }

    void ShaderMaterialProgramStats::LogGetOrCreateProgramByKeyCacheHit(const ShaderMaterialProgram *program) const
    {
        if(!program)
            return;

        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] GetOrCreateProgramByKey: cache_hit material=%p name='%s' material_prim=%u\n",
            program,
            program->GetName().c_str(),
            static_cast<unsigned>(program->GetPrimitiveType()));
    }

    void ShaderMaterialProgramStats::LogGetOrCreateProgramByKeyCreated(const ShaderMaterialProgram *program) const
    {
        if(!program)
            return;

        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] GetOrCreateProgramByKey: created material=%p name='%s' material_prim=%u\n",
            program,
            program->GetName().c_str(),
            static_cast<unsigned>(program->GetPrimitiveType()));
    }

    void ShaderMaterialProgramStats::LogGetOrCreateProgramByKeyAliasWarning(const uint64_t key_hash) const
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] WARN GetOrCreateProgramByKey: "
            "key.Hash=0x%llx not in material_by_key after creation — "
            "MaterialKey derivation may be inconsistent.\n",
            static_cast<unsigned long long>(key_hash));
    }

    void ShaderMaterialProgramStats::LogCreateMaterialFromRecordRequest(const uint32_t preset,
                                                                         const uint32_t dim,
                                                                         const uint32_t prim,
                                                                         const bool l2w,
                                                                         const uint32_t pipeline,
                                                                         const uint64_t key_hash) const
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterialFromRecord: preset=%u dim=%u prim=%u l2w=%u pipeline=%u key_hash=0x%llx\n",
            static_cast<unsigned>(preset),
            static_cast<unsigned>(dim),
            static_cast<unsigned>(prim),
            l2w ? 1u : 0u,
            static_cast<unsigned>(pipeline),
            static_cast<unsigned long long>(key_hash));
    }

    void ShaderMaterialProgramStats::LogCreateMaterialFromRecordBillboard(const int preset,
                                                                           const int use_texture_array,
                                                                           const int blend_mode) const
    {
        std::fprintf(stderr,
            "[CreateMaterialFromRecord] Billboard preset=%d  use_texture_array=%d  blend=%d\n",
            preset,
            use_texture_array,
            blend_mode);
    }

    void ShaderMaterialProgramStats::LogCreateMaterialFromRecord2D(const uint32_t prim,
                                                                    const uint32_t preset) const
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterialFromRecord: 2D cfg.prim=%u preset=%u\n",
            static_cast<unsigned>(prim),
            static_cast<unsigned>(preset));
    }

    void ShaderMaterialProgramStats::LogCreateMaterialFromRecord3D(const uint32_t prim,
                                                                    const uint32_t preset) const
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterialFromRecord: 3D cfg.prim=%u preset=%u\n",
            static_cast<unsigned>(prim),
            static_cast<unsigned>(preset));
    }

    void ShaderMaterialProgramStats::LogMaterialFinalizeSummary(const AnsiString &material_name,
                                                                const uint32_t mi_data_bytes,
                                                                const uint32_t mi_max_count,
                                                                const uint32_t schema,
                                                                const std::string &schema_file,
                                                                const size_t descriptor_set_count) const
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] Finalize material='%s' mi_bytes=%u mi_max=%u schema=%u schema_file=%s descriptor_sets=%zu\n",
            material_name.c_str(),
            mi_data_bytes,
            mi_max_count,
            schema,
            schema_file.empty() ? "<none>" : schema_file.c_str(),
            descriptor_set_count);
    }

    void ShaderMaterialProgramStats::LogFallbackMaterialInitialized(const AnsiString &material_name) const
    {
        std::fprintf(stdout,
            "[ShaderMaterialProgramManager] Fallback material initialized: default checkerboard name=%s\n",
            material_name.c_str());
    }

    void ShaderMaterialProgramStats::LogBillboardDomainArrayKey(const uint64_t cache_key_hash) const
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] Billboard domain: use_texture_array=true cache_key_hash=%llu\n",
            static_cast<unsigned long long>(cache_key_hash));
    }

    void ShaderMaterialProgramStats::LogCreateMaterialKey3DProfileNull(const uint64_t key_hash) const
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterial(key/3D) warning: physical device profile is null (key_hash=%llu)\n",
            static_cast<unsigned long long>(key_hash));
    }

    void ShaderMaterialProgramStats::LogCreateMaterialKey3DCreateInfoFailed(const mtl::MaterialVariantKey &key,
                                                                             const std::string &cfg_hash) const
    {
        std::fprintf(stderr,
            "[ShaderMaterialProgramManager] CreateMaterial(key/3D) failed: CreateMaterialCreateInfo returned null (key_hash=%llu surface=%u geom=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X cfg_hash=%s)\n",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.geometry_mode),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits,
            cfg_hash.c_str());
    }

    void ShaderMaterialProgramStats::LogLine(const std::string &line) const
    {
        if(!line.empty())
            std::fprintf(stderr, "%s\n", line.c_str());
    }
}
