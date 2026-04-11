#include <hgl/graph/module/MaterialAssetRegistry.h>
#include <hgl/graph/module/MaterialAssetLoader.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/vk/VKMaterialTemplate.h>
#include <hgl/vk/VKMaterialResourceDomain.h>
#include <hgl/vk/VKDomainMaterialBinding.h>
#include <hgl/vk/VKVertexInput.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/common/VertexInputDef.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/vk/VKVertexAttribBuffer.h>
#include <hgl/mtl/VertexAttributeSpec.h>

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

static std::atomic<uint64_t> g_resolve_vil_fallback_no_geometry {0};
static std::atomic<uint64_t> g_resolve_vil_fallback_no_vab {0};
static std::atomic<uint64_t> g_resolve_vil_fallback_add_failed {0};
static std::atomic<uint64_t> g_resolve_vil_fallback_create_failed {0};
static std::atomic<uint64_t> g_resolve_vil_fallback_incompatible {0};

static const VertexInputAttribute *FindMaterialVIAByAttrib(const VertexInput *vi,const VertexAttrib attrib)
{
    if(!vi)
        return nullptr;

    const auto &via_array = vi->GetVIAArray();
    const VertexInputAttribute *via = via_array.items;

    for(uint i = 0; i < via_array.count; ++i)
    {
        if(via->attrib == attrib)
            return via;

        ++via;
    }

    return nullptr;
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

        const VertexInputAttribute *mat_via = FindMaterialVIAByAttrib(material->GetVertexInput(), attrib);
        if (mat_via)
        {
            VAType shader_type;
            shader_type.basetype = VABaseType(mat_via->basetype);
            shader_type.vec_size = mat_via->vec_size;

            const VkFormat geom_format = vab->GetFormat();
            if(!mtl::IsStorageFormatCompatibleWithShaderType(shader_type, geom_format))
            {
                const uint64_t n = ++g_resolve_vil_fallback_incompatible;
                if (ShouldLogPow2(n))
                {
                    std::fprintf(stderr,
                        "[MaterialAssetRegistry] ResolveVIL fallback(default): incompatible geometry format, material='%s' attrib='%s' shader='%s' format='%s' total=%llu\n",
                        material->GetName().c_str(),
                        GetVertexAttribName(attrib),
                        GetVertexAttribName((VABaseType)mat_via->basetype, mat_via->vec_size),
                        GetVulkanFormatName(geom_format),
                        static_cast<unsigned long long>(n));
                }

#ifdef _DEBUG
                assert(false && "MaterialAssetRegistry::ResolveVILFromGeometry incompatible geometry format");
#endif

                return ResolveVILFromRecord(material, fallback_rec);
            }
        }

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

    const std::string &mat_name = handle.material->GetName();
    std::string mat_name_str = mat_name;

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

    auto release_slot = [](PrimitiveMaterialSlot &slot) {
        if (slot.domain && slot.mi_id >= 0)
            slot.domain->FreeMISlot(slot.mi_id);
        slot = {};
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
            const VIL *resolved_vil = ResolveRuntimeVIL(handle.material, final_rec, geometry);

            // If domain changes, recycle old slot and allocate on new domain.
            if (slot.domain != handle.domain)
            {
                release_slot(slot);
                slot = mm->AllocMaterialInstanceSlot(handle.domain,
                                                     handle.material,
                                                     resolved_vil,
                                                     request.pipeline,
                                                     instance_data,
                                                     instance_data_size);
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

            return slot;
        }

        ++legacy_resolve_miss_count;

        PrimitiveMaterialSlot slot = mm->AllocMaterialInstanceSlot(handle.domain,
                                                                    handle.material,
                                                                    ResolveRuntimeVIL(handle.material, final_rec, geometry),
                                                                    request.pipeline,
                                                                    instance_data,
                                                                    instance_data_size);
        if (!slot.IsValid())
            return {};

        slot.material_preset = final_rec.preset;
        legacy_final_mi_cache.emplace(std::move(key), slot);
        return slot;
    }

    // New Phase D path: stable MI slot per (entity, semantic).
    EntitySemanticKey es_key { entity_id, semantic_id };
    auto it = entity_mi_cache.find(es_key);
    if (it != entity_mi_cache.end())
    {
        ++entity_resolve_hit_count;

        PrimitiveMaterialSlot &slot = it->second;
        slot.material_template = handle.material;
        slot.vil = ResolveRuntimeVIL(handle.material, final_rec, geometry);
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

        return slot;
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

    PrimitiveMaterialSlot slot = mm->AllocMaterialInstanceSlot(entry.shared_domain,
                                                                handle.material,
                                                                ResolveRuntimeVIL(handle.material, final_rec, geometry),
                                                                request.pipeline,
                                                                instance_data,
                                                                instance_data_size);
    if (!slot.IsValid())
        return {};

    slot.material_preset = final_rec.preset;
    entity_mi_cache.emplace(es_key, slot);
    return slot;
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
            if (it->second.domain && it->second.mi_id >= 0)
                it->second.domain->FreeMISlot(it->second.mi_id);

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

        if (it->second.domain && it->second.mi_id >= 0)
            it->second.domain->FreeMISlot(it->second.mi_id);

        entity_mi_cache.erase(it);
    }
}

MaterialInstanceHandle MaterialAssetRegistry::AllocateHandle(const MaterialBindingInit &init)
{
    if (!mm || !init.material || !init.domain)
        return InvalidMaterialInstanceHandle;

    PrimitiveMaterialSlot slot = mm->AllocMaterialInstanceSlot(
        init.domain,
        init.material,
        init.vil,
        init.preset,
        init.instance_data,
        init.instance_data_size);

    if (!slot.IsValid())
        return InvalidMaterialInstanceHandle;

    MaterialBindingRecord rec;
    rec.material_template = slot.material_template;
    rec.domain = slot.domain;
    rec.mi_id = slot.mi_id;
    rec.vil = slot.vil;
    rec.preset = slot.preset;
    rec.material_preset = init.material_preset;
    rec.texture_array_slot_flags = slot.texture_array_slot_flags;

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
    out_slot.domain = rec.domain;
    out_slot.mi_id = rec.mi_id;
    out_slot.vil = rec.vil;
    out_slot.preset = rec.preset;
    out_slot.texture_array_slot_flags = rec.texture_array_slot_flags;
    out_slot.material_preset = rec.material_preset;
    out_slot.mit_data = rec.mit_packed.empty() ? nullptr : rec.mit_packed.data();
    out_slot.mit_data_count = static_cast<uint32_t>(rec.mit_packed.size());
    return out_slot.IsValid();
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
    if (!mm || !req.new_material || !req.new_domain)
    {
        ++handle_rebind_fail_count;
        return false;
    }

    PrimitiveMaterialSlot new_slot = mm->AllocMaterialInstanceSlot(
        req.new_domain,
        req.new_material,
        req.new_vil,
        req.new_preset,
        nullptr,
        0);

    if (!new_slot.IsValid())
    {
        ++handle_rebind_fail_count;
        return false;
    }

    // Copy MI payload first; keep old binding intact if copy fails unexpectedly.
    if (req.copy_policy != MaterialRebindCopyPolicy::None
        && rec.mi_id >= 0 && rec.domain && rec.material_template
        && new_slot.mi_id >= 0 && new_slot.domain && req.new_material)
    {
        void *new_ptr = new_slot.domain->GetMIData(new_slot.mi_id);
        const void *old_ptr = rec.domain->GetMIData(rec.mi_id);

        if (new_ptr && old_ptr)
        {
            const uint32_t old_bytes = rec.material_template->GetMIDataBytes();
            const uint32_t new_bytes = req.new_material->GetMIDataBytes();
            const uint32_t copy_bytes = std::min(old_bytes, new_bytes);
            if (copy_bytes > 0)
                std::memcpy(new_ptr, old_ptr, copy_bytes);
        }
    }

    MaterialResourceDomain *old_domain = rec.domain;
    const int old_mi_id = rec.mi_id;
    const std::vector<int8_t> old_offsets = rec.mit_slot_offset;
    const std::vector<uint32_t> old_packed = rec.mit_packed;

    // Update record to the new binding.
    rec.material_template = new_slot.material_template;
    rec.domain = new_slot.domain;
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
    if (old_domain != rec.domain)
        ++cross_domain_rebind_count;

    if (old_domain && old_mi_id >= 0)
        old_domain->FreeMISlot(old_mi_id);

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
    if (!rec.material_template || !rec.domain || rec.mi_id < 0)
        return false;

    void *dst = rec.domain->GetMIData(rec.mi_id);
    if (!dst)
        return false;

    const uint32_t dst_bytes = rec.material_template->GetMIDataBytes();
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
    if (rec.domain && rec.mi_id >= 0)
        rec.domain->FreeMISlot(rec.mi_id);

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
