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
#include <cctype>
#include <cstdio>
#include <limits>

namespace hgl::graph
{

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
    const std::vector<mtl::MaterialRecipe::TextureSlotConfig> &textures)
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

static bool ParseDomainIDString(const std::string &did, uint32_t &out_id)
{
    if (did.empty())
        return false;

    for (char ch : did)
    {
        if (!std::isdigit(static_cast<unsigned char>(ch)))
            return false;
    }

    const unsigned long long value = std::strtoull(did.c_str(), nullptr, 10);
    if (value > std::numeric_limits<uint32_t>::max())
        return false;

    out_id = static_cast<uint32_t>(value);
    return true;
}

static bool HasExplicitVertexStreamProviders(const mtl::MaterialRecipe &rec)
{
    if (rec.position_provider.has_value())
        return true;

    for (const auto provider : rec.attribute_providers)
    {
        if (provider != AttributeProviderId::None)
            return true;
    }

    return false;
}

static AttributeProviderId InferProviderByFormat(const VertexAttrib attrib, const VkFormat format)
{
    if (format == VK_FORMAT_UNDEFINED)
        return AttributeProviderId::None;

    switch (attrib)
    {
        case VertexAttrib::Normal:
        case VertexAttrib::Tangent:
        case VertexAttrib::Bitangent:
        {
            if (format == VK_FORMAT_R32G32B32_SFLOAT)
                return AttributeProviderId::SSBO_Vec3;

            if (format == VK_FORMAT_R32G32B32A32_SFLOAT)
                return AttributeProviderId::SSBO_Vec4;

            return AttributeProviderId::None;
        }

        case VertexAttrib::Color:
        {
            if (format == VK_FORMAT_R8G8B8A8_UNORM
             || format == VK_FORMAT_B8G8R8A8_UNORM
             || format == VK_FORMAT_A8B8G8R8_UNORM_PACK32)
                return AttributeProviderId::SSBO_PackedRGBA8;

            if (format == VK_FORMAT_R32G32B32A32_SFLOAT)
                return AttributeProviderId::SSBO_Vec4;

            if (format == VK_FORMAT_R32G32B32_SFLOAT)
                return AttributeProviderId::SSBO_Vec3;

            return AttributeProviderId::None;
        }

        case VertexAttrib::TexCoord:
        {
            if (format == VK_FORMAT_R32G32_SFLOAT)
                return AttributeProviderId::SSBO_Vec2;

            if (format == VK_FORMAT_R16G16_UNORM)
                return AttributeProviderId::SSBO_PackedUV_2x16;

            return AttributeProviderId::None;
        }

        case VertexAttrib::JointID:
        case VertexAttrib::JointWeight:
        {
            if (format == VK_FORMAT_R32G32B32A32_SFLOAT)
                return AttributeProviderId::SSBO_Vec4;

            return AttributeProviderId::None;
        }

        default:
            return AttributeProviderId::None;
    }
}

static bool BuildLegacyVertexStreamBridgeRecipe(const mtl::MaterialRecipe &rec,
                                                const GeometryVertexFormat &gvf,
                                                mtl::MaterialRecipe &out_recipe)
{
    out_recipe = rec;

    if (HasExplicitVertexStreamProviders(rec))
        return false;

    bool changed = false;

    if (!out_recipe.position_provider.has_value())
    {
        const VkFormat pos_format = gvf.GetFormat(VertexAttrib::Position);

        // Legacy bridge: when old callers still provide V2F/V3F position via
        // VAB, auto-switch to SSBO position fetch without requiring recipe edits.
        if (pos_format == VK_FORMAT_R32G32_SFLOAT)
        {
            out_recipe.position_provider = PositionProviderId::SSBO_PackedVec2;
            changed = true;
        }
        else if (pos_format == VK_FORMAT_R32G32B32_SFLOAT)
        {
            out_recipe.position_provider = PositionProviderId::SSBO_PackedVec3;
            changed = true;
        }
    }

    constexpr VertexAttrib kBridgeAttribs[] = {
        VertexAttrib::Normal,
        VertexAttrib::Tangent,
        VertexAttrib::Color,
        VertexAttrib::TexCoord,
        VertexAttrib::JointID,
        VertexAttrib::JointWeight,
    };

    for (const VertexAttrib attrib : kBridgeAttribs)
    {
        if (!gvf.Has(attrib))
            continue;

        const size_t attrib_index = size_t(attrib);
        if (attrib_index >= out_recipe.attribute_providers.size())
            continue;

        if (out_recipe.attribute_providers[attrib_index] != AttributeProviderId::None)
            continue;

        const AttributeProviderId inferred = InferProviderByFormat(attrib, gvf.GetFormat(attrib));
        if (inferred == AttributeProviderId::None)
            continue;

        out_recipe.attribute_providers[attrib_index] = inferred;
        changed = true;
    }

    return changed;
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
    return Acquire(mtl::ResolveRecipePrimaryKey(rec), rec);
}

MaterialDomainHandle MaterialRecipeRegistry::Acquire(const mtl::MaterialKey &key, const mtl::MaterialRecipe &rec)
{
    MaterialDomainHandle handle;

    // 1. ShaderMaterialProgram — key-transparent fast path (checks material_by_key first)
    handle.material = mm->GetOrCreateProgramByKey(key, rec);
    if (!handle.material)
        return {};

    const AnsiString &mat_name = handle.material->GetName();
    std::string mat_name_str(mat_name.c_str() ? mat_name.c_str() : "",
                             mat_name.c_str() ? static_cast<size_t>(mat_name.Length()) : 0);

    // 2. ResourceDomain (按 schema + domain_id 缓存)
    const std::string &did = rec.domain_id;          // 空串 → 默认域
    const auto schema = handle.material->GetShaderDataSchema();

    uint32_t numeric_domain_id = 0;
    if (!ParseDomainIDString(did, numeric_domain_id))
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
    }
    else
    {
        handle.binding = mm->CreateDomainMaterialBinding(handle.domain, handle.material);
        if (!handle.binding)
            return {};

        if (tm && sm && !rec.textures.empty())
        {
            if (!BindDomainTexturesFromRecord(handle.binding, tm, sm, rec))
                return {};
        }

        dmb_cache[dmb_key] = handle.binding;
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
    MaterialDomainHandle *out_handle)
{
    mtl::MaterialRecipe bridged_recipe;
    const bool bridged = BuildLegacyVertexStreamBridgeRecipe(rec, gvf, bridged_recipe);
    const mtl::MaterialRecipe &effective_recipe = bridged ? bridged_recipe : rec;

    MaterialDomainHandle handle = Acquire(effective_recipe);
    if (!handle.material)
        return nullptr;

    if (!handle.domain)
        return nullptr;

    if (handle.material->hasMI() && !handle.binding)
        return nullptr;

    if (out_handle)
        *out_handle = handle;

    MaterialInstanceSpec spec;
    spec.material = handle.material;
    spec.domain   = handle.domain;
    spec.preset   = effective_recipe.pipeline;
    spec.instance_data      = instance_data;
    spec.instance_data_size = instance_data_size;

    MaterialBindingInstance *mi = mm->AcquireMaterialInstance(spec);
    if (mi)
        MaterialBindingInstanceInternalAccess::SetDomainBinding(mi, handle.binding);

    return mi;
}

// ── ResolveOrCreateBindingInstance (key + GVF, key-transparent path) ─────────

MaterialBindingInstance *MaterialRecipeRegistry::ResolveOrCreateBindingInstance(
    const mtl::MaterialKey &key,
    const mtl::MaterialRecipe &rec,
    const GeometryVertexFormat &gvf,
    const void *instance_data,
    uint32_t instance_data_size,
    MaterialDomainHandle *out_handle)
{
    mtl::MaterialRecipe bridged_recipe;
    const bool bridged = BuildLegacyVertexStreamBridgeRecipe(rec, gvf, bridged_recipe);
    const mtl::MaterialRecipe &effective_recipe = bridged ? bridged_recipe : rec;
    const mtl::MaterialKey effective_key = bridged ? mtl::ResolveRecipePrimaryKey(effective_recipe) : key;

    MaterialDomainHandle handle = Acquire(effective_key, effective_recipe);
    if (!handle.material)
        return nullptr;

    if (!handle.domain)
        return nullptr;

    if (handle.material->hasMI() && !handle.binding)
        return nullptr;

    if (out_handle)
        *out_handle = handle;

    MaterialInstanceSpec spec;
    spec.material = handle.material;
    spec.domain   = handle.domain;
    spec.preset   = effective_recipe.pipeline;
    spec.instance_data      = instance_data;
    spec.instance_data_size = instance_data_size;

    MaterialBindingInstance *mi = mm->AcquireMaterialInstance(spec);
    if (mi)
        MaterialBindingInstanceInternalAccess::SetDomainBinding(mi, handle.binding);

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
    spec.preset   = rec.pipeline;
    spec.instance_data      = instance_data;
    spec.instance_data_size = instance_data_size;

    MaterialBindingInstance *mi = mm->AcquireMaterialInstance(spec);
    if (mi)
        MaterialBindingInstanceInternalAccess::SetDomainBinding(mi, handle.binding);

    return mi;
}

} // namespace hgl::graph
