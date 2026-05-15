#include "CodegenRegistry.h"

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
    // Step 1: 骨架 —— 返回空注册表；后续 Step 2 在此处注册内置实现。
    static ColorSourceCodegenRegistry instance;
    return instance;
}

} // namespace hgl::graph
