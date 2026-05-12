#include<hgl/graph/module/TextureDomainRegistry.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKDomainResourceBinding.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/mtl/SamplerSlot.h>
#include<functional>

namespace hgl::graph
{
    // ── Static member definition ──────────────────────────────────────────────
    std::unordered_map<std::string, TextureDomainRegistry::DomainEntry>
        TextureDomainRegistry::s_entries;

    constexpr uint32_t kDefaultDomainMaxLayers = 256;

    // ── RegisterTexture ───────────────────────────────────────────────────────
    int TextureDomainRegistry::RegisterTexture(const std::string& domain_tag,
                                               const hgl::OSString& texture_path)
    {
        auto& entry = s_entries[domain_tag];
        if (entry.domain_tag.empty())
        {
            entry.domain_tag  = domain_tag;
            entry.max_layers  = kDefaultDomainMaxLayers;
            entry.used_layers = 0;
            entry.dirty       = true;
        }

        auto it = entry.path_to_layer.find(texture_path);
        if (it != entry.path_to_layer.end())
            return static_cast<int>(it->second);

        if (entry.used_layers >= entry.max_layers)
            return -1; // capacity full

        uint32_t layer = entry.used_layers++;
        entry.path_to_layer[texture_path] = layer;
        entry.dirty = true;
        return static_cast<int>(layer);
    }

    // ── GetEntry ──────────────────────────────────────────────────────────────
    TextureDomainRegistry::DomainEntry*
    TextureDomainRegistry::GetEntry(const std::string& domain_tag)
    {
        auto it = s_entries.find(domain_tag);
        return (it != s_entries.end()) ? &it->second : nullptr;
    }

    // ── ForEach ───────────────────────────────────────────────────────────────
    void TextureDomainRegistry::ForEach(
        const std::function<void(const std::string&, DomainEntry&)>& fn)
    {
        for (auto& [tag, entry] : s_entries)
            fn(tag, entry);
    }

    // ── EnsureResources ───────────────────────────────────────────────────────
    bool TextureDomainRegistry::EnsureResources(
        GraphicsContext* gc,
        std::function<bool(const std::string&, DomainEntry&, GraphicsContext*)> build_material)
    {
        if (!gc)
            return false;

        auto* texture_manager = gc->GetTextureManager();
        auto* sampler_manager = gc->GetSamplerManager();
        if (!texture_manager || !sampler_manager)
            return false;

        bool all_ok = true;

        for (auto& [tag, entry] : s_entries)
        {
            if (!entry.dirty && entry.texture_array)
                continue;

            if (entry.path_to_layer.empty())
                continue;

            // ── Texture2DArray ────────────────────────────────────────────
            if (entry.texture_array)
            {
                texture_manager->Destory(entry.texture_array);
                entry.texture_array = nullptr;
            }

            // Probe dimensions from first texture
            auto first_it = entry.path_to_layer.begin();
            auto* probe = texture_manager->LoadTexture2D(first_it->first, false);
            if (!probe)
            {
                all_ok = false;
                continue;
            }

            const uint32_t  tex_w   = probe->GetWidth();
            const uint32_t  tex_h   = probe->GetHeight();
            const VkFormat  tex_fmt = probe->GetFormat();

            entry.texture_array = texture_manager->CreateTexture2DArray(
                tex_w, tex_h, entry.used_layers, tex_fmt, false);

            if (!entry.texture_array)
            {
                all_ok = false;
                continue;
            }

            for (auto& [path, layer] : entry.path_to_layer)
            {
                if (!texture_manager->LoadTexture2DArray(entry.texture_array, layer, path))
                    all_ok = false;
            }

            // ── Sampler ───────────────────────────────────────────────────
            if (!entry.sampler)
                entry.sampler = sampler_manager->CreateSampler();

            // ── ShaderMaterialProgram + DMB ───────────────────────────────
            if (build_material && (!entry.material || !entry.dmb))
            {
                if (!build_material(tag, entry, gc))
                {
                    all_ok = false;
                    continue;
                }
            }

            if (!entry.material || !entry.dmb)
                continue; // will be ready after material callback next time

            // Bind texture array to descriptors
            entry.dmb->BindResourceSampler(mtl::SamplerSlot::BaseColor,
                                            entry.texture_array,
                                            entry.sampler);
            entry.dmb->Update();

            entry.material->BindResourceSampler(mtl::SamplerSlot::BaseColor,
                                                 entry.texture_array,
                                                 entry.sampler);
            entry.material->Update();

            entry.dirty = false;
        }

        return all_ok;
    }

    // ── ReleaseAll ────────────────────────────────────────────────────────────
    void TextureDomainRegistry::ReleaseAll(GraphicsContext* gc)
    {
        if (!gc)
        {
            s_entries.clear();
            return;
        }

        auto* texture_manager   = gc->GetTextureManager();
        auto* sampler_manager   = gc->GetSamplerManager();
        auto* primitive_manager = gc->GetPrimitiveManager();

        for (auto& [tag, entry] : s_entries)
        {
            if (entry.primitive && primitive_manager)
            {
                auto* geometry = entry.primitive->GetGeometry();
                primitive_manager->Release(entry.primitive);
                if (geometry)
                    delete geometry;
            }

            if (entry.texture_array && texture_manager)
                texture_manager->Destory(entry.texture_array);

            if (entry.sampler && sampler_manager)
                sampler_manager->Release(entry.sampler);
        }

        s_entries.clear();
    }

}//namespace hgl::graph
