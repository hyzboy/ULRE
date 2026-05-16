/// BuiltinBindlessSamplerArrayCodegen.cpp
///
/// C7 完整实现 —— VK_EXT_descriptor_indexing (GL_EXT_nonuniform_qualifier)。
///
/// EmitDeclarations:
///   #extension GL_EXT_nonuniform_qualifier : enable
///   layout(set=S, binding=B) uniform sampler2D Sampler_<Slot>_Bindless[];
///
/// EmitGetterFunction:
///   vec4 GetSampler<Slot>(uint idx, vec2 uv) {
///       return texture(Sampler_<Slot>_Bindless[nonuniformEXT(idx)], uv);
///   }
///
/// FallbackToArray: FinalizeColorSources 在 supports_descriptor_indexing=false 时
/// 将 kind 重写为 BuiltinSampler2DArray 并 warn；不需要此文件做任何处理。

#include "BuiltinSamplerCodegen.h"
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/shadergen/ShaderWriter.h>

namespace hgl::graph
{

// ── DeclareRequirements ───────────────────────────────────────────────────────
//
// count=0 = unbounded array（VK_EXT_descriptor_indexing 语义）。
// BindingAllocator 对 count=0 的项目照常分配一个 binding 槽位；
// 实际描述符集合创建时由 Vulkan 层负责声明为 VARIABLE_COUNT。

void BuiltinBindlessSamplerArrayCodegen::DeclareRequirements(
    const ColorSource                     &src,
    std::vector<DescriptorRequirement>    &out_bindings,
    std::vector<PushConstantRequirement>  & /*out_push*/) const
{
    const char *slot_name = mtl::SamplerSlotNameList[uint8_t(src.slot)];

    DescriptorRequirement req;
    req.type           = DescriptorType::TextureSampler;
    req.count          = 0;                         // 0 = unbounded（bindless）
    req.stages         = ShaderStage::Fragment;
    req.binding_policy = BindingPolicy::Auto;
    req.debug_name     = std::string("Sampler_") + slot_name + "_Bindless";

    out_bindings.push_back(std::move(req));
}

// ── EmitDeclarations ──────────────────────────────────────────────────────────
//
// 发出 GL_EXT_nonuniform_qualifier extension 启用指令和 unbounded sampler2D 数组声明。
// (set, binding) 来自 resolved_bindings，保证与 BindingAllocator 分配结果一致。

void BuiltinBindlessSamplerArrayCodegen::EmitDeclarations(
    ShaderWriter            &writer,
    const ColorSource        &src,
    const ResolvedBindings   &resolved_bindings) const
{
    const char *slot_name = mtl::SamplerSlotNameList[uint8_t(src.slot)];
    const std::string debug_name = std::string("Sampler_") + slot_name + "_Bindless";

    uint32_t set = 0, binding = 0;
    FindResolved(resolved_bindings, debug_name, set, binding);

    // Extension guard — may already be emitted by another bindless slot; the guard prevents duplication.
    writer.EmitLine("#ifndef ULRE_EXT_NONUNIFORM_QUALIFIER_ENABLED");
    writer.EmitLine("#extension GL_EXT_nonuniform_qualifier : enable");
    writer.EmitLine("#define ULRE_EXT_NONUNIFORM_QUALIFIER_ENABLED");
    writer.EmitLine("#endif");

    // Unbounded array: [] after type name (requires VK_EXT_descriptor_indexing on device).
    std::string decl = "layout(set=";
    decl += std::to_string(set);
    decl += ", binding=";
    decl += std::to_string(binding);
    decl += ") uniform sampler2D ";
    decl += debug_name;
    decl += "[];";
    writer.EmitLine(decl);
}

// ── EmitGetterFunction ────────────────────────────────────────────────────────
//
// nonuniformEXT(idx) は VK_EXT_descriptor_indexing が要求する non-uniform index ラッパー。
// Grayscale_R フォーマットにも対応。

void BuiltinBindlessSamplerArrayCodegen::EmitGetterFunction(
    ShaderWriter            &writer,
    const ColorSource        &src,
    const ResolvedBindings   & /*resolved_bindings*/) const
{
    const char *slot_name  = mtl::SamplerSlotNameList[uint8_t(src.slot)];
    const std::string array_name = std::string("Sampler_") + slot_name + "_Bindless";

    std::string tmp;
    if (src.builtin.output_format == ColorSourceOutputFormat::Grayscale_R)
    {
        tmp += "vec4 GetSampler"; tmp += slot_name;
        tmp += "(uint idx, vec2 uv) { float r = texture(";
        tmp += array_name; tmp += "[nonuniformEXT(idx)], uv).r; return vec4(r,r,r,r); }\n";
    }
    else
    {
        tmp += "vec4 GetSampler"; tmp += slot_name;
        tmp += "(uint idx, vec2 uv) { return texture(";
        tmp += array_name; tmp += "[nonuniformEXT(idx)], uv); }\n";
    }
    writer.EmitLine(tmp);
}

// ── Validate ─────────────────────────────────────────────────────────────────
//
// slot 合法性チェック。bindings[] は DeclareRequirements で動的生成するため空でも Ok。

ColorSourceCodegenValidation BuiltinBindlessSamplerArrayCodegen::Validate(
    const ColorSource &src) const
{
    if (size_t(src.slot) >= mtl::SamplerSlotCount)
        return ColorSourceCodegenValidation::Fail("invalid SamplerSlot value for BuiltinBindlessSamplerArray");
    return ColorSourceCodegenValidation::Ok();
}

} // namespace hgl::graph
