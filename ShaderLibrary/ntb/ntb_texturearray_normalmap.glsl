// @ulre begin
// @ulre name ntb_texturearray_normalmap
// @ulre kind Utility
// @ulre priority 0
// @ulre uses ntb_interface
// @ulre uses bindless_textures
// @ulre texture_layer custom0 Fragment required fallback
// @ulre end
// NTB provider for Texture2DArray normal maps.

#ifndef NTB_TEXTUREARRAY_NORMALMAP_GLSL
#define NTB_TEXTUREARRAY_NORMALMAP_GLSL

#include "common/ntb_interface.glsl"
#include "ntb/ntb_orthonormal.glsl"
#include "common/bindless_textures.glsl"

NTBSpace GetNTB(NTBInput ntb_input)
{
    const SurfaceInput si = ntb_input.surface;
    const float layer = float(mtl_texture_layer_rows.data[ntb_input.dataIndex].custom0);
    const uint normalTexHandle = mtl_texture_layer_rows.data[ntb_input.dataIndex].normal;
    NTBSpace ntb = BuildOrthoNTB(si.worldNormal);

    if (normalTexHandle != 0u)
    {
        vec3 nm =
            Sample2DArray(normalTexHandle, TrilinearSampler, si.uv0, layer).xyz
            * 2.0 - 1.0;
        nm.y = -nm.y;
        const vec3 tangentNormal =
            normalize(vec3(nm.xy * ntb_input.normalScale, nm.z));
        const mat3 TBN = mat3(ntb.T, ntb.B, ntb.N);
        ntb.N = normalize(TBN * tangentNormal);
        ntb = BuildOrthoNTB(ntb.N);
    }

    return ntb;
}

#endif // NTB_TEXTUREARRAY_NORMALMAP_GLSL
