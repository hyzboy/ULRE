#ifndef INSTANCE_ROWS_SSBO_GLSL
#define INSTANCE_ROWS_SSBO_GLSL

#include "common/descriptor_macros.glsl"

// Each macro declares the buffer AND its Resolve helper, then ends with
// "const int _anchor = 0" that absorbs the semicolon added by the call site
// (e.g. "L2W_INDEX_ROWS_SSBO;"). Without this anchor, GLSL 4.50 rejects
// the extraneous ";" that follows the closing "}" of the function body.
// This also ensures each Resolve function is defined AFTER its buffer,
// regardless of which macros the caller chooses to expand.

#define L2W_INDEX_ROWS_SSBO \
    layout(set=L2W_INDEX_ROWS_SET, binding=L2W_INDEX_ROWS_BINDING) readonly buffer LocalToWorldIndexRows { \
        uint values[]; \
    } l2w_index_rows; \
    uint ResolveTransformID(uint iid) { return l2w_index_rows.values[iid]; } \
    const int _ulre_l2w_rows_anchor = 0

#define DATA_INDEX_ROWS_SSBO \
    layout(set=MI_DATA_INDEX_ROWS_SET, binding=MI_DATA_INDEX_ROWS_BINDING) readonly buffer DataIndexRows { \
        uint values[]; \
    } mtl_data_index_rows; \
    uint ResolveDataIndexID(uint iid) { return mtl_data_index_rows.values[iid]; } \
    const int _ulre_data_rows_anchor = 0

#define TEXTURE_LAYER_ROWS_SSBO \
    layout(set=MI_TEXTURE_LAYER_ROWS_SET, binding=MI_TEXTURE_LAYER_ROWS_BINDING) readonly buffer TextureLayerRows { \
        uint values[]; \
    } mtl_texture_layer_rows; \
    uint ResolveTextureLayerID(uint iid) { return mtl_texture_layer_rows.values[iid]; } \
    const int _ulre_texlayer_rows_anchor = 0

#endif // INSTANCE_ROWS_SSBO_GLSL
