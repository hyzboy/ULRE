#include <hgl/shadergen/MaterialFactory3D.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <ankerl/unordered_dense.h>
#include <cstdio>
#include <mutex>

namespace hgl{namespace graph{namespace mtl{

#define ULRE_FOR_EACH_BUILTIN_FACTORY(X) \
    X(PureColor2D)                      \
    X(PureTexture2D)                    \
    X(VertexColor2D)                    \
    X(Text2D)                           \
    X(Checkerboard3D)                   \
    X(FullscreenTriangle)               \
    X(PureColor3D)                      \
    X(VertexColor3D)                    \
    X(VertexLuminance3D)                \
    X(VertexLuminance2D)                \
    X(Billboard2DDynamic)               \
    X(Billboard2DFixed)                 \
    X(Gizmo3D)                          \
    X(SkyMinimal)                       \
    X(Standard)                         \
    X(PBRColor3D)                       \
    X(VertexPaletteColor3D)             \
    X(TerrainGrid)

#define DECLARE_BUILTIN_FACTORY_REGISTER_FN(preset) void RegisterBuiltinFactory_##preset();
    ULRE_FOR_EACH_BUILTIN_FACTORY(DECLARE_BUILTIN_FACTORY_REGISTER_FN)
#undef DECLARE_BUILTIN_FACTORY_REGISTER_FN

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

        void RegisterBuiltinFactoriesImpl()
        {
#define REGISTER_BUILTIN_IF_MISSING(preset)                                         \
            if (!MaterialFactory3D::GetRegisteredName(MaterialPreset::preset))      \
                RegisterBuiltinFactory_##preset();
            ULRE_FOR_EACH_BUILTIN_FACTORY(REGISTER_BUILTIN_IF_MISSING)
#undef REGISTER_BUILTIN_IF_MISSING
        }
    } // anonymous namespace

    void MaterialFactory3D::RegisterBuiltinFactories()
    {
        static std::once_flag once;
        std::call_once(once, []()
        {
            RegisterBuiltinFactoriesImpl();
        });
    }

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

    std::unique_ptr<MaterialCreateInfo> MaterialFactory3D::Create(
        MaterialPreset                             preset,
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialVariantDesc                 *desc,
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
        return it->second.fn(profile, desc, key, cfg);
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

#undef ULRE_FOR_EACH_BUILTIN_FACTORY
}}} // namespace hgl::graph::mtl
