#include <hgl/graph/module/MaterialAssetRegistry.h>
#include <hgl/graph/module/MaterialAssetLoader.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/graph/module/VertexBindingCompatibility.h>
#include <hgl/graph/module/VertexBindingDiagnostics.h>
#include <hgl/vk/VKMaterialTemplate.h>
#include <hgl/vk/VKInstanceDataDomain.h>
#include <hgl/vk/VKDomainMaterialBinding.h>
#include <hgl/vk/VKVertexInput.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/common/VertexInputDef.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/vk/VKVertexAttribBuffer.h>

#include <vector>
#include <cassert>
#include <cstdio>
#include <atomic>
#include <algorithm>
#include <cstring>

namespace hgl::graph
{

namespace
{
static constexpr const char *kDefaultDomainId = "__default__";

static std::atomic<uint64_t> g_default_domain_normalize_count {0};

static const std::string &NormalizeDomainId(const std::string &domain_id, bool *used_default = nullptr)
{
    static const std::string kDefaultDomain(kDefaultDomainId);
    const bool use_default = domain_id.empty();

    if (used_default)
        *used_default = use_default;

    return use_default ? kDefaultDomain : domain_id;
}

static void BuildMITOffsets(const uint8_t slot_flags, std::vector<int8_t> &out_offsets, std::vector<uint32_t> &out_packed)
{
    out_offsets.assign(mtl::SamplerSlotCount, int8_t(-1));

    uint32_t offset = 0;
    for (size_t s = 0; s < mtl::SamplerSlotCount; ++s)
    {
        if ((slot_flags & (1u << s)) != 0)
        {
            out_offsets[s] = static_cast<int8_t>(offset);
            ++offset;
        }
    }

    out_packed.assign(offset, 0u);
}

static bool ShouldLogPow2(const uint64_t v)
{
    return v != 0 && ((v & (v - 1)) == 0);
}

static std::atomic<uint64_t> g_resolve_geometry_layout_hash_synthesized {0};

static uint32_t ComputeGeometryLayoutHash(const Geometry *geo)
{
    if (!geo)
        return 0;

    // Keep hash deterministic for deferred resolve keys when vil_hash==0.
    uint32_t h = 2166136261u;
    const int count = geo->GetVABCount();

    for (int i = 0; i < count; ++i)
    {
        const auto *vab = geo->GetVAB(i);

        const uint32_t binding = static_cast<uint32_t>(i);
        h ^= binding;
        h *= 16777619u;

        const uint32_t format = vab ? static_cast<uint32_t>(vab->GetFormat()) : 0u;
        h ^= format;
        h *= 16777619u;

        const uint32_t stride = vab ? vab->GetStride() : 0u;
        h ^= stride;
        h *= 16777619u;
    }

    return h;
}
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
        auto *smp = sm->CreateSampler().lock().get();
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

        auto smp_wp = sm->CreateSampler();
        auto *smp = smp_wp.lock().get();
        if (!smp)
            continue;

        // Non-fatal by design: may already be bound in cached material path.
        material->BindTextureSampler(tc.slot, tex, smp);
    }
}

static const VIL *ResolveVILFromGeometry(MaterialTemplate *material,
                                         const Geometry *geometry,
                                         const mtl::MaterialAssetRecord &rec)
{
    if (!material)
        return nullptr;

    if (!geometry)
    {
        std::fprintf(stderr,
            "[MaterialAssetRegistry] ResolveVIL failed: geometry is null, cannot derive VIL. "
            "material='%s' domain='%s' id='%s'\n",
            material->GetName().c_str(),
            rec.domain_id.c_str(),
            rec.id.c_str());
        return nullptr;
    }

    VILConfig vil_cfg;
    bool has_any = false;
    std::string build_reason;

    if (!BuildGeometryDrivenVILConfig(material, geometry, nullptr, vil_cfg, has_any, &build_reason, nullptr))
    {
        std::fprintf(stderr,
            "[MaterialAssetRegistry] ResolveVIL failed: geometry-material mismatch. "
            "material='%s' reason='%s'\n",
            material->GetName().c_str(),
            build_reason.c_str());

        DumpResolveVILIncompatibleDiagnostics(stderr,
                                             "[MaterialAssetRegistry]",
                                             material,
                                             geometry,
                                             rec,
                                             build_reason,
                                             vil_cfg);
        return nullptr;
    }

    if (!has_any)
    {
        std::fprintf(stderr,
            "[MaterialAssetRegistry] ResolveVIL failed: geometry has no VAB matching any material attribute. "
            "material='%s' domain='%s' id='%s'\n",
            material->GetName().c_str(),
            rec.domain_id.c_str(),
            rec.id.c_str());
        return nullptr;
    }

    if (auto *vil = material->CreateVIL(&vil_cfg))
        return vil;

    std::fprintf(stderr,
        "[MaterialAssetRegistry] ResolveVIL failed: CreateVIL returned null. "
        "material='%s' domain='%s' id='%s'\n",
        material->GetName().c_str(),
        rec.domain_id.c_str(),
        rec.id.c_str());
    return nullptr;
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
        // geometry_layout_hash participates only when vil_hash == 0 (deferred path).
        // See GeometrySignature::operator== for the matching equality rule.
        if (k.geometry.vil_hash == 0)
            feed(&k.geometry.geometry_layout_hash, sizeof(k.geometry.geometry_layout_hash));
    }

    return h;
}

size_t MaterialAssetRegistry::EntityVariantKeyHash::operator()(const EntityVariantKey &k) const
{
    size_t h = 14695981039346656037ULL;

    const auto *e = reinterpret_cast<const uint8_t*>(&k.entity_id);
    for (size_t i = 0; i < sizeof(k.entity_id); ++i)
        h = (h ^ e[i]) * 1099511628211ULL;

    const auto *s = reinterpret_cast<const uint8_t*>(&k.semantic_id);
    for (size_t i = 0; i < sizeof(k.semantic_id); ++i)
        h = (h ^ s[i]) * 1099511628211ULL;

    const auto *r = reinterpret_cast<const uint8_t*>(&k.request_hash);
    for (size_t i = 0; i < sizeof(k.request_hash); ++i)
        h = (h ^ r[i]) * 1099511628211ULL;

    const auto *g = reinterpret_cast<const uint8_t*>(&k.geometry_hash);
    for (size_t i = 0; i < sizeof(k.geometry_hash); ++i)
        h = (h ^ g[i]) * 1099511628211ULL;

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
    const MaterialManager::MaterialAccessToken access_token(0);
    handle.material = CreateMaterialFromRecord(mm, rec, access_token);
    if (!handle.material)
        return {};

    const std::string &mat_name = handle.material->GetName();
    std::string mat_name_str = mat_name;

    // 2. InstanceDataDomain (按 material_name + domain_id 缓存)
    bool used_default_domain = false;
    const std::string &did = NormalizeDomainId(rec.domain_id, &used_default_domain); // 未指定 → 显式默认域
    if (used_default_domain)
    {
        const uint64_t n = ++g_default_domain_normalize_count;
        if (ShouldLogPow2(n))
        {
            std::fprintf(stderr,
                "[MaterialAssetRegistry] domain_id is empty; normalized to '%s' (count=%llu, material='%s')\n",
                kDefaultDomainId,
                static_cast<unsigned long long>(n),
                mat_name.c_str());
        }
    }
    const std::string domain_cache_key = mat_name_str + "#" + did;

    auto it_domain = domain_cache.find(domain_cache_key);
    if (it_domain != domain_cache.end())
    {
        handle.idd_handle = it_domain->second;
    }
    else
    {
        InstanceDataDomain *new_domain = mm->CreateInstanceDataDomain(handle.material);
        if (!new_domain)
            return {};
        handle.idd_handle = mm->GetIDDManager()->GetHandle(new_domain);
        domain_cache[domain_cache_key] = handle.idd_handle;
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
        InstanceDataDomain *domain_ptr = mm->GetIDDManager()->Get(handle.idd_handle);
        handle.binding = mm->CreateDomainMaterialBinding(domain_ptr, handle.material);
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

    if (semantic_cache.find(id) == semantic_cache.end())
    {
        SemanticMaterialEntry entry;
        entry.rec = rec;
        // canonical_material / shared_domain are intentionally NOT created here.
        // MaterialTemplate = f(semantic, Geometry VAB, runtime state) — all three are
        // only known at render time. ResolveMI() calls Acquire() with the final merged
        // record then, so there is nothing to pre-build here.
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
    GeometrySignature effective_geometry = geometry;
    if (effective_geometry.vil_hash == 0
     && effective_geometry.geometry_layout_hash == 0
     && effective_geometry.geometry_for_vil_derivation)
    {
        effective_geometry.geometry_layout_hash = ComputeGeometryLayoutHash(effective_geometry.geometry_for_vil_derivation);

        const uint64_t n = ++g_resolve_geometry_layout_hash_synthesized;
        if (ShouldLogPow2(n))
        {
            std::fprintf(stderr,
                "[MaterialAssetRegistry] ResolveMI synthesized geometry_layout_hash for deferred signature (vil_hash=0), total=%llu\n",
                static_cast<unsigned long long>(n));
        }
    }

    VariantKey key;
    key.semantic_id = semantic_id;
    key.request = request;
    key.geometry = effective_geometry;

    // Check variant cache before rebuilding the full record (read-write symmetric).
    MaterialDomainHandle handle;
    {
        auto vc_it = variant_cache.find(key);
        if (vc_it != variant_cache.end())
        {
            ++variant_cache_hit_count;
            handle = vc_it->second;
        }
        else
        {
            ++variant_cache_miss_count;
            // Build final record from semantic + runtime + geometry.
            mtl::MaterialAssetRecord final_rec;
            if (!QuerySemanticMaterial(semantic_id, final_rec))
                return {};

            final_rec.pipeline = request.pipeline;
            final_rec.domain_id = request.domain_id;
            final_rec.prim = effective_geometry.primitive;

            handle = Acquire(final_rec);
            if (!handle.IsValid())
                return {};

            variant_cache[key] = handle;
        }
    }

    // Rebuild final_rec for downstream VIL resolution (needed even on cache hit).
    mtl::MaterialAssetRecord final_rec;
    if (!QuerySemanticMaterial(semantic_id, final_rec))
        return {};
    final_rec.pipeline  = request.pipeline;
    final_rec.domain_id = request.domain_id;
    final_rec.prim      = effective_geometry.primitive;

    if (out_handle)
        *out_handle = handle;

    // P5: resolve idd_handle to raw ptr for the legacy slot-level helpers below.
    InstanceDataDomain *resolved_target_domain = mm->GetIDDManager()->Get(handle.idd_handle);

    auto release_slot = [](PrimitiveMaterialSlot &slot) {
        if (slot.domain && slot.mi_id >= 0)
            slot.domain->FreeMISlot(slot.mi_id);
        slot = {};
    };

    auto apply_runtime_slot = [&](PrimitiveMaterialSlot &slot,
                                  InstanceDataDomain *target_domain,
                                  bool &ok) -> PrimitiveMaterialSlot &
    {
        ok = false;

        const VIL *resolved_vil = ResolveRuntimeVIL(handle.material, final_rec, effective_geometry);
        if (!resolved_vil)
        {
            release_slot(slot);
            return slot;
        }

        // Align both legacy/entity paths: domain change requires slot re-allocation.
        if (slot.domain != target_domain)
        {
            release_slot(slot);
            slot = mm->AllocMaterialInstanceSlot(target_domain,
                                                 instance_data,
                                                 instance_data_size);
            if (!slot.domain)
                return slot;

            slot.material_template        = handle.material;
            slot.vil                      = resolved_vil;
            slot.preset                   = request.pipeline;
            slot.texture_array_slot_flags = handle.material->GetTextureArraySlotFlags();

            if (!slot.IsValid())
                return slot;
        }
        else
        {
            slot.material_template = handle.material;
            slot.vil = resolved_vil;
            slot.preset = request.pipeline;
            slot.material_preset = final_rec.preset;
            slot.texture_array_slot_flags = handle.material ? handle.material->GetTextureArraySlotFlags() : 0;

            if (instance_data && instance_data_size > 0 && slot.mi_id >= 0 && slot.domain)
            {
                if (void *dst = slot.domain->GetMIData(slot.mi_id))
                {
                    const uint32_t dst_bytes = handle.material ? handle.material->GetMIDataBytes() : 0;
                    const uint32_t copy_bytes = std::min(instance_data_size, dst_bytes);
                    if (copy_bytes > 0)
                        std::memcpy(dst, instance_data, copy_bytes);
                }
            }
        }

        slot.material_preset = final_rec.preset;
        ok = slot.IsValid();
        return slot;
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

            PrimitiveMaterialSlot &slot = it->second;
            bool ok = false;
        apply_runtime_slot(slot, resolved_target_domain, ok);
        if (!ok)
            return {};

        return slot;
    }

    ++legacy_resolve_miss_count;

    PrimitiveMaterialSlot slot = {};
    bool ok = false;
    apply_runtime_slot(slot, resolved_target_domain, ok);

        slot.material_preset = final_rec.preset;
        legacy_final_mi_cache.emplace(std::move(key), slot);
        return slot;
    }

    // New Phase D path: stable MI slot per (entity, semantic, request variant, geometry variant).
    // Compute discriminator hashes so one entity can hold multiple concurrent variant slots.
    const uint64_t req_hash = [&]() -> uint64_t {
        uint64_t h = 14695981039346656037ULL;
        const auto *p = reinterpret_cast<const uint8_t*>(&request.pipeline);
        for (size_t i = 0; i < sizeof(request.pipeline); ++i)
            h = (h ^ p[i]) * 1099511628211ULL;
        for (unsigned char c : request.domain_id)
            h = (h ^ c) * 1099511628211ULL;
        h = (h ^ static_cast<uint64_t>(request.policy_flags))       * 1099511628211ULL;
        h = (h ^ static_cast<uint64_t>(request.transparency_mode))  * 1099511628211ULL;
        h = (h ^ static_cast<uint64_t>(request.lod_tier))           * 1099511628211ULL;
        return h;
    }();
    const uint64_t geo_hash =
        static_cast<uint64_t>(effective_geometry.geometry_layout_hash)
        | (static_cast<uint64_t>(effective_geometry.vil_hash) << 32);

    EntityVariantKey ev_key { entity_id, semantic_id, req_hash, geo_hash };
    auto it = entity_mi_cache.find(ev_key);
    if (it != entity_mi_cache.end())
    {
        ++entity_resolve_hit_count;

        PrimitiveMaterialSlot &slot = it->second;
        bool ok = false;
        apply_runtime_slot(slot, resolved_target_domain, ok);
        if (!ok)
            return {};

        return slot;
    }

    ++entity_resolve_miss_count;

    PrimitiveMaterialSlot slot = {};
    bool ok = false;
    apply_runtime_slot(slot, resolved_target_domain, ok);
    if (!ok)
        return {};

    slot.material_preset = final_rec.preset;
    entity_mi_cache.emplace(ev_key, slot);
    return slot;
}

void MaterialAssetRegistry::ReleaseEntityResolvedMI(uint64_t entity_id, SemanticMaterialId semantic_id)
{
    if (entity_id == 0)
        return;

    // With EntityVariantKey containing request/geometry discriminators, one
    // (entity_id, semantic_id) pair may have multiple variant entries.
    // Always scan to collect all matching keys.
    std::vector<EntityVariantKey> keys;
    keys.reserve(8);

    for (const auto &kv : entity_mi_cache)
    {
        if (kv.first.entity_id == entity_id
         && (semantic_id == 0 || kv.first.semantic_id == semantic_id))
            keys.push_back(kv.first);
    }

    for (const auto &k : keys)
    {
        auto it = entity_mi_cache.find(k);
        if (it == entity_mi_cache.end())
            continue;

        if (it->second.domain && it->second.mi_id >= 0)
            it->second.domain->FreeMISlot(it->second.mi_id);

        entity_mi_cache.erase(it);
    }
}

MaterialInstanceHandle MaterialAssetRegistry::AllocateHandle(const MaterialBindingInit &init)
{
    if (!mm || !init.material || !init.idd_handle.IsValid())
        return InvalidMaterialInstanceHandle;

    PrimitiveMaterialSlot slot = mm->AllocMaterialInstanceSlot(
        init.idd_handle,
        init.instance_data,
        init.instance_data_size);

    if (!slot.domain)
        return InvalidMaterialInstanceHandle;

    slot.material_template        = init.material;
    slot.vil                      = init.vil;
    slot.preset                   = init.preset;
    slot.texture_array_slot_flags = init.material->GetTextureArraySlotFlags();

    // Init-time: only need domain slot allocated; VIL may be deferred until render time.
    if (!slot.HasData())
        return InvalidMaterialInstanceHandle;

    MaterialBindingRecord rec;
    rec.material_template = slot.material_template;
    rec.idd_handle = init.idd_handle;
    rec.mi_id = slot.mi_id;
    rec.vil = slot.vil;
    rec.preset = slot.preset;
    rec.material_preset = init.material_preset;
    rec.texture_array_slot_flags = slot.texture_array_slot_flags;
    rec.binding_complete = (rec.material_template != nullptr && rec.vil != nullptr);

    const uint32_t payload_bytes = rec.material_template ? rec.material_template->GetMIDataBytes() : 0;
    if (payload_bytes > 0)
    {
        rec.instance_payload.assign((payload_bytes + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0u);
        if (init.instance_data && init.instance_data_size > 0)
        {
            const uint32_t copy_bytes = std::min(payload_bytes, init.instance_data_size);
            std::memcpy(rec.instance_payload.data(), init.instance_data, copy_bytes);
        }
    }

    BuildMITOffsets(rec.texture_array_slot_flags, rec.mit_slot_offset, rec.mit_packed);
    if (init.mit_data && init.mit_data_count > 0 && !rec.mit_packed.empty())
    {
        const uint32_t copy_count = std::min<uint32_t>(init.mit_data_count, static_cast<uint32_t>(rec.mit_packed.size()));
        std::memcpy(rec.mit_packed.data(), init.mit_data, copy_count * sizeof(uint32_t));
    }

    if (next_handle == InvalidMaterialInstanceHandle)
        ++next_handle;

    const MaterialInstanceHandle handle = next_handle++;
    rec.handle = handle;

    handle_table.emplace(handle, std::move(rec));
    return handle;
}

bool MaterialAssetRegistry::BuildSlot(MaterialInstanceHandle handle, PrimitiveMaterialSlot &out_slot) const
{
    out_slot = {};

    const auto it = handle_table.find(handle);
    if (it == handle_table.end() || !it->second.alive)
        return false;

    const MaterialBindingRecord &rec = it->second;
    out_slot.material_template = rec.material_template;
    out_slot.domain = mm->GetIDDManager()->Get(rec.idd_handle);
    out_slot.idd_handle = rec.idd_handle;   // P9: propagate handle alongside raw ptr
    out_slot.idd_manager   = mm->GetIDDManager(); // P12: allow Primitive to delegate data access
    out_slot.mi_id = rec.mi_id;
    out_slot.vil = rec.vil;
    out_slot.preset = rec.preset;
    out_slot.texture_array_slot_flags = rec.texture_array_slot_flags;
    out_slot.material_preset = rec.material_preset;
    out_slot.mit_data = rec.mit_packed.empty() ? nullptr : rec.mit_packed.data();
    out_slot.mit_data_count = static_cast<uint32_t>(rec.mit_packed.size());
    // Return true if domain slot is allocated; caller checks IsRenderable() when needed for rendering.
    return out_slot.HasData();
}

bool MaterialAssetRegistry::CompleteBinding(MaterialInstanceHandle handle,
                                            MaterialTemplate *material,
                                            const VIL *vil,
                                            GraphicsPipelinePreset preset)
{
    auto it = handle_table.find(handle);
    if (it == handle_table.end() || !it->second.alive)
        return false;

    MaterialBindingRecord &rec = it->second;

    if (!material || !vil)
        return false;

    // Already complete — skip redundant update unless material changed.
    if (rec.binding_complete
        && rec.material_template == material
        && rec.vil == vil
        && rec.preset == preset)
        return true;

    rec.material_template = material;
    rec.vil = vil;
    rec.preset = preset;
    rec.texture_array_slot_flags = material->GetTextureArraySlotFlags();
    rec.binding_complete = true;
    ++rec.binding_version;

    // Rebuild MIT offsets with updated texture array flags.
    BuildMITOffsets(rec.texture_array_slot_flags, rec.mit_slot_offset, rec.mit_packed);

    return true;
}

bool MaterialAssetRegistry::RebindHandle(MaterialInstanceHandle handle, const MaterialBindingRebind &req)
{
    auto it = handle_table.find(handle);
    if (it == handle_table.end() || !it->second.alive)
    {
        ++handle_rebind_fail_count;
        return false;
    }

    MaterialBindingRecord &rec = it->second;
    if (!mm || !req.new_material || !req.new_idd_handle.IsValid())
    {
        ++handle_rebind_fail_count;
        return false;
    }

    PrimitiveMaterialSlot new_slot = mm->AllocMaterialInstanceSlot(
        req.new_idd_handle,
        nullptr,
        0);

    if (!new_slot.domain)
    {
        ++handle_rebind_fail_count;
        return false;
    }

    new_slot.material_template        = req.new_material;
    new_slot.vil                      = req.new_vil;
    new_slot.preset                   = req.new_preset;
    new_slot.texture_array_slot_flags = req.new_material->GetTextureArraySlotFlags();

    if (!new_slot.IsValid())
    {
        ++handle_rebind_fail_count;
        return false;
    }

    // Copy MI payload first; keep old binding intact if copy fails unexpectedly.
    if (req.copy_policy != MaterialRebindCopyPolicy::None
        && rec.mi_id >= 0 && rec.idd_handle.IsValid() && rec.material_template
        && new_slot.mi_id >= 0 && new_slot.domain && req.new_material)
    {
        void *new_ptr = new_slot.domain->GetMIData(new_slot.mi_id);
        auto *old_domain_ptr = mm->GetIDDManager()->Get(rec.idd_handle);
        const void *old_ptr = old_domain_ptr ? old_domain_ptr->GetMIData(rec.mi_id) : nullptr;

        if (new_ptr && old_ptr)
        {
            const uint32_t old_bytes = rec.material_template->GetMIDataBytes();
            const uint32_t new_bytes = req.new_material->GetMIDataBytes();
            const uint32_t copy_bytes = std::min(old_bytes, new_bytes);
            if (copy_bytes > 0)
                std::memcpy(new_ptr, old_ptr, copy_bytes);
        }
    }

    IDDHandle old_idd_handle = rec.idd_handle;
    const int old_mi_id = rec.mi_id;
    const std::vector<int8_t> old_offsets = rec.mit_slot_offset;
    const std::vector<uint32_t> old_packed = rec.mit_packed;

    // Update record to the new binding.
    rec.material_template = new_slot.material_template;
    rec.idd_handle = req.new_idd_handle;
    rec.mi_id = new_slot.mi_id;
    rec.vil = new_slot.vil;
    rec.preset = new_slot.preset;
    rec.material_preset = req.new_material_preset;
    rec.texture_array_slot_flags = new_slot.texture_array_slot_flags;
    BuildMITOffsets(rec.texture_array_slot_flags, rec.mit_slot_offset, rec.mit_packed);

    if (req.copy_policy != MaterialRebindCopyPolicy::None
        && !old_offsets.empty() && !old_packed.empty()
        && !rec.mit_slot_offset.empty() && !rec.mit_packed.empty())
    {
        for (size_t s = 0; s < mtl::SamplerSlotCount; ++s)
        {
            const int8_t old_off = old_offsets[s];
            const int8_t new_off = rec.mit_slot_offset[s];
            if (old_off < 0 || new_off < 0)
                continue;

            if (static_cast<size_t>(old_off) >= old_packed.size()
                || static_cast<size_t>(new_off) >= rec.mit_packed.size())
                continue;

            rec.mit_packed[new_off] = old_packed[old_off];
        }
    }

    ++rec.binding_version;
    ++handle_rebind_count;
    if (old_idd_handle != rec.idd_handle)
        ++cross_domain_rebind_count;

    if (old_idd_handle.IsValid() && old_mi_id >= 0)
    {
        auto *old_domain_ptr = mm->GetIDDManager()->Get(old_idd_handle);
        if (old_domain_ptr) old_domain_ptr->FreeMISlot(old_mi_id);
    }

    return true;
}

bool MaterialAssetRegistry::WriteMIData(MaterialInstanceHandle handle, const void *data, uint32_t size)
{
    if (!data || size == 0)
        return false;

    auto it = handle_table.find(handle);
    if (it == handle_table.end() || !it->second.alive)
        return false;

    MaterialBindingRecord &rec = it->second;
    // Relaxed guard: only need domain slot — material_template may be deferred.
    if (!rec.idd_handle.IsValid() || rec.mi_id < 0)
        return false;

    auto *domain_ptr = mm->GetIDDManager()->Get(rec.idd_handle);
    void *dst = domain_ptr ? domain_ptr->GetMIData(rec.mi_id) : nullptr;
    if (!dst)
        return false;

    // Prefer material_template stride; fall back to domain stride for partial handles.
    const uint32_t dst_bytes = rec.material_template
        ? rec.material_template->GetMIDataBytes()
        : domain_ptr->GetMIDataBytes();
    const uint32_t copy_bytes = std::min(dst_bytes, size);
    if (copy_bytes == 0)
        return false;

    std::memcpy(dst, data, copy_bytes);

    rec.instance_payload.assign((dst_bytes + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0u);
    std::memcpy(rec.instance_payload.data(), dst, copy_bytes);
    return true;
}

bool MaterialAssetRegistry::SetTextureArrayLayer(MaterialInstanceHandle handle, mtl::SamplerSlot slot, uint32_t layer)
{
    auto it = handle_table.find(handle);
    if (it == handle_table.end() || !it->second.alive)
        return false;

    MaterialBindingRecord &rec = it->second;
    if (rec.mit_slot_offset.empty())
        return false;

    const size_t slot_index = static_cast<size_t>(slot);
    if (slot_index >= rec.mit_slot_offset.size())
        return false;

    const int8_t off = rec.mit_slot_offset[slot_index];
    if (off < 0 || static_cast<size_t>(off) >= rec.mit_packed.size())
        return false;

    rec.mit_packed[off] = layer;
    return true;
}

bool MaterialAssetRegistry::ReleaseHandle(MaterialInstanceHandle handle)
{
    auto it = handle_table.find(handle);
    if (it == handle_table.end())
        return false;

    MaterialBindingRecord &rec = it->second;
    if (rec.idd_handle.IsValid() && rec.mi_id >= 0)
    {
        auto *domain_ptr = mm->GetIDDManager()->Get(rec.idd_handle);
        if (domain_ptr) domain_ptr->FreeMISlot(rec.mi_id);
    }

    rec.alive = false;
    handle_table.erase(it);
    return true;
}

bool MaterialAssetRegistry::QueryBindingVersion(MaterialInstanceHandle handle, uint32_t &out_version) const
{
    out_version = 0;

    const auto it = handle_table.find(handle);
    if (it == handle_table.end() || !it->second.alive)
        return false;

    out_version = it->second.binding_version;
    return true;
}

bool MaterialAssetRegistry::QueryHandleAlive(MaterialInstanceHandle handle) const
{
    const auto it = handle_table.find(handle);
    return it != handle_table.end() && it->second.alive;
}

} // namespace hgl::graph
