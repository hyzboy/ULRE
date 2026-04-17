#include <hgl/graph/module/MaterialAssetRegistry.h>
#include <hgl/graph/module/MaterialAssetLoader.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/module/ResourceDomainManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/vk/VKMaterial.h>
#include <hgl/vk/VKMaterialInstance.h>
#include <hgl/vk/VKResourceDomain.h>
#include <hgl/vk/VKDomainMaterialBinding.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/vk/VKVertexInputLayout.h>

#include <cctype>
#include <cstdio>

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

static uint32_t HashDomainIDString(const std::string &did)
{
    uint32_t h = 2166136261u; // FNV-1a 32-bit offset
    for (unsigned char c : did)
        h = (h ^ c) * 16777619u;

    // 0 作为默认域，避免哈希碰到 0。
    return h == 0 ? 1u : h;
}

static bool TryParseDomainID(const std::string &did, uint32_t &out_id)
{
    if (did.empty())
    {
        out_id = 0;
        return true;
    }

    uint64_t value = 0;
    for (char ch : did)
    {
        if (!std::isdigit(static_cast<unsigned char>(ch)))
            return false;

        value = value * 10 + uint64_t(ch - '0');
        if (value > 0xFFFFFFFFull)
            return false;
    }

    out_id = static_cast<uint32_t>(value);
    return true;
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
    const bool tracked_wire = (!rec.id.empty() && rec.id == "bounds_wire");
    MaterialDomainHandle handle;

    // 1. Material (AcquireMaterial 内部已缓存)
    handle.material = CreateMaterialFromRecord(mm, rec);
    if (!handle.material)
    {
        if (tracked_wire)
        {
            std::fprintf(stderr,
                "[WireTrace] MaterialAssetRegistry::Acquire failed: CreateMaterialFromRecord rec.id=%s\n",
                rec.id.c_str());
        }
        return {};
    }

    if (tracked_wire)
    {
        std::fprintf(stderr,
            "[WireTrace] MaterialAssetRegistry::Acquire material ok rec.id=%s material='%s' prim=%u hasMI=%d\n",
            rec.id.c_str(),
            handle.material->GetName().c_str(),
            static_cast<unsigned>(handle.material->GetPrimitiveType()),
            handle.material->hasMI() ? 1 : 0);
    }

    const AnsiString &mat_name = handle.material->GetName();
    std::string mat_name_str(mat_name.c_str() ? mat_name.c_str() : "",
                             mat_name.c_str() ? static_cast<size_t>(mat_name.Length()) : 0);

    // 2. ResourceDomain (按 schema + domain_id 缓存)
    const std::string &did = rec.domain_id;          // 空串 → 默认域
    const auto schema = handle.material->GetShaderDataSchema();

    uint32_t numeric_domain_id = 0;
    if (!TryParseDomainID(did, numeric_domain_id))
    {
        numeric_domain_id = HashDomainIDString(did);
    }

    const std::string normalized_domain_id = std::to_string(numeric_domain_id);
    const std::string domain_cache_key = std::to_string(static_cast<uint32_t>(schema)) + "#" + normalized_domain_id;

    auto it_domain = domain_cache.find(domain_cache_key);
    if (it_domain != domain_cache.end())
    {
        handle.domain = it_domain->second;
    }
    else
    {
        auto *gc = mm->GetGraphicsContext();
        auto *rdm = gc ? gc->GetResourceDomainManager() : nullptr;

        if (!rdm)
        {
            std::fprintf(stderr,
                "[MaterialAssetRegistry] Acquire failed: ResourceDomainManager unavailable for material='%s' schema=%u domain_id=%u\n",
                mat_name.c_str(),
                static_cast<unsigned>(schema),
                static_cast<unsigned>(numeric_domain_id));
            return {};
        }

        handle.domain = rdm->Get(schema, numeric_domain_id);

        if (!handle.domain)
        {
            ResourceDomainCreateInfo ci;
            ci.schema = schema;
            ci.domain_id = numeric_domain_id;
            ci.initial_capacity = 256;
            handle.domain = rdm->Create(ci);
        }

        if (!handle.domain)
            return {};

        if (tracked_wire)
        {
            std::fprintf(stderr,
                "[WireTrace] MaterialAssetRegistry::Acquire domain ready rec.id=%s schema=%u domain_id=%u\n",
                rec.id.c_str(),
                static_cast<unsigned>(schema),
                static_cast<unsigned>(numeric_domain_id));
        }

        domain_cache[domain_cache_key] = handle.domain;
    }

    // 无 MI 数据的材质不需要 DomainMaterialBinding。
    // 但仍显式携带 domain，以统一 MI 创建入口的约束。
    if (!handle.material->hasMI())
    {
        if (tm && sm && !rec.textures.empty())
            BindMaterialTexturesCompat(handle.material, tm, sm, rec);

        if (tracked_wire)
        {
            std::fprintf(stderr,
                "[WireTrace] MaterialAssetRegistry::Acquire return (no MI needed) rec.id=%s\n",
                rec.id.c_str());
        }

        return handle;
    }

    // 3. DomainMaterialBinding (按 material_name + domain_id + texture_hash 缓存)
    uint64_t tex_hash = ComputeTextureConfigHash(rec.textures);
    DMBKey key { std::move(mat_name_str), normalized_domain_id, tex_hash };

    auto it_dmb = dmb_cache.find(key);
    if (it_dmb != dmb_cache.end())
    {
        handle.binding = it_dmb->second;
    }
    else
    {
        handle.binding = mm->CreateDomainMaterialBinding(handle.domain, handle.material);
        if (!handle.binding)
        {
            if (tracked_wire)
            {
                std::fprintf(stderr,
                    "[WireTrace] MaterialAssetRegistry::Acquire failed: CreateDomainMaterialBinding rec.id=%s\n",
                    rec.id.c_str());
            }
            return {};
        }

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

    if (tracked_wire)
    {
        std::fprintf(stderr,
            "[WireTrace] MaterialAssetRegistry::Acquire done rec.id=%s binding=%p\n",
            rec.id.c_str(),
            static_cast<void *>(handle.binding));
    }

    return handle;
}

MaterialInstance *MaterialAssetRegistry::AcquireMI(const mtl::MaterialAssetRecord &rec,
                                                   const void *instance_data,
                                                   uint32_t instance_data_size,
                                                   MaterialDomainHandle *out_handle)
{
    MaterialDomainHandle handle = Acquire(rec);
    if (!handle.material)
        return nullptr;

    if (!handle.domain)
        return nullptr;

    // 有 MI 数据的材质必须具备 domain + binding。
    if (handle.material->hasMI() && !handle.binding)
        return nullptr;

    if (out_handle)
        *out_handle = handle;

    return CreateMI(handle, rec, instance_data, instance_data_size);
}

// ── AcquireMI (GVF auto-derive) ─────────────────────────────────────────────

MaterialInstance *MaterialAssetRegistry::AcquireMI(
    const mtl::MaterialAssetRecord &rec,
    const GeometryVertexFormat &gvf,
    const void *instance_data,
    uint32_t instance_data_size,
    MaterialDomainHandle *out_handle)
{
    const bool tracked_wire = (!rec.id.empty() && rec.id == "bounds_wire");
    MaterialDomainHandle handle = Acquire(rec);
    if (!handle.material)
    {
        if (tracked_wire)
            std::fprintf(stderr, "[WireTrace] MaterialAssetRegistry::AcquireMI failed: no material rec.id=%s\n", rec.id.c_str());
        return nullptr;
    }

    if (!handle.domain)
    {
        if (tracked_wire)
            std::fprintf(stderr, "[WireTrace] MaterialAssetRegistry::AcquireMI failed: no domain rec.id=%s\n", rec.id.c_str());
        return nullptr;
    }

    if (handle.material->hasMI() && !handle.binding)
    {
        if (tracked_wire)
            std::fprintf(stderr, "[WireTrace] MaterialAssetRegistry::AcquireMI failed: hasMI but no binding rec.id=%s\n", rec.id.c_str());
        return nullptr;
    }

    if (out_handle)
        *out_handle = handle;

    // 从 Material DefaultVIL 与 GVF 的差异自动推算 VILConfig
    const VIL *default_vil = handle.material->GetDefaultVIL();
    if (!default_vil)
    {
        if (tracked_wire)
            std::fprintf(stderr, "[WireTrace] MaterialAssetRegistry::AcquireMI failed: default VIL null rec.id=%s\n", rec.id.c_str());
        return nullptr;
    }

    MaterialInstanceSpec spec;
    spec.material = handle.material;
    spec.domain   = handle.domain;
    spec.preset   = rec.pipeline;
    spec.instance_data      = instance_data;
    spec.instance_data_size = instance_data_size;

    VILConfig vil_cfg;
    const uint32_t attrib_count = default_vil->GetVertexAttribCount();

    for (uint32_t i = 0; i < attrib_count; ++i)
    {
        const auto *vif = default_vil->GetConfig(i);
        if (!vif)
            continue;

        const VkFormat gvf_format = gvf.GetFormat(vif->attrib);

        if (gvf_format == VK_FORMAT_UNDEFINED)
            continue;   // Geometry 没有此 attrib，跳过（使用 DefaultVIL 默认值）

        if (gvf_format != vif->format)
        {
            VAConfig vac;
            vac.format = gvf_format;

            if (!vil_cfg.Add(vif->attrib, vac))
                return nullptr;
        }
    }

    if (vil_cfg.GetCount() > 0)
        spec.vil_cfg = &vil_cfg;

    if (tracked_wire)
    {
        std::fprintf(stderr,
            "[WireTrace] MaterialAssetRegistry::AcquireMI create rec.id=%s gvf_active=%u default_vil_attribs=%u overrides=%u pipeline=%u\n",
            rec.id.c_str(),
            gvf.GetActiveCount(),
            default_vil->GetVertexAttribCount(),
            vil_cfg.GetCount(),
            static_cast<unsigned>(rec.pipeline));
    }

    return mm->AcquireMaterialInstance(spec);
}

// ── CreateMI ─────────────────────────────────────────────────────────────────

MaterialInstance *MaterialAssetRegistry::CreateMI(
    const MaterialDomainHandle &handle,
    const mtl::MaterialAssetRecord &rec,
    const void *instance_data,
    uint32_t instance_data_size)
{
    if (!handle.material || !handle.domain)
        return nullptr;

    MaterialInstanceSpec spec;
    spec.material = handle.material;
    spec.domain   = handle.domain;
    spec.preset   = rec.pipeline;
    spec.instance_data      = instance_data;
    spec.instance_data_size = instance_data_size;

    VILConfig vil_cfg;

#pragma warning(push)
#pragma warning(disable : 4996)  // suppress deprecated mi_vil_overrides — backward compat
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
#pragma warning(pop)

    return mm->AcquireMaterialInstance(spec);
}

} // namespace hgl::graph
