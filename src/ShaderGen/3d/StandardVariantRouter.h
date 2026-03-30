#pragma once

#include <hgl/mtl/MaterialVariantKey.h>

namespace hgl::graph::mtl
{

struct StandardVariantPolicyResult
{
    TextureSourceMode resolved_base = TextureSourceMode::None;
    TextureSourceMode resolved_normal = TextureSourceMode::None;

    bool base_is_array = false;
    bool normal_is_array = false;
    bool any_array = false;

    MaterialVariantKey route_key;
    MaterialVariantKey assemble_key;
};

StandardVariantPolicyResult BuildStandardVariantPolicy(const MaterialVariantKey &input_key);

} // namespace hgl::graph::mtl
