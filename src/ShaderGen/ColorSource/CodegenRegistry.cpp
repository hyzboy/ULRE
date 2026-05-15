#include "CodegenRegistry.h"
#include "BuiltinSamplerCodegen.h"

#include <cassert>

namespace hgl::graph
{

void ColorSourceCodegenRegistry::Register(ColorSourceKind kind,
                                           std::unique_ptr<IColorSourceCodegen> impl)
{
    assert(impl && "ColorSourceCodegenRegistry::Register: null impl");
    const uint8_t key = static_cast<uint8_t>(kind);
    impls_[key] = std::move(impl);
}

const IColorSourceCodegen* ColorSourceCodegenRegistry::Find(ColorSourceKind kind) const
{
    const uint8_t key = static_cast<uint8_t>(kind);
    auto it = impls_.find(key);
    return (it != impls_.end()) ? it->second.get() : nullptr;
}

ColorSourceCodegenRegistry& ColorSourceCodegenRegistry::Global()
{
    static ColorSourceCodegenRegistry instance = []
    {
        ColorSourceCodegenRegistry reg;
        reg.Register(ColorSourceKind::BuiltinSampler2D,
                     std::make_unique<BuiltinSampler2DCodegen>());
        reg.Register(ColorSourceKind::BuiltinSampler2DArray,
                     std::make_unique<BuiltinSampler2DArrayCodegen>());
        return reg;
    }();
    return instance;
}

} // namespace hgl::graph
