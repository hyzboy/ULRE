#include "StandardVariantPolicy.h"

#include <hgl/mtl/SamplerName.h>

namespace hgl::graph::mtl
{

StandardVariantPolicyResult BuildStandardVariantPolicy(const MaterialVariantKey &input_key)
{
    StandardVariantPolicyResult result;

    result.resolved_base = input_key.GetTextureSourceMode(SamplerSlot::BaseColor);
    result.resolved_normal = input_key.GetTextureSourceMode(SamplerSlot::Normal);

    result.base_is_array = (result.resolved_base == TextureSourceMode::Array);
    result.normal_is_array = (result.resolved_normal == TextureSourceMode::Array);
    result.any_array = result.base_is_array || result.normal_is_array;

    result.route_key = input_key;
    result.route_key.surface_type = SurfaceType::Standard;
    result.route_key.SetTextureSourceMode(SamplerSlot::BaseColor, result.any_array ? TextureSourceMode::Array : TextureSourceMode::Simple);
    result.route_key.SetTextureSourceMode(SamplerSlot::Normal, result.any_array ? TextureSourceMode::Array : TextureSourceMode::Simple);
    result.route_key.SetHasTexture(SamplerSlot::BaseColor);
    result.route_key.SetHasTexture(SamplerSlot::Normal);

    result.assemble_key = result.route_key;
    result.assemble_key.SetTextureSourceMode(SamplerSlot::BaseColor, result.resolved_base);
    result.assemble_key.SetTextureSourceMode(SamplerSlot::Normal, result.resolved_normal);

    return result;
}

} // namespace hgl::graph::mtl
