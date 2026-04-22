#include <hgl/mtl/MaterialVariantRegistry.h>
#include <atomic>

namespace hgl::graph::mtl
{
    static std::atomic<VariantRegistryStatsSink *> g_sink{nullptr};

    void SetGlobalVariantRegistryStatsSink(VariantRegistryStatsSink *sink)
    {
        g_sink.store(sink, std::memory_order_release);
    }

    VariantRegistryStatsSink *GetGlobalVariantRegistryStatsSink()
    {
        return g_sink.load(std::memory_order_acquire);
    }
}
