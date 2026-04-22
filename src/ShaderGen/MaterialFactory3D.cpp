#include <hgl/shadergen/MaterialFactory3D.h>
#include <ankerl/unordered_dense.h>
#include <cstdio>

namespace hgl::graph::mtl
{
    namespace
    {
        struct FactoryEntry
        {
            const char    *name;
            PresetFactoryFn fn;
        };

        // Meyers singleton — safe against static-init order issues.
        ankerl::unordered_dense::map<MaterialPreset, FactoryEntry> &Registry()
        {
            static ankerl::unordered_dense::map<MaterialPreset, FactoryEntry> r;
            return r;
        }
    } // anonymous namespace

    bool MaterialFactory3D::Register(MaterialPreset preset, const char *name, PresetFactoryFn fn)
    {
        if (!fn) return false;
        auto &r = Registry();
        if (r.contains(preset))
        {
            std::fprintf(stderr,
                "[MaterialFactory3D] WARN: duplicate registration for preset=%u name=%s (kept first)\n",
                static_cast<unsigned>(preset), name ? name : "<null>");
            return false;
        }
        r.emplace(preset, FactoryEntry{ name ? name : "<unnamed>", fn });
        return true;
    }

    MaterialCreateInfo *MaterialFactory3D::Create(
        MaterialPreset                             preset,
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialVariantKey                  &key,
        MaterialCreateConfig                      *cfg)
    {
        auto &r = Registry();
        auto  it = r.find(preset);
        if (it == r.end())
        {
            std::fprintf(stderr,
                "[MaterialFactory3D] ERROR: no factory registered for preset=%u\n",
                static_cast<unsigned>(preset));
            return nullptr;
        }
        return it->second.fn(profile, key, cfg);
    }

    size_t MaterialFactory3D::RegisteredCount()
    {
        return Registry().size();
    }

    const char *MaterialFactory3D::GetRegisteredName(MaterialPreset preset)
    {
        auto &r = Registry();
        auto  it = r.find(preset);
        return it != r.end() ? it->second.name : nullptr;
    }
} // namespace hgl::graph::mtl
