#pragma once

#include <hgl/common/AttributeProvider.h>
#include <cstdint>

namespace hgl::graph
{
    constexpr uint32_t kVertexStreamAttributeBindingBegin = 0u;
    constexpr uint32_t kVertexStreamAttributeBindingCount = uint32_t(AttributeSemantic::BuiltinCount);
    constexpr uint32_t kVertexStreamPositionBinding = kVertexStreamAttributeBindingCount;
    constexpr uint32_t kVertexStreamBindingCount = kVertexStreamPositionBinding + 1u;

    static_assert(kVertexStreamPositionBinding == uint32_t(AttributeSemantic::BuiltinCount));
    static_assert(kVertexStreamBindingCount == (uint32_t(AttributeSemantic::BuiltinCount) + 1u));
}