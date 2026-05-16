#pragma once

/// BuiltinSamplerCodegen.h
///
/// IColorSourceCodegen 内置 sampler 实现类声明。
///
/// BuiltinSampler2DCodegen      — 对应 ColorSourceKind::BuiltinSampler2D
///   emit:  layout(set,binding) uniform sampler2D Sampler_Xxx;
///          vec4 GetSamplerXxx(uint /*mi_id*/, vec2 uv) { ... }   // mi_id 忽略，兼容统一调用形式
///
/// BuiltinSampler2DArrayCodegen — 对应 ColorSourceKind::BuiltinSampler2DArray
///   emit:  layout(set,binding) uniform sampler2DArray Sampler_Xxx;
///          vec4 GetSamplerXxx(uint mi_id, vec2 uv) { ... }       // mi_id 作为 layer 索引参数
///
/// 两者均使用统一的双参数签名 (uint mi_id, vec2 uv)，surface shader 调用点一律写
///   GetSamplerXxx(GetMaterialInstanceID(), uv)
/// 无需 ifdef 区分 2D vs 2DArray。
///
/// (set,binding) 来自 resolved_bindings，不由 emitter 自行决策。
/// 这是 Step 2 "内置 PCG 也声明 bindings[]" 目标的落地。

#include "ColorSource/CodegenRegistry.h"
#include <hgl/shadergen/IColorSourceCodegen.h>

namespace hgl::graph
{

// ── 公共辅助基类（不可直接实例化）────────────────────────────────────────────

class BuiltinSamplerCodegenBase : public IColorSourceCodegen
{
protected:
    /// 从 resolved_bindings 按 debug_name 查找 (set, binding)。
    /// 未找到时返回 false，并把 set/binding 置为 0。
    static bool FindResolved(const ResolvedBindings &resolved,
                             const std::string      &debug_name,
                             uint32_t               &out_set,
                             uint32_t               &out_binding);

public:
    // DeclareRequirements：把 src.bindings / src.push_constants 复制给 out
    void DeclareRequirements(const ColorSource                     &src,
                             std::vector<DescriptorRequirement>    &out_bindings,
                             std::vector<PushConstantRequirement>  &out_push) const override;

    // Validate：slot 合法 + bindings 非空
    ColorSourceValidationResult Validate(const ColorSource &src) const override;
};

// ── BuiltinSampler2DCodegen ───────────────────────────────────────────────────

class BuiltinSampler2DCodegen final : public BuiltinSamplerCodegenBase
{
public:
    ColorSourceKind GetKind() const override { return ColorSourceKind::BuiltinSampler2D; }

    void EmitDeclarations(ShaderWriter            &writer,
                          const ColorSource        &src,
                          const ResolvedBindings   &resolved_bindings) const override;

    void EmitGetterFunction(ShaderWriter            &writer,
                            const ColorSource        &src,
                            const ResolvedBindings   &resolved_bindings) const override;
};

// ── BuiltinSampler2DArrayCodegen ─────────────────────────────────────────────

class BuiltinSampler2DArrayCodegen final : public BuiltinSamplerCodegenBase
{
public:
    ColorSourceKind GetKind() const override { return ColorSourceKind::BuiltinSampler2DArray; }

    void EmitDeclarations(ShaderWriter            &writer,
                          const ColorSource        &src,
                          const ResolvedBindings   &resolved_bindings) const override;

    void EmitGroupAuxiliary(ShaderWriter                      &writer,
                            const std::vector<ColorSource>    &group,
                            const ResolvedBindings            &resolved_bindings) const override;

    void EmitGetterFunction(ShaderWriter            &writer,
                            const ColorSource        &src,
                            const ResolvedBindings   &resolved_bindings) const override;
};

// ── BuiltinBindlessSamplerArrayCodegen ───────────────────────────────────────
//
// Step 6 stub: VK_EXT_descriptor_indexing 尚未实现。
//
// DeclareRequirements: count=0（unbounded），标记未来 bindless descriptor 形状。
// EmitDeclarations:    emit TODO 注释，不产生实际 layout 声明。
// EmitGetterFunction:  emit 品红色 fallback getter (vec4(1,0,1,1))，使 shader 可编译
//                      且运行时出现品红色即可识别 bindless 路径未实现。
// Validate:            仅警告，不 fatal，避免阻塞已有 sample。
//
// 待 VK_EXT_descriptor_indexing capability 查询基础设施就绪后，替换为真实实现。

class BuiltinBindlessSamplerArrayCodegen final : public BuiltinSamplerCodegenBase
{
public:
    ColorSourceKind GetKind() const override
    {
        return ColorSourceKind::BuiltinBindlessSamplerArray;
    }

    void DeclareRequirements(const ColorSource                     &src,
                             std::vector<DescriptorRequirement>    &out_bindings,
                             std::vector<PushConstantRequirement>  &out_push) const override;

    void EmitDeclarations(ShaderWriter            &writer,
                          const ColorSource        &src,
                          const ResolvedBindings   &resolved_bindings) const override;

    void EmitGetterFunction(ShaderWriter            &writer,
                            const ColorSource        &src,
                            const ResolvedBindings   &resolved_bindings) const override;

    ColorSourceValidationResult Validate(const ColorSource &src) const override;
};

} // namespace hgl::graph
