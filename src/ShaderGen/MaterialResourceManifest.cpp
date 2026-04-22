#include <hgl/mtl/MaterialResourceManifest.h>

namespace hgl::graph::mtl
{
    MaterialResourceManifest MaterialResourceManifest::FromStaticDef(const StaticMaterialDef &def)
    {
        MaterialResourceManifest m;

        if (def.ubo_descriptors)
            m.ubos = *def.ubo_descriptors;

        if (def.ssbo_descriptors)
            m.ssbos = *def.ssbo_descriptors;

        if (def.texture_samplers)
            m.samplers = *def.texture_samplers;

        return m;
    }

    void MaterialResourceManifest::MergeOverwrite(const MaterialResourceManifest &other)
    {
        ubos.insert(other.ubos.begin(), other.ubos.end());
        ssbos.insert(other.ssbos.begin(), other.ssbos.end());

        for (const auto &[slot, desc] : other.samplers)
            samplers[slot] = desc;
    }

    void MaterialResourceManifest::MergeKeepFirst(const MaterialResourceManifest &other)
    {
        ubos.insert(other.ubos.begin(), other.ubos.end());
        ssbos.insert(other.ssbos.begin(), other.ssbos.end());

        for (const auto &[slot, desc] : other.samplers)
            samplers.try_emplace(slot, desc);
    }

    StaticMaterialDef MaterialResourceManifest::ProjectIntoStaticDef(const StaticMaterialDef &base_def) const
    {
        StaticMaterialDef def = base_def;
        def.ubo_descriptors  = ubos.empty()     ? nullptr : &ubos;
        def.ssbo_descriptors = ssbos.empty()    ? nullptr : &ssbos;
        def.texture_samplers = samplers.empty() ? nullptr : &samplers;
        return def;
    }

} // namespace hgl::graph::mtl
