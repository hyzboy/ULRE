#include "BuiltinSamplerCodegen.h"
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/shadergen/ShaderWriter.h>

namespace hgl::graph
{

// ── BuiltinSamplerCodegenBase 公共实现 ───────────────────────────────────────

bool BuiltinSamplerCodegenBase::FindResolved(const ResolvedBindings &resolved,
                                             const std::string      &debug_name,
                                             uint32_t               &out_set,
                                             uint32_t               &out_binding)
{
    for (const ResolvedBinding &rb : resolved)
    {
        if (rb.debug_name == debug_name)
        {
            out_set     = rb.set;
            out_binding = rb.binding;
            return true;
        }
    }
    out_set = out_binding = 0;
    return false;
}

void BuiltinSamplerCodegenBase::DeclareRequirements(
    const ColorSource                     &src,
    std::vector<DescriptorRequirement>    &out_bindings,
    std::vector<PushConstantRequirement>  &out_push) const
{
    for (const auto &b : src.bindings)
        out_bindings.push_back(b);
    for (const auto &p : src.push_constants)
        out_push.push_back(p);
}

ColorSourceValidationResult BuiltinSamplerCodegenBase::Validate(const ColorSource &src) const
{
    if (size_t(src.slot) >= mtl::SamplerSlotCount)
        return ColorSourceValidationResult::Fail("invalid SamplerSlot value");
    if (src.bindings.empty())
        return ColorSourceValidationResult::Fail("BuiltinSampler requires at least one DescriptorRequirement in bindings[]");
    return ColorSourceValidationResult::Ok();
}

// ── BuiltinSampler2DCodegen ───────────────────────────────────────────────────

void BuiltinSampler2DCodegen::EmitDeclarations(ShaderWriter          &writer,
                                               const ColorSource      &src,
                                               const ResolvedBindings &resolved_bindings) const
{
    // debug_name 由 ColorSource::MakeSampler2D 设置为 "Sampler_Xxx"
    const std::string &debug_name = src.bindings[0].debug_name;
    uint32_t set = 0, binding = 0;
    FindResolved(resolved_bindings, debug_name, set, binding);

    const char *sampler_symbol = mtl::ToGLSLSamplerSymbol(src.slot);

    writer.EmitLayoutBinding(set, binding)
          .EmitUniform("sampler2D", sampler_symbol);
}

void BuiltinSampler2DCodegen::EmitGetterFunction(ShaderWriter          &writer,
                                                 const ColorSource      &src,
                                                 const ResolvedBindings & /*resolved_bindings*/) const
{
    const char *sampler_symbol = mtl::ToGLSLSamplerSymbol(src.slot);
    const char *slot_name      = mtl::SamplerSlotNameList[uint8_t(src.slot)];

    // ShaderWriter 目前没有"任意字符串拼接"接口，直接往底层 string 写
    // 保持与现有 SamplerGLSLEmitter 字节级兼容
    std::string tmp;
    if (src.builtin.output_format == ColorSourceOutputFormat::Grayscale_R)
    {
        tmp += "vec4 GetSampler"; tmp += slot_name;
        tmp += "(vec2 uv) { float r = texture("; tmp += sampler_symbol;
        tmp += ", uv).r; return vec4(r,r,r,r); }\n";
    }
    else
    {
        tmp += "vec4 GetSampler"; tmp += slot_name;
        tmp += "(vec2 uv) { return texture("; tmp += sampler_symbol;
        tmp += ", uv); }\n";
    }
    writer.EmitLine(tmp);
}

} // namespace hgl::graph
