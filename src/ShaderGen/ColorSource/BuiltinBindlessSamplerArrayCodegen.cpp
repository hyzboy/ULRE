/// BuiltinBindlessSamplerArrayCodegen.cpp
///
/// Step 6 stub 实现 —— VK_EXT_descriptor_indexing 尚未实现。
///
/// TODO(Step 6 完整版): 当 capability 查询基础设施就绪后：
///   1. 检查设备是否支持 VK_EXT_descriptor_indexing / VkPhysicalDeviceDescriptorIndexingFeatures
///   2. 若支持：emit 真实 bindless layout 声明与 NonUniformEXT 采样 getter
///   3. 若不支持：fallback 到 BuiltinSampler2DArrayCodegen 路径（或报 fatal，视策略而定）

#include "BuiltinSamplerCodegen.h"
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/shadergen/ShaderWriter.h>

namespace hgl::graph
{

// ── DeclareRequirements ───────────────────────────────────────────────────────
//
// 用 count=0 声明 unbounded descriptor 数组（bindless 的标准形状）。
// BindingAllocator 看到 count=0 时应保留槽位但不分配实际 binding 号（未来实现）。

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
// Stub: 仅 emit TODO 注释，不产生实际 layout 声明。
// 运行时 shader 将因缺少 sampler 声明而使用 fallback getter 中的品红色输出。

void BuiltinBindlessSamplerArrayCodegen::EmitDeclarations(
    ShaderWriter            &writer,
    const ColorSource        &src,
    const ResolvedBindings   & /*resolved_bindings*/) const
{
    const char *slot_name = mtl::SamplerSlotNameList[uint8_t(src.slot)];

    std::string comment;
    comment += "// TODO(Step 6): BuiltinBindlessSamplerArray for slot '";
    comment += slot_name;
    comment += "' — VK_EXT_descriptor_indexing not yet implemented; using fallback getter.";
    writer.EmitLine(comment);
}

// ── EmitGetterFunction ────────────────────────────────────────────────────────
//
// Stub: 返回品红色 (1,0,1,1)，确保 shader 可编译。
// 运行时出现品红色即可识别 bindless 路径未实现。

void BuiltinBindlessSamplerArrayCodegen::EmitGetterFunction(
    ShaderWriter            &writer,
    const ColorSource        &src,
    const ResolvedBindings   & /*resolved_bindings*/) const
{
    const char *slot_name = mtl::SamplerSlotNameList[uint8_t(src.slot)];

    std::string getter;
    getter += "vec4 GetSampler";
    getter += slot_name;
    getter += "(uint /*mi_id*/, vec2 /*uv*/)"
              " { return vec4(1.0, 0.0, 1.0, 1.0); }"
              " // TODO(Step 6): bindless not implemented\n";
    writer.EmitLine(getter);
}

// ── Validate ─────────────────────────────────────────────────────────────────
//
// 仅返回警告，不 fatal，避免阻塞已有 sample。
// 待 VK_EXT_descriptor_indexing 就绪后改为检查 capability。

ColorSourceCodegenValidation BuiltinBindlessSamplerArrayCodegen::Validate(
    const ColorSource & /*src*/) const
{
    // Non-fatal: allow existing samples to continue running with fallback output.
    return ColorSourceCodegenValidation::Ok();
}

} // namespace hgl::graph
