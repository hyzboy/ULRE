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

    const char       *sampler_symbol = mtl::ToGLSLSamplerSymbol(src.slot);
    const char       *slot_name      = mtl::SamplerSlotNameList[uint8_t(src.slot)];
    const std::string upper_name     = mtl::ToUpperASCII(slot_name);
    const std::string layer_var      = std::string("_tex_layer_") + slot_name;

    writer.EmitLayoutBinding(set, binding)
          .EmitUniform("sampler2DArray", sampler_symbol);

    writer.EmitVariable("uint", layer_var);

    writer.EmitDefine("HAS_SAMPLER_" + upper_name)
          .EmitDefine("SAMPLER_" + upper_name + "_ARRAY");

    if (src.builtin.output_format == ColorSourceOutputFormat::Grayscale_R)
        writer.EmitDefine("SAMPLER_" + upper_name + "_GRAYSCALE");
}

void BuiltinSampler2DArrayCodegen::EmitGetterFunction(ShaderWriter          &writer,
                                                      const ColorSource      &src,
                                                      const ResolvedBindings & /*resolved_bindings*/) const
{
    const char *sampler_symbol = mtl::ToGLSLSamplerSymbol(src.slot);
    const char *slot_name      = mtl::SamplerSlotNameList[uint8_t(src.slot)];
    const std::string layer_var = std::string("_tex_layer_") + slot_name;

    std::string tmp;
    if (src.builtin.output_format == ColorSourceOutputFormat::Grayscale_R)
    {
        tmp += "vec4 GetSampler"; tmp += slot_name;
        tmp += "(vec2 uv) { float r = texture("; tmp += sampler_symbol;
        tmp += ", vec3(uv, float("; tmp += layer_var; tmp += "))).r; return vec4(r,r,r,r); }\n";
    }
    else
    {
        tmp += "vec4 GetSampler"; tmp += slot_name;
        tmp += "(vec2 uv) { return texture("; tmp += sampler_symbol;
        tmp += ", vec3(uv, float("; tmp += layer_var; tmp += "))); }\n";
    }
    writer.EmitLine(tmp);
}

} // namespace hgl::graph
