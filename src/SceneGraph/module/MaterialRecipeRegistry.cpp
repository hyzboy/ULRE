#include <hgl/graph/module/MaterialRecipeRegistry.h>
#include <hgl/graph/module/MaterialAssetLoader.h>
#include <hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
#include <hgl/graph/module/ShaderMaterialProgramManager.h>
#include <hgl/graph/module/ResourceDomainManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/mtl/RecipeToKey.h>
#include <hgl/vk/VKShaderMaterialProgram.h>
#include <hgl/vk/VKMaterialBindingInstance.h>
#include <hgl/vk/VKResourceDomain.h>
#include <hgl/vk/VKDomainResourceBinding.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/vk/VKVertexInputLayout.h>
#include <hgl/log/Log.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>

#include <cctype>
#include <cstdio>

namespace hgl::graph
{

// ---------------------------------------------------------------------------
// Map RenderAlphaMode + MaterialPreset to GraphicsPipelinePreset.
// render_phase from MatchedShaderSet will eventually drive this selection;
// for now, blend mode + material preset category are the primary axes.
// ---------------------------------------------------------------------------
static GraphicsPipelinePreset BlendToPreset(graph::RenderAlphaMode blend, bool is2D,
                                             mtl::MaterialPreset mat_preset = mtl::MaterialPreset::Standard) noexcept
{
    // Preset-category overrides (take priority over blend mode).
    if (mat_preset == mtl::MaterialPreset::SkyMinimal)
        return GraphicsPipelinePreset::Sky;

    using R = graph::RenderAlphaMode;
    switch (blend)
    {
    case R::Transparent:      return is2D ? GraphicsPipelinePreset::Alpha2D   : GraphicsPipelinePreset::Alpha3D;
    case R::Masked:           return GraphicsPipelinePreset::Masked3D;
    case R::Dither:           return GraphicsPipelinePreset::Dither3D;
    case R::AlphaToCoverage:  return GraphicsPipelinePreset::AlphaToCoverage3D;
    default:                  return is2D ? GraphicsPipelinePreset::Solid2D   : GraphicsPipelinePreset::Solid3D;
    }
}


// ── 将 record 中的纹理加载并绑定到 DomainResourceBinding ─────────────────────

static bool BindDomainTexturesFromRecord(
    DomainResourceBinding *dmb,
    TextureManager *tm,
    SamplerManager *sm,
    const mtl::MaterialRecipe &rec)
{
    if (!dmb || !tm || !sm) return false;

    for (const auto &tc : rec.textures)
    {
        if (tc.path.empty()) continue;
        auto *tex = tm->LoadTexture2D(hgl::ToOSString(tc.path), true);
        if (!tex) return false;
        auto *smp = sm->CreateSampler();
        if (!smp) return false;
        if (!dmb->BindResourceSampler(tc.slot, tex, smp))
            return false;
    }
    return true;
}

// ── FNV-1a 64-bit texture config hash ────────────────────────────────────────

static uint64_t ComputeTextureConfigHash(
    const std::vector<mtl::MaterialRecipe::TextureAssetRef> &textures)
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
        feed(&slot, 1);
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

size_t MaterialRecipeRegistry::DMBKeyHash::operator()(const DMBKey &k) const
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

MaterialRecipeRegistry::MaterialRecipeRegistry(
    ShaderMaterialProgramManager *mm_,
    TextureManager  *tm_,
    SamplerManager  *sm_)
    : mm(mm_), tm(tm_), sm(sm_)
{}

// ── Acquire ──────────────────────────────────────────────────────────────────

MaterialDomainHandle MaterialRecipeRegistry::Acquire(const mtl::MaterialRecipe &rec)
{
    std::fprintf(stderr,
        "[MaterialRecipeRegistry] Acquire(recipe): preset=%u dim=%u prim=%u pipeline=%u domain='%s' textures=%zu key_hash=0x%llx\n",
        static_cast<unsigned>(rec.preset),
        static_cast<unsigned>(rec.dim),
        static_cast<unsigned>(rec.prim),
        static_cast<unsigned>(BlendToPreset(rec.default_render_state.blend, rec.dim == mtl::MaterialRecipe::Dim::D2)),
        rec.domain_id.c_str(),
        rec.textures.size(),
        static_cast<unsigned long long>(mtl::ResolveRecipePrimaryKey(rec).Hash()));
    return Acquire(mtl::ResolveRecipePrimaryKey(rec), rec);
}

MaterialDomainHandle MaterialRecipeRegistry::Acquire(const mtl::MaterialKey &key, const mtl::MaterialRecipe &rec)
{
    MaterialDomainHandle handle;

    std::fprintf(stderr,
        "[MaterialRecipeRegistry] Acquire(key): key_hash=0x%llx preset=%u prim=%u pipeline=%u\n",
        static_cast<unsigned long long>(key.Hash()),
        static_cast<unsigned>(rec.preset),
        static_cast<unsigned>(rec.prim),
        static_cast<unsigned>(BlendToPreset(rec.default_render_state.blend, rec.dim == mtl::MaterialRecipe::Dim::D2)));

    GLogInfo("[MaterialRecipeRegistry] Acquire(key) request key_hash=0x%llx preset=%u prim=%u pipeline=%u",
             static_cast<unsigned long long>(key.Hash()),
             static_cast<unsigned>(rec.preset),
             static_cast<unsigned>(rec.prim),
             static_cast<unsigned>(BlendToPreset(rec.default_render_state.blend, rec.dim == mtl::MaterialRecipe::Dim::D2)));

    // 1. ShaderMaterialProgram — key-transparent fast path (checks material_by_key first)
    handle.material = mm->GetOrCreateProgramByKey(key, rec);
    if (!handle.material)
    {
        GLogError("[MaterialRecipeRegistry] Acquire(key) fail: material=null key_hash=0x%llx preset=%u prim=%u pipeline=%u",
                  static_cast<unsigned long long>(key.Hash()),
                  static_cast<unsigned>(rec.preset),
                  static_cast<unsigned>(rec.prim),
                  static_cast<unsigned>(BlendToPreset(rec.default_render_state.blend, rec.dim == mtl::MaterialRecipe::Dim::D2)));
        return {};
    }

    std::fprintf(stderr,
        "[MaterialRecipeRegistry] Acquire(key): material=%p material_name='%s' material_prim=%u shader_schema=%u has_mi=%u req_prim=%u\n",
        handle.material,
        handle.material->GetName().c_str(),
        static_cast<unsigned>(handle.material->GetPrimitiveType()),
        static_cast<unsigned>(handle.material->GetShaderDataSchema()),
        handle.material->hasMI() ? 1u : 0u,
        static_cast<unsigned>(rec.prim));

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
                "[MaterialRecipeRegistry] Acquire failed: ResourceDomainManager unavailable for material='%s' schema=%u domain_id=%u\n",
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

        domain_cache[domain_cache_key] = handle.domain;
    }

    // 3. DomainResourceBinding
    // tex_hash is needed for both hasMI and !hasMI paths, compute once here.
    const uint64_t tex_hash = ComputeTextureConfigHash(rec.textures);
    DMBKey dmb_key { std::move(mat_name_str), normalized_domain_id, tex_hash };

    // Materials without per-instance data still need DMB for texture bindings.
    if (!handle.material->hasMI())
    {
        std::fprintf(stderr,
            "[MaterialRecipeRegistry] Acquire(key): material hasMI=0, skip MI binding path material='%s' req_prim=%u pipeline=%u\n",
            handle.material->GetName().c_str(),
            static_cast<unsigned>(rec.prim),
            static_cast<unsigned>(BlendToPreset(rec.default_render_state.blend, rec.dim == mtl::MaterialRecipe::Dim::D2)));

        if (tm && sm && !rec.textures.empty())
        {
            auto it_dmb_nomi = dmb_cache.find(dmb_key);
            if (it_dmb_nomi != dmb_cache.end())
            {
                handle.binding = it_dmb_nomi->second;
            }
            else
            {
                handle.binding = mm->CreateDomainMaterialBinding(handle.domain, handle.material);
                if (!handle.binding)
                    return {};
                if (!BindDomainTexturesFromRecord(handle.binding, tm, sm, rec))
                    return {};
                dmb_cache[dmb_key] = handle.binding;
            }
        }
        return handle;
    }

    auto it_dmb = dmb_cache.find(dmb_key);
    if (it_dmb != dmb_cache.end())
    {
        handle.binding = it_dmb->second;
        std::fprintf(stderr,
            "[MaterialRecipeRegistry] Acquire(key): reuse DMB material='%s' domain=%p binding=%p\n",
            handle.material->GetName().c_str(),
            handle.domain,
            handle.binding);
    }
    else
    {
        handle.binding = mm->CreateDomainMaterialBinding(handle.domain, handle.material);
        if (!handle.binding)
        {
            std::fprintf(stderr,
                "[MaterialRecipeRegistry] Acquire(key) fail: CreateDomainMaterialBinding null material='%s' domain=%p schema=%u req_prim=%u\n",
                handle.material->GetName().c_str(),
                handle.domain,
                static_cast<unsigned>(schema),
                static_cast<unsigned>(rec.prim));
            return {};
        }

        if (tm && sm && !rec.textures.empty())
        {
            if (!BindDomainTexturesFromRecord(handle.binding, tm, sm, rec))
                return {};
        }

        dmb_cache[dmb_key] = handle.binding;
        std::fprintf(stderr,
            "[MaterialRecipeRegistry] Acquire(key): create DMB material='%s' domain=%p binding=%p\n",
            handle.material->GetName().c_str(),
            handle.domain,
            handle.binding);
    }

    return handle;
}

MaterialBindingInstance *MaterialRecipeRegistry::ResolveOrCreateBindingInstance(const mtl::MaterialRecipe &rec,
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

// ── ResolveOrCreateBindingInstance (GVF auto-derive) ─────────────────────────────────────────────

MaterialBindingInstance *MaterialRecipeRegistry::ResolveOrCreateBindingInstance(
    const mtl::MaterialRecipe &rec,
    const GeometryVertexFormat &gvf,
    const void *instance_data,
    uint32_t instance_data_size,
    MaterialDomainHandle *out_handle,
    const VIL **out_vil)
{
    MaterialDomainHandle handle = Acquire(rec);
    if (!handle.material)
        return nullptr;

    if (!handle.domain)
        return nullptr;

    if (handle.material->hasMI() && !handle.binding)
        return nullptr;

    if (out_handle)
        *out_handle = handle;

    // 从 ShaderMaterialProgram DefaultVIL 与 GVF 的差异自动推算 VILConfig
    const VIL *default_vil = handle.material->GetDefaultVIL();
    if (!default_vil)
        return nullptr;

    MaterialInstanceSpec spec;
    spec.material = handle.material;
    spec.domain   = handle.domain;
    spec.preset   = BlendToPreset(rec.default_render_state.blend, rec.dim == mtl::MaterialRecipe::Dim::D2, rec.preset);
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
            {
                std::fprintf(stderr,
                    "[MaterialRecipeRegistry] ResolveOrCreateBindingInstance(rec+gvf) fail: vil_cfg.Add failed attrib=%u gvf_format=%u default_format=%u material='%s'\n",
                    static_cast<unsigned>(vif->attrib),
                    static_cast<unsigned>(gvf_format),
                    static_cast<unsigned>(vif->format),
                    handle.material->GetName().c_str());
                return nullptr;
            }
        }
    }

    if (vil_cfg.GetCount() > 0)
        spec.vil_cfg = &vil_cfg;

    // Stage-5: expose the actual VIL that will be used so callers can avoid
    // storing it in MaterialBindingInstance.
    if (out_vil)
        *out_vil = (vil_cfg.GetCount() > 0)
                       ? handle.material->CreateVIL(&vil_cfg)   // cached by ShaderMaterialProgram
                       : default_vil;

    MaterialBindingInstance *mi = mm->AcquireMaterialInstance(spec);
    if (mi)
    {
        MaterialBindingInstanceInternalAccess::SetDomainBinding(mi, handle.binding);

        std::fprintf(stderr,
            "[MaterialRecipeRegistry] ResolveOrCreateBindingInstance(key+gvf): mi=%p material=%p material_prim=%u preset=%u domain=%p vil=%p\n",
            mi,
            handle.material,
            static_cast<unsigned>(handle.material->GetPrimitiveType()),
            static_cast<unsigned>(spec.preset),
            handle.domain,
            out_vil ? *out_vil : nullptr);
    }

    return mi;
}

// ── ResolveOrCreateBindingInstance (key + GVF, key-transparent path) ─────────

MaterialBindingInstance *MaterialRecipeRegistry::ResolveOrCreateBindingInstance(
    const mtl::MaterialKey &key,
    const mtl::MaterialRecipe &rec,
    const GeometryVertexFormat &gvf,
    const void *instance_data,
    uint32_t instance_data_size,
    MaterialDomainHandle *out_handle,
    const VIL **out_vil)
{
    MaterialDomainHandle handle = Acquire(key, rec);
    if (!handle.material)
    {
        std::fprintf(stderr,
            "[MaterialRecipeRegistry] ResolveOrCreateBindingInstance(key+gvf) fail: handle.material=null key_hash=0x%llx preset=%u prim=%u pipeline=%u\n",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(rec.preset),
            static_cast<unsigned>(rec.prim),
            static_cast<unsigned>(BlendToPreset(rec.default_render_state.blend, rec.dim == mtl::MaterialRecipe::Dim::D2)));
        return nullptr;
    }

    if (!handle.domain)
    {
        std::fprintf(stderr,
            "[MaterialRecipeRegistry] ResolveOrCreateBindingInstance(key+gvf) fail: handle.domain=null key_hash=0x%llx material='%s'\n",
            static_cast<unsigned long long>(key.Hash()),
            handle.material->GetName().c_str());
        return nullptr;
    }

    if (handle.material->hasMI() && !handle.binding)
    {
        std::fprintf(stderr,
            "[MaterialRecipeRegistry] ResolveOrCreateBindingInstance(key+gvf) fail: hasMI but handle.binding=null key_hash=0x%llx material='%s' domain=%p\n",
            static_cast<unsigned long long>(key.Hash()),
            handle.material->GetName().c_str(),
            handle.domain);
        return nullptr;
    }

    if (out_handle)
        *out_handle = handle;

    const VIL *default_vil = handle.material->GetDefaultVIL();
    if (!default_vil)
    {
        std::fprintf(stderr,
            "[MaterialRecipeRegistry] ResolveOrCreateBindingInstance(key+gvf) fail: default_vil=null key_hash=0x%llx material='%s'\n",
            static_cast<unsigned long long>(key.Hash()),
            handle.material->GetName().c_str());
        return nullptr;
    }

    MaterialInstanceSpec spec;
    spec.material = handle.material;
    spec.domain   = handle.domain;
    spec.preset   = BlendToPreset(rec.default_render_state.blend, rec.dim == mtl::MaterialRecipe::Dim::D2, rec.preset);
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
            continue;

        if (gvf_format != vif->format)
        {
            VAConfig vac;
            vac.format = gvf_format;

            if (!vil_cfg.Add(vif->attrib, vac))
            {
                std::fprintf(stderr,
                    "[MaterialRecipeRegistry] ResolveOrCreateBindingInstance(key+gvf) fail: vil_cfg.Add failed key_hash=0x%llx attrib=%u gvf_format=%u default_format=%u material='%s'\n",
                    static_cast<unsigned long long>(key.Hash()),
                    static_cast<unsigned>(vif->attrib),
                    static_cast<unsigned>(gvf_format),
                    static_cast<unsigned>(vif->format),
                    handle.material->GetName().c_str());
                return nullptr;
            }
        }
    }

    if (vil_cfg.GetCount() > 0)
        spec.vil_cfg = &vil_cfg;

    if (out_vil)
        *out_vil = (vil_cfg.GetCount() > 0)
                       ? handle.material->CreateVIL(&vil_cfg)
                       : default_vil;

    MaterialBindingInstance *mi = mm->AcquireMaterialInstance(spec);
    if (mi)
    {
        MaterialBindingInstanceInternalAccess::SetDomainBinding(mi, handle.binding);
    }
    else
    {
        std::fprintf(stderr,
            "[MaterialRecipeRegistry] ResolveOrCreateBindingInstance(key+gvf) fail: AcquireMaterialInstance returned null key_hash=0x%llx material='%s' prim=%u pipeline=%u instance_data_size=%u\n",
            static_cast<unsigned long long>(key.Hash()),
            handle.material->GetName().c_str(),
            static_cast<unsigned>(rec.prim),
            static_cast<unsigned>(BlendToPreset(rec.default_render_state.blend, rec.dim == mtl::MaterialRecipe::Dim::D2)),
            static_cast<unsigned>(instance_data_size));
    }

    return mi;
}

// ── CreateMI ─────────────────────────────────────────────────────────────────

MaterialBindingInstance *MaterialRecipeRegistry::CreateMI(
    const MaterialDomainHandle &handle,
    const mtl::MaterialRecipe &rec,
    const void *instance_data,
    uint32_t instance_data_size)
{
    if (!handle.material || !handle.domain)
        return nullptr;

    MaterialInstanceSpec spec;
    spec.material = handle.material;
    spec.domain   = handle.domain;
    spec.preset   = BlendToPreset(rec.default_render_state.blend, rec.dim == mtl::MaterialRecipe::Dim::D2, rec.preset);
    spec.instance_data      = instance_data;
    spec.instance_data_size = instance_data_size;

    MaterialBindingInstance *mi = mm->AcquireMaterialInstance(spec);
    if (mi)
        MaterialBindingInstanceInternalAccess::SetDomainBinding(mi, handle.binding);

    return mi;
}

} // namespace hgl::graph
