#pragma once

#include <hgl/common/PositionProvider.h>

namespace hgl::graph
{
    /// Returns a pointer to the built-in PositionProvider record for @p id,
    /// or nullptr if @p id is not a known built-in ID.
    const PositionProvider *FindBuiltinProvider(PositionProviderId id) noexcept;

}//namespace hgl::graph
