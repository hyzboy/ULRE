#pragma once

/// IColorSourceCodegen.h
///
/// ColorSource 代码生成器抽象接口。
///
/// 每种 ColorSourceKind 对应一个 IColorSourceCodegen 实现，负责：
///   1. 声明该实现需要什么 descriptor / push constant（DeclareRequirements）
///   2. emit 顶层资源声明（uniform sampler2D ... / #include user.glsl / ...）
///   3. emit GetSamplerXxx(...) 函数体
///   4. 自我校验（签名合法性、文件存在性等）
///
/// 生成器主流程（CompositorAssembler 等）只与此接口交互，不感知 kind 细节。

#include <hgl/shadergen/ColorSource.h>
#include <hgl/shadergen/DescriptorRequirement.h>
#include <hgl/shadergen/ShaderWriter.h>
#include <string>
#include <vector>

namespace hgl::graph
{

// ── 单条目校验结果（per-source Validate() 返回值）──────────────────────────────

struct ColorSourceCodegenValidation
{
    bool        ok      = true;
    std::string message;        ///< 仅 !ok 时有效

    static ColorSourceCodegenValidation Ok() { return {}; }
    static ColorSourceCodegenValidation Fail(std::string msg)
    {
        return { false, std::move(msg) };
    }
};

// ── 已分配的 binding 映射（BindingAllocator 输出）────────────────────────────

/// DescriptorRequirement::debug_name → ResolvedBinding 的映射。
/// 由 BindingAllocator 填充，传入 EmitDeclarations / EmitGetterFunction。
using ResolvedBindings = std::vector<ResolvedBinding>;

// ── IColorSourceCodegen 接口 ──────────────────────────────────────────────────

class IColorSourceCodegen
{
public:
    virtual ~IColorSourceCodegen() = default;

    /// 返回此实现支持的 ColorSourceKind（用于 CodegenRegistry 路由）
    virtual ColorSourceKind GetKind() const = 0;

    /// 声明该实现产生的 descriptor / push constant 需求。
    /// 生成器在 RecipeFinalize 阶段调用，结果汇入 BindingAllocator。
    ///
    /// 注意：内置 sampler PCG 也必须在此处声明 bindings，不允许在
    /// EmitDeclarations 里自己决定 set/binding 号。
    virtual void DeclareRequirements(const ColorSource &src,
                                     std::vector<DescriptorRequirement>  &out_bindings,
                                     std::vector<PushConstantRequirement> &out_push) const = 0;

    /// Emit 顶层声明（layout(set,binding) uniform sampler2D ... 或 #include "user.glsl" 等）。
    /// 在 shader 文件开头调用一次；(set,binding) 从 resolved_bindings 查 debug_name 获取。
    virtual void EmitDeclarations(ShaderWriter            &writer,
                                  const ColorSource        &src,
                                  const ResolvedBindings   &resolved_bindings) const = 0;

    /// Emit GetSamplerXxx(...) 函数定义。
    /// 函数签名必须与 src.signature 匹配。
    virtual void EmitGetterFunction(ShaderWriter            &writer,
                                    const ColorSource        &src,
                                    const ResolvedBindings   &resolved_bindings) const = 0;

    /// Emit 跨全组 sources 共享的辅助代码（例如 sampler2DArray 组的 MIT SSBO + GetMITLayer_* 函数）。
    /// 在同一 kind 的所有 EmitDeclarations 调用之后、所有 EmitGetterFunction 调用之前调用一次。
    /// 默认实现为空，仅需 group-level 辅助代码的 codegen 才需重写。
    virtual void EmitGroupAuxiliary(ShaderWriter                      &writer,
                                    const std::vector<ColorSource>    &group,
                                    const ResolvedBindings            &resolved_bindings) const
    {
        (void)writer; (void)group; (void)resolved_bindings;
    }

    /// 校验此 ColorSource 的配置是否合法（路径存在、签名兼容等）。
    /// 对应计划文档中的 G1 闸门（RecipeFinalize 时调用）。
    virtual ColorSourceCodegenValidation Validate(const ColorSource &src) const = 0;
};

} // namespace hgl::graph
