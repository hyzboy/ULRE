#pragma once

#include <hgl/common/AttributeProvider.h>

namespace hgl::graph
{
    /// Returns a pointer to the built-in AttributeProvider record for @p id,
    /// or nullptr if @p id is not a known built-in ID.
    const AttributeProvider *FindBuiltinAttribProvider(AttributeProviderId id) noexcept;

}//namespace hgl::graph
