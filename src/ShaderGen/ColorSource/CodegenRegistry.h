#pragma once

/// CodegenRegistry.h
///
/// ColorSourceKind → IColorSourceCodegen 的路由表。
///
/// 职责：
///   - 注册每种 kind 的 codegen 实现（Register）
///   - 根据 kind 查找实现（Find）
///   - 未来可支持用户注册自定义 kind
///
/// Step 1 仅提供骨架，不注册任何具体实现（Step 2 再注册内置 sampler codegen）。

#include <hgl/shadergen/ColorSource.h>
#include <hgl/shadergen/IColorSourceCodegen.h>
#include <memory>
#include <unordered_map>

namespace hgl::graph
{

class ColorSourceCodegenRegistry
{
public:
    /// 注册 kind → codegen 实现（取所有权）
    void Register(ColorSourceKind kind, std::unique_ptr<IColorSourceCodegen> impl);

    /// 查找 kind 对应的实现；未注册返回 nullptr
    const IColorSourceCodegen* Find(ColorSourceKind kind) const;

    /// 全局单例（生产路径用；单元测试可使用独立实例）
    static ColorSourceCodegenRegistry& Global();

private:
    std::unordered_map<uint8_t, std::unique_ptr<IColorSourceCodegen>> impls_;
};

} // namespace hgl::graph
