#include "BuiltinSamplerCodegen.h"
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/shadergen/ShaderWriter.h>

namespace hgl::graph
{

// ── BuiltinSampler2DArrayCodegen ─────────────────────────────────────────────

void BuiltinSampler2DArrayCodegen::EmitDeclarations(ShaderWriter          &writer,
                                                    const ColorSource      &src,
                                                    const ResolvedBindings &resolved_bindings) const
{
    const std::string &debug_name = src.bindings[0].debug_name;
    uint32_t set = 0, binding = 0;
    FindResolved(resolved_bindings, debug_name, set, binding);

    const char *sampler_symbol = mtl::ToGLSLSamplerSymbol(src.slot);

    writer.EmitLayoutBinding(set, binding)
          .EmitUniform("sampler2DArray", sampler_symbol);
}

void BuiltinSampler2DArrayCodegen::EmitGetterFunction(ShaderWriter          &writer,
                                                      const ColorSource      &src,
                                                      const ResolvedBindings & /*resolved_bindings*/) const
{
    const char *sampler_symbol = mtl::ToGLSLSamplerSymbol(src.slot);
    const char *slot_name      = mtl::SamplerSlotNameList[uint8_t(src.slot)];

    std::string tmp;
    if (src.builtin.output_format == ColorSourceOutputFormat::Grayscale_R)
    {
        tmp += "vec4 GetSampler"; tmp += slot_name;
        tmp += "(uint mi_id, vec2 uv) { uint _layer = GetMITLayer_"; tmp += slot_name;
        tmp += "(mi_id);"
               " float r = texture("; tmp += sampler_symbol;
        tmp += ", vec3(uv, float(_layer))).r; return vec4(r,r,r,r); }\n";
    }
    else
    {
        tmp += "vec4 GetSampler"; tmp += slot_name;
        tmp += "(uint mi_id, vec2 uv) { uint _layer = GetMITLayer_"; tmp += slot_name;
        tmp += "(mi_id);"
               " return texture("; tmp += sampler_symbol;
        tmp += ", vec3(uv, float(_layer))); }\n";
    }
    writer.EmitLine(tmp);
}

} // namespace hgl::graph
