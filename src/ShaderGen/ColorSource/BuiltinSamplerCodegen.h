#pragma once

/// BuiltinSamplerCodegen.h
///
/// IColorSourceCodegen 内置 sampler 实现类声明。
///
/// BuiltinSampler2DCodegen      — 对应 ColorSourceKind::BuiltinSampler2D
///   emit:  layout(set,binding) uniform sampler2D Sampler_Xxx;
///          vec4 GetSamplerXxx(vec2 uv) { ... }
///
/// BuiltinSampler2DArrayCodegen — 对应 ColorSourceKind::BuiltinSampler2DArray
///   emit:  layout(set,binding) uniform sampler2DArray Sampler_Xxx;
///          uint _tex_layer_Xxx;          // per-frame global (兼容旧路径，Step 5 再迁移)
///          vec4 GetSamplerXxx(vec2 uv) { ... }   // 从 _tex_layer_Xxx 取 layer
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

    void EmitGetterFunction(ShaderWriter            &writer,
                            const ColorSource        &src,
                            const ResolvedBindings   &resolved_bindings) const override;
};

} // namespace hgl::graph
