#include <hgl/graph/module/MaterialAssetRegistry.h>
#include <hgl/graph/module/MaterialAssetLoader.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/vk/VKMaterialTemplate.h>
#include <hgl/vk/VKMaterialInstance.h>
#include <hgl/vk/VKMaterialResourceDomain.h>
#include <hgl/vk/VKDomainMaterialBinding.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/vk/VKVertexAttribBuffer.h>

#include <vector>
#include <cstdio>
#include <atomic>

namespace hgl::graph
{

namespace
{
static bool ShouldLogPow2(const uint64_t v)
{
    return v != 0 && ((v & (v - 1)) == 0);
}

static std::atomic<uint64_t> g_resolve_vil_fallback_no_geometry {0};
static std::atomic<uint64_t> g_resolve_vil_fallback_no_vab {0};
static std::atomic<uint64_t> g_resolve_vil_fallback_add_failed {0};
static std::atomic<uint64_t> g_resolve_vil_fallback_create_failed {0};
}

// ── 将 record 中的纹理加载并绑定到 DomainMaterialBinding ─────────────────────

static bool BindDomainTexturesFromRecord(
    DomainMaterialBinding *dmb,
    TextureManager *tm,
    SamplerManager *sm,
    const mtl::MaterialAssetRecord &rec)
{
    if (!dmb || !tm || !sm) return false;

    for (const auto &tc : rec.textures)
    {
        if (tc.path.empty()) continue;
        auto *tex = tm->LoadTexture2D(hgl::ToOSString(tc.path), true);
        if (!tex) return false;
        auto *smp = sm->CreateSampler();
        if (!smp) return false;
        if (!dmb->BindTextureSampler(tc.slot, tex, smp))
            return false;
    }
    return true;
}

// Legacy compatibility: many render paths still bind per-material descriptors
// from MaterialTemplate directly (without domain binding registration).
// Keep this best-effort fallback to avoid unbound sampler validation errors.
static void BindMaterialTexturesCompat(
    MaterialTemplate *material,
    TextureManager *tm,
    SamplerManager *sm,
    const mtl::MaterialAssetRecord &rec)
{
    if (!material || !tm || !sm)
        return;

    for (const auto &tc : rec.textures)
    {
        if (tc.path.empty())
            continue;

        auto *tex = tm->LoadTexture2D(hgl::ToOSString(tc.path), true);
        if (!tex)
            continue;

        auto *smp = sm->CreateSampler();
        if (!smp)
            continue;

        // Non-fatal by design: may already be bound in cached material path.
        material->BindTextureSampler(tc.slot, tex, smp);
    }
}

static const VIL *ResolveVILFromRecord(MaterialTemplate *material, const mtl::MaterialAssetRecord &rec)
{
    (void)rec;

    if (!material)
        return nullptr;

    return material->GetDefaultVIL();
}

static const VIL *ResolveVILFromGeometry(MaterialTemplate *material,
                                         const Geometry *geometry,
                                         const mtl::MaterialAssetRecord &fallback_rec)
{
    if (!material)
        return nullptr;

    if (!geometry)
    {
        const uint64_t n = ++g_resolve_vil_fallback_no_geometry;
        if (ShouldLogPow2(n))
        {
            std::fprintf(stderr,
                "[MaterialAssetRegistry] ResolveVIL fallback(default): geometry missing, material='%s' domain='%s' id='%s' total=%llu\n",
                material->GetName().c_str(),
                fallback_rec.domain_id.c_str(),
                fallback_rec.id.c_str(),
                static_cast<unsigned long long>(n));
        }
        return ResolveVILFromRecord(material, fallback_rec);
    }

    VILConfig vil_cfg;
    bool has_any = false;

    for (int i = 0; i < static_cast<int>(VAN::RANGE_SIZE); ++i)
    {
        const auto attrib = static_cast<VertexAttrib>(i);
        auto *vab = geometry->GetVAB(attrib);
        if (!vab)
            continue;

        has_any = true;

        VAConfig vac;
        vac.format = vab->GetFormat();

        if (!vil_cfg.Add(attrib, vac))
        {
            const uint64_t n = ++g_resolve_vil_fallback_add_failed;
            if (ShouldLogPow2(n))
            {
                std::fprintf(stderr,
                    "[MaterialAssetRegistry] ResolveVIL fallback(default): VILConfig::Add failed, material='%s' attrib='%s' format='%s' total=%llu\n",
                    material->GetName().c_str(),
                    GetVertexAttribName(attrib),
                    GetVulkanFormatName(vab->GetFormat()),
                    static_cast<unsigned long long>(n));
            }
            return ResolveVILFromRecord(material, fallback_rec);
        }
    }

    if (!has_any)
    {
        const uint64_t n = ++g_resolve_vil_fallback_no_vab;
        if (ShouldLogPow2(n))
        {
            std::fprintf(stderr,
                "[MaterialAssetRegistry] ResolveVIL fallback(default): geometry has no VAB, material='%s' domain='%s' id='%s' total=%llu\n",
                material->GetName().c_str(),
                fallback_rec.domain_id.c_str(),
                fallback_rec.id.c_str(),
                static_cast<unsigned long long>(n));
        }
        return ResolveVILFromRecord(material, fallback_rec);
    }

    if (auto *vil = material->CreateVIL(&vil_cfg))
        return vil;

    const uint64_t n = ++g_resolve_vil_fallback_create_failed;
    if (ShouldLogPow2(n))
    {
        std::fprintf(stderr,
            "[MaterialAssetRegistry] ResolveVIL fallback(default): CreateVIL failed, material='%s' domain='%s' id='%s' total=%llu\n",
            material->GetName().c_str(),
            fallback_rec.domain_id.c_str(),
            fallback_rec.id.c_str(),
            static_cast<unsigned long long>(n));
    }

    return ResolveVILFromRecord(material, fallback_rec);
}

static const VIL *ResolveRuntimeVIL(MaterialTemplate *material,
                                    const mtl::MaterialAssetRecord &final_rec,
                                    const GeometrySignature &geometry)
{
    return ResolveVILFromGeometry(material,
                                  geometry.geometry_for_vil_derivation,
                                  final_rec);
}

// ── FNV-1a 64-bit texture config hash ────────────────────────────────────────

static uint64_t ComputeTextureConfigHash(
    const std::vector<mtl::MaterialAssetRecord::TextureSlotConfig> &textures)
{
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis

    for (const auto &tc : textures)
    {
        auto feed = [&](const void *p, size_t n) {
            auto *bytes = static_cast<const uint8_t*>(p);
            for (size_t i = 0; i < n; ++i)
                hash = (hash ^ bytes[i]) * 1099511628211ULL;
        };

        auto slot = static_cast<uint8_t>(tc.slot);
        auto mode = static_cast<uint8_t>(tc.source_mode);
        feed(&slot, 1);
        feed(&mode, 1);
        feed(tc.path.data(), tc.path.size());
    }
    return hash;
}

// ── SemanticMaterialId hash (runtime policy fields intentionally excluded) ──

static uint64_t ComputeSemanticMaterialHash(const mtl::MaterialAssetRecord &rec)
{
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis

    auto feed = [&](const void *p, size_t n)
    {
        auto *bytes = static_cast<const uint8_t*>(p);
        for (size_t i = 0; i < n; ++i)
            hash = (hash ^ bytes[i]) * 1099511628211ULL;
    };

    auto feed_str = [&](const std::string &s)
    {
        feed(s.data(), s.size());
        const uint8_t z = 0;
        feed(&z, 1);
    };

    // semantic core
    {
        const uint8_t preset = static_cast<uint8_t>(rec.preset);
        const uint8_t dim = static_cast<uint8_t>(rec.dim);
        const uint8_t prim = static_cast<uint8_t>(rec.prim);
        const uint8_t l2w = rec.l2w ? 1u : 0u;
        const uint8_t coord2d = static_cast<uint8_t>(rec.coord_2d);
        const uint8_t camera = rec.camera ? 1u : 0u;
        const uint8_t sky = rec.sky ? 1u : 0u;
        const uint8_t sky_ambient = static_cast<uint8_t>(rec.sky_ambient);
        const uint8_t lighting = static_cast<uint8_t>(rec.lighting);

        feed(&preset, 1);
        feed(&dim, 1);
        feed(&prim, 1);
        feed(&l2w, 1);
        feed(&rec.pos_format, sizeof(rec.pos_format));
        feed(&coord2d, 1);
        feed(&camera, 1);
        feed(&sky, 1);
        feed(&sky_ambient, 1);
        feed(&lighting, 1);
    }

    // semantic texture references
    for (const auto &tc : rec.textures)
    {
        const uint8_t slot = static_cast<uint8_t>(tc.slot);
        const uint8_t mode = static_cast<uint8_t>(tc.source_mode);
        feed(&slot, 1);
        feed(&mode, 1);
        feed_str(tc.path);
    }

    // billboard semantic fields
    {
        const uint8_t fixed_size = rec.billboard.fixed_size ? 1u : 0u;
        const uint8_t blend_mode = static_cast<uint8_t>(rec.billboard.blend_mode);
        const uint8_t base_channel = static_cast<uint8_t>(rec.billboard.base_color_channel);
        const uint8_t front_face_ccw = rec.billboard.front_face_ccw ? 1u : 0u;

        feed(&fixed_size, 1);
        feed(&rec.billboard.pixel_w, sizeof(rec.billboard.pixel_w));
        feed(&rec.billboard.pixel_h, sizeof(rec.billboard.pixel_h));
        feed(&blend_mode, 1);
        feed(&base_channel, 1);
        feed(&front_face_ccw, 1);
        feed_str(rec.billboard.texture_id);
    }

    return hash;
}

// ── DMBKeyHash ───────────────────────────────────────────────────────────────

size_t MaterialAssetRegistry::DMBKeyHash::operator()(const DMBKey &k) const
{
    // Combine the three fields with FNV-1a
    size_t h = 14695981039346656037ULL;
    for (char c : k.material_name)
        h = (h ^ static_cast<size_t>(c)) * 1099511628211ULL;
    for (char c : k.domain_id)
        h = (h ^ static_cast<size_t>(c)) * 1099511628211ULL;
    const auto *bytes = reinterpret_cast<const uint8_t*>(&k.texture_hash);
    for (size_t i = 0; i < sizeof(k.texture_hash); ++i)
        h = (h ^ bytes[i]) * 1099511628211ULL;
    return h;
}

size_t MaterialAssetRegistry::VariantKeyHash::operator()(const VariantKey &k) const
{
    size_t h = 14695981039346656037ULL;

    auto feed = [&](const void *p, size_t n)
    {
        const auto *bytes = reinterpret_cast<const uint8_t*>(p);
        for (size_t i = 0; i < n; ++i)
            h = (h ^ bytes[i]) * 1099511628211ULL;
    };

    auto feed_str = [&](const std::string &s)
    {
        feed(s.data(), s.size());
        const uint8_t z = 0;
        feed(&z, 1);
    };

    feed(&k.semantic_id, sizeof(k.semantic_id));

    {
        const uint8_t pipeline = static_cast<uint8_t>(k.request.pipeline);
        feed(&pipeline, 1);
        feed_str(k.request.domain_id);
        feed(&k.request.policy_flags, sizeof(k.request.policy_flags));
        feed(&k.request.transparency_mode, sizeof(k.request.transparency_mode));
        feed(&k.request.lod_tier, sizeof(k.request.lod_tier));
    }

    {
        const uint8_t prim = static_cast<uint8_t>(k.geometry.primitive);
        feed(&prim, 1);
        feed(&k.geometry.vil_hash, sizeof(k.geometry.vil_hash));
        feed(&k.geometry.geometry_layout_hash, sizeof(k.geometry.geometry_layout_hash));
    }

    return h;
}

size_t MaterialAssetRegistry::EntitySemanticKeyHash::operator()(const EntitySemanticKey &k) const
{
    size_t h = 14695981039346656037ULL;

    const auto *e = reinterpret_cast<const uint8_t*>(&k.entity_id);
    for (size_t i = 0; i < sizeof(k.entity_id); ++i)
        h = (h ^ e[i]) * 1099511628211ULL;

    const auto *s = reinterpret_cast<const uint8_t*>(&k.semantic_id);
    for (size_t i = 0; i < sizeof(k.semantic_id); ++i)
        h = (h ^ s[i]) * 1099511628211ULL;

    return h;
}

// ── Constructor ──────────────────────────────────────────────────────────────

MaterialAssetRegistry::MaterialAssetRegistry(
    MaterialManager *mm_,
    TextureManager  *tm_,
    SamplerManager  *sm_)
    : mm(mm_), tm(tm_), sm(sm_)
{}

const VIL *MaterialAssetRegistry::ResolveVIL(const MaterialTemplate *material,
                                             const mtl::MaterialAssetRecord &rec,
                                             const Geometry *geometry) const
{
    if (!material)
    {
        std::fprintf(stderr,
            "[MaterialAssetRegistry] ResolveVIL failed: material is null (domain='%s', id='%s')\n",
            rec.domain_id.c_str(),
            rec.id.c_str());
        return nullptr;
    }

    auto *resolved = ResolveVILFromGeometry(const_cast<MaterialTemplate *>(material), geometry, rec);
    if (!resolved)
    {
        std::fprintf(stderr,
            "[MaterialAssetRegistry] ResolveVIL failed: ResolveVILFromGeometry returned null for material='%s' domain='%s'\n",
            material->GetName().c_str(),
            rec.domain_id.c_str());
    }

    return resolved;
}

// ── Acquire ──────────────────────────────────────────────────────────────────

MaterialDomainHandle MaterialAssetRegistry::Acquire(const mtl::MaterialAssetRecord &rec)
{
    MaterialDomainHandle handle;

    // 1. MaterialTemplate (AcquireMaterial 内部已缓存)
    handle.material = CreateMaterialFromRecord(mm, rec);
    if (!handle.material)
        return {};

    const AnsiString &mat_name = handle.material->GetName();
    std::string mat_name_str(mat_name.c_str() ? mat_name.c_str() : "",
                             mat_name.c_str() ? static_cast<size_t>(mat_name.Length()) : 0);

    // 2. MaterialResourceDomain (按 material_name + domain_id 缓存)
    const std::string &did = rec.domain_id;          // 空串 → 默认域
    const std::string domain_cache_key = mat_name_str + "#" + did;

    auto it_domain = domain_cache.find(domain_cache_key);
    if (it_domain != domain_cache.end())
    {
        handle.domain = it_domain->second;
    }
    else
    {
        handle.domain = mm->CreateMaterialResourceDomain(handle.material);
        if (!handle.domain)
            return {};
        domain_cache[domain_cache_key] = handle.domain;
    }

    // 3. DomainMaterialBinding (按 material_name + domain_id + texture_hash 缓存)
    uint64_t tex_hash = ComputeTextureConfigHash(rec.textures);
    DMBKey key { std::move(mat_name_str), did, tex_hash };

    auto it_dmb = dmb_cache.find(key);
    if (it_dmb != dmb_cache.end())
    {
        handle.binding = it_dmb->second;
    }
    else
    {
        handle.binding = mm->CreateDomainMaterialBinding(handle.domain, handle.material);
        if (!handle.binding)
            return {};

        // 绑定纹理到 DMB
        if (tm && sm && !rec.textures.empty())
        {
            if (!BindDomainTexturesFromRecord(handle.binding, tm, sm, rec))
                return {};

            // Compatibility fallback for code paths that still use MaterialTemplate MP
            // instead of DomainMaterialBinding MP during draw binding.
            BindMaterialTexturesCompat(handle.material, tm, sm, rec);
        }

        dmb_cache[key] = handle.binding;
    }

    return handle;
}

SemanticMaterialId MaterialAssetRegistry::RegisterSemanticMaterial(const mtl::MaterialAssetRecord &rec)
{
    const SemanticMaterialId id = ComputeSemanticMaterialHash(rec);

    auto it = semantic_cache.find(id);
    if (it == semantic_cache.end())
    {
        SemanticMaterialEntry entry;
        entry.rec = rec;

        // Build canonical material/domain once as Phase B foundation.
        entry.canonical_material = CreateMaterialFromRecord(mm, rec);
        if (entry.canonical_material && mm)
        {
            entry.shared_domain = mm->CreateMaterialResourceDomain(entry.canonical_material);
        }

        semantic_cache.emplace(id, std::move(entry));
    }

    return id;
}

bool MaterialAssetRegistry::QuerySemanticMaterial(SemanticMaterialId id, mtl::MaterialAssetRecord &out_rec) const
{
    const auto it = semantic_cache.find(id);
    if (it == semantic_cache.end())
        return false;

    out_rec = it->second.rec;
    return true;
}

PrimitiveMaterialSlot MaterialAssetRegistry::ResolveMI(SemanticMaterialId semantic_id,
                                                   const RuntimeMaterialRequest &request,
                                                   const GeometrySignature &geometry,
                                                   const void *instance_data,
                                                   uint32_t instance_data_size,
                                                   MaterialDomainHandle *out_handle)
{
    // Legacy compatibility path: no entity id means old variant-level MI cache behavior.
    return ResolveMI(0, semantic_id, request, geometry, instance_data, instance_data_size, out_handle);
}

PrimitiveMaterialSlot MaterialAssetRegistry::ResolveMI(uint64_t entity_id,
                                                   SemanticMaterialId semantic_id,
                                                   const RuntimeMaterialRequest &request,
                                                   const GeometrySignature &geometry,
                                                   const void *instance_data,
                                                   uint32_t instance_data_size,
                                                   MaterialDomainHandle *out_handle)
{
    VariantKey key;
    key.semantic_id = semantic_id;
    key.request = request;
    key.geometry = geometry;

    // Build final record from semantic + runtime + geometry.
    mtl::MaterialAssetRecord final_rec;
    if (!QuerySemanticMaterial(semantic_id, final_rec))
        return {};

    final_rec.pipeline = request.pipeline;
    final_rec.domain_id = request.domain_id;
    final_rec.prim = geometry.primitive;

    MaterialDomainHandle handle = Acquire(final_rec);
    if (!handle.IsValid())
        return {};

    if (out_handle)
        *out_handle = handle;

    variant_cache[key] = handle.material;

    // Helper: build PrimitiveMaterialSlot from a resolved MI.
    auto make_slot = [](MaterialInstance *mi) -> PrimitiveMaterialSlot {
        return mi->ToSlot();
    };

    // Legacy path: still keep variant-level MI cache for callsites that do not provide entity id.
    if (entity_id == 0)
    {
        if (!legacy_resolve_warned)
        {
            legacy_resolve_warned = true;
            LogWarning("[MaterialAssetRegistry] ResolveMI called without entity_id (legacy compatibility path). "
                       "Migrate to ResolveMI(entity_id, semantic_id, ...) for stable per-entity MI slots.");
        }

        auto it = legacy_final_mi_cache.find(key);
        if (it != legacy_final_mi_cache.end())
        {
            ++legacy_resolve_hit_count;

            mm->RebindMaterialInstance(it->second, handle.material, ResolveRuntimeVIL(handle.material, final_rec, geometry));
            it->second->SetRenderPreset(request.pipeline);
            it->second->SetMaterialPreset(final_rec.preset);

            if (instance_data && instance_data_size > 0)
                it->second->WriteMIData(instance_data, instance_data_size);

            return make_slot(it->second);
        }

        ++legacy_resolve_miss_count;

        MaterialInstance *mi = mm->CreateMaterialInstance(handle.domain,
                                                          handle.material,
                                                          ResolveRuntimeVIL(handle.material, final_rec, geometry),
                                                          instance_data,
                                                          instance_data_size);
        if (!mi)
            return {};

        mi->SetMaterialPreset(final_rec.preset);

        legacy_final_mi_cache.emplace(std::move(key), mi);
        return make_slot(mi);
    }

    // New Phase D path: stable MI slot per (entity, semantic).
    EntitySemanticKey es_key { entity_id, semantic_id };
    auto it = entity_mi_cache.find(es_key);
    if (it != entity_mi_cache.end())
    {
        ++entity_resolve_hit_count;

        mm->RebindMaterialInstance(it->second, handle.material, ResolveRuntimeVIL(handle.material, final_rec, geometry));
        it->second->SetRenderPreset(request.pipeline);
        it->second->SetMaterialPreset(final_rec.preset);

        if (instance_data && instance_data_size > 0)
            it->second->WriteMIData(instance_data, instance_data_size);

        return make_slot(it->second);
    }

    ++entity_resolve_miss_count;

    auto sem_it = semantic_cache.find(semantic_id);
    if (sem_it == semantic_cache.end())
        return {};

    auto &entry = sem_it->second;
    if (!entry.shared_domain)
    {
        if (!entry.canonical_material)
            entry.canonical_material = CreateMaterialFromRecord(mm, entry.rec);

        if (entry.canonical_material)
        {
            entry.shared_domain = mm->CreateMaterialResourceDomain(entry.canonical_material);
        }
    }

    if (!entry.shared_domain)
        return {};

    MaterialInstance *mi = mm->CreateMaterialInstance(entry.shared_domain,
                                                      handle.material,
                                                      ResolveRuntimeVIL(handle.material, final_rec, geometry),
                                                      instance_data,
                                                      instance_data_size);
    if (!mi)
        return {};

    mi->SetRenderPreset(request.pipeline);
    mi->SetMaterialPreset(final_rec.preset);
    entity_mi_cache.emplace(es_key, mi);
    return make_slot(mi);
}

MaterialInstance *MaterialAssetRegistry::AcquireMI(const mtl::MaterialAssetRecord &rec,
                                                   const void *instance_data,
                                                   uint32_t instance_data_size,
                                                   MaterialDomainHandle *out_handle)
{
    MaterialDomainHandle handle = Acquire(rec);
    if (!handle.IsValid())
        return nullptr;

    if (out_handle)
        *out_handle = handle;

    return CreateMI(handle, rec, instance_data, instance_data_size);
}

// ── CreateMI ─────────────────────────────────────────────────────────────────

MaterialInstance *MaterialAssetRegistry::CreateMI(
    const MaterialDomainHandle &handle,
    const mtl::MaterialAssetRecord &rec,
    const void *instance_data,
    uint32_t instance_data_size)
{
    static bool s_warned_create_mi_compat = false;
    if (!s_warned_create_mi_compat)
    {
        s_warned_create_mi_compat = true;
        LogWarning("[MaterialAssetRegistry] CreateMI compatibility path still uses AcquireMaterialInstance. "
                   "Prefer slot-first resolve/binding flow for new callsites.");
    }

    if (!handle.IsValid())
        return nullptr;

    MaterialInstanceSpec spec;
    spec.material = handle.material;
    spec.domain   = handle.domain;
    spec.preset   = rec.pipeline;
    spec.instance_data      = instance_data;
    spec.instance_data_size = instance_data_size;

    MaterialInstance *mi = mm->AcquireMaterialInstance(spec);
    if (mi)
        mi->SetMaterialPreset(rec.preset);
    return mi;
}

void MaterialAssetRegistry::ReleaseEntityResolvedMI(uint64_t entity_id, SemanticMaterialId semantic_id)
{
    if (entity_id == 0)
        return;

    if (semantic_id != 0)
    {
        EntitySemanticKey key { entity_id, semantic_id };
        auto it = entity_mi_cache.find(key);
        if (it != entity_mi_cache.end())
        {
            if (mm && it->second)
                mm->Release(it->second);

            entity_mi_cache.erase(it);
        }
        return;
    }

    std::vector<EntitySemanticKey> keys;
    keys.reserve(entity_mi_cache.size());

    for (const auto &kv : entity_mi_cache)
    {
        if (kv.first.entity_id == entity_id)
            keys.push_back(kv.first);
    }

    for (const auto &k : keys)
    {
        auto it = entity_mi_cache.find(k);
        if (it == entity_mi_cache.end())
            continue;

        if (mm && it->second)
            mm->Release(it->second);

        entity_mi_cache.erase(it);
    }
}

} // namespace hgl::graph
