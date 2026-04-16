#include <hgl/mtl/ShaderDataSchema.h>
#include <cassert>

namespace hgl::graph::mtl {

static const ShaderDataSchemaInfo kSchemaTable[] = {
    // None
    { nullptr,                          nullptr,             0  },
    // Color4f
    { "schema_color4f.glsl",            "MaterialInstance",  16 },
    // TextColor
    { "schema_text_color.glsl",         "MaterialInstance",  4  },
    // BillboardSizeUVec2
    { "schema_billboard_size.glsl",     "MaterialInstance",  8  },
    // PBRColorParams
    { "schema_pbr_color_params.glsl",   "MaterialInstance",  12 },
    // StandardParams
    { "schema_standard_params.glsl",    "MaterialInstance",  16 },
    // TextureArrayID
    { "schema_texture_array_id.glsl",   "MaterialInstance",  16 },
};

static_assert(sizeof(kSchemaTable) / sizeof(kSchemaTable[0]) ==
              static_cast<uint32_t>(ShaderDataSchema::COUNT),
              "kSchemaTable entry count must match ShaderDataSchema::COUNT");

const ShaderDataSchemaInfo & GetShaderDataSchemaInfo(ShaderDataSchema schema)
{
    const uint32_t idx = static_cast<uint32_t>(schema);
    assert(idx < static_cast<uint32_t>(ShaderDataSchema::COUNT));
    return kSchemaTable[idx];
}

} // namespace hgl::graph::mtl
