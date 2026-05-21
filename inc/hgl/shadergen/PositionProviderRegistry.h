#pragma once

#include <hgl/common/PositionProvider.h>

namespace hgl::graph
{
    /// Returns a pointer to the built-in PositionProvider record for @p id,
    /// or nullptr if @p id is not a known built-in ID.
    const PositionProvider *FindBuiltinProvider(PositionProviderId id) noexcept;

    /// Returns a pointer to a contiguous array of all defined builtin
    /// PositionProviderIds (excludes Unknown, Invalid, UserPCG).
    /// *out_count receives the element count.
    const PositionProviderId *GetAllBuiltinProviderIds(size_t *out_count) noexcept;

}//namespace hgl::graph
