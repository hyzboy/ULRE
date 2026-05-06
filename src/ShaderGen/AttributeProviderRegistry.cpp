#include <hgl/shadergen/AttributeProviderRegistry.h>

namespace hgl::graph
{
    using Id  = AttributeProviderId;

    // Stable, ordered by ID value.  Do NOT reorder or renumber entries.
    //
    // Columns:
    //   id                          glsl_path                                                               ssbo   ubo    samp   stride
    static const AttributeProvider kBuiltins[] =
    {
        { Id::None,                 "",                                                                      false, false, false,  0 },
        { Id::SSBO_Vec2,            "ShaderLibrary/attribute_provider/ssbo_vec2.glsl",                       true,  false, false,  8 },
        { Id::SSBO_Vec3,            "ShaderLibrary/attribute_provider/ssbo_vec3.glsl",                       true,  false, false, 12 },
        { Id::SSBO_Vec4,            "ShaderLibrary/attribute_provider/ssbo_vec4.glsl",                       true,  false, false, 16 },
        { Id::SSBO_PackedRGBA8,     "ShaderLibrary/attribute_provider/ssbo_packed_rgba8.glsl",               true,  false, false,  4 },
        { Id::SSBO_PackedNormal_Oct,"ShaderLibrary/attribute_provider/ssbo_packed_normal_oct.glsl",          true,  false, false,  4 },
        { Id::SSBO_PackedUV_2x16,   "ShaderLibrary/attribute_provider/ssbo_packed_uv_2x16.glsl",             true,  false, false,  4 },
        { Id::Constant,             "",                                                                      false, true,  false,  0 },
    };

    static constexpr size_t kBuiltinCount =
        sizeof(kBuiltins) / sizeof(kBuiltins[0]);

    const AttributeProvider *FindBuiltinAttribProvider(AttributeProviderId id) noexcept
    {
        for (size_t i = 0; i < kBuiltinCount; ++i)
            if (kBuiltins[i].id == id)
                return &kBuiltins[i];
        return nullptr;
    }

}//namespace hgl::graph
