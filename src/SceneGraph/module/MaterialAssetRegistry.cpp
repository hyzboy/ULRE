#include <hgl/graph/module/MaterialAssetRegistry.h>
#include <hgl/graph/module/MaterialAssetLoader.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/vk/VKMaterial.h>
#include <hgl/vk/VKMaterialInstance.h>
#include <hgl/vk/VKResourceDomain.h>
#include <hgl/vk/VKDomainMaterialBinding.h>
#include <hgl/vk/VKVertexInputConfig.h>

namespace hgl::graph
{

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
// from Material directly (without domain binding registration).
// Keep this best-effort fallback to avoid unbound sampler validation errors.
static void BindMaterialTexturesCompat(
    Material *material,
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

// ── Constructor ──────────────────────────────────────────────────────────────

MaterialAssetRegistry::MaterialAssetRegistry(
    MaterialManager *mm_,
    TextureManager  *tm_,
    SamplerManager  *sm_)
    : mm(mm_), tm(tm_), sm(sm_)
{}

// ── Acquire ──────────────────────────────────────────────────────────────────

MaterialDomainHandle MaterialAssetRegistry::Acquire(const mtl::MaterialAssetRecord &rec)
{
    MaterialDomainHandle handle;

    // 1. Material (AcquireMaterial 内部已缓存)
    handle.material = CreateMaterialFromRecord(mm, rec);
    if (!handle.material)
        return {};

    const AnsiString &mat_name = handle.material->GetName();
    std::string mat_name_str(mat_name.c_str() ? mat_name.c_str() : "",
                             mat_name.c_str() ? static_cast<size_t>(mat_name.Length()) : 0);

    // 2. ResourceDomain (按 material_name + domain_id 缓存)
    const std::string &did = rec.domain_id;          // 空串 → 默认域
    const std::string domain_cache_key = mat_name_str + "#" + did;

    auto it_domain = domain_cache.find(domain_cache_key);
    if (it_domain != domain_cache.end())
    {
        handle.domain = it_domain->second;
    }
    else
    {
        handle.domain = mm->CreateResourceDomain(handle.material);
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

            // Compatibility fallback for code paths that still use Material MP
            // instead of DomainMaterialBinding MP during draw binding.
            BindMaterialTexturesCompat(handle.material, tm, sm, rec);
        }

        dmb_cache[key] = handle.binding;
    }

    return handle;
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
    if (!handle.IsValid())
        return nullptr;

    MaterialInstanceSpec spec;
    spec.material = handle.material;
    spec.domain   = handle.domain;
    spec.preset   = rec.pipeline;
    spec.instance_data      = instance_data;
    spec.instance_data_size = instance_data_size;

    VILConfig vil_cfg;
    if (!rec.mi_vil_overrides.empty())
    {
        for (const auto &ov : rec.mi_vil_overrides)
        {
            VAConfig vac;
            vac.format = ov.format;

            if (!vil_cfg.Add(ov.attrib, vac))
                return nullptr;
        }

        spec.vil_cfg = &vil_cfg;
    }

    return mm->AcquireMaterialInstance(spec);
}

} // namespace hgl::graph
