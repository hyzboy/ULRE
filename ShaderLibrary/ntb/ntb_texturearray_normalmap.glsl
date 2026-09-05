// @ulre begin
// @ulre name ntb_texturearray_normalmap
// @ulre kind Utility
// @ulre priority 0
// @ulre slot ntb_provider
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
#ifdef ULRE_MATERIAL_ARENA_BDA
    // Arena+BDA：layer/句柄在材质数据行尾（MTL_ROW 宏自带行类型）
    PBRSurfaceRow material_row = MTL_ROW(ntb_input.dataIndex);
    const float layer = float(material_row.tex_custom0);
    const uint normalTexHandle = material_row.tex_normal;
#else
    const float layer = float(mtl_texture_layer_rows.data[ntb_input.dataIndex].custom0);
    const uint normalTexHandle = mtl_texture_layer_rows.data[ntb_input.dataIndex].normal;
#endif
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
