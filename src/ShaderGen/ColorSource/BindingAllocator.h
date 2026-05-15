#pragma once

/// BindingAllocator.h
///
/// 统一 descriptor binding 分配器。
///
/// 职责：
///   - 收集所有 ColorSource 的 DescriptorRequirement[]
///   - 按策略分配 (set, binding) 号
///     * FixedSetAndBinding 优先占位，冲突即 fatal
///     * FixedSet：set 固定，binding 在该 set 内递增分配
///     * Auto：默认策略（MaterialBinding 资源进 set=3），按 debug_name 字典序稳定排序后顺序分配
///   - 输出 debug_name → ResolvedBinding 的映射供 codegen 使用
///
/// 稳定性保证：
///   相同的 DescriptorRequirement 集合（无论加入顺序）产生相同的 (set,binding) 结果，
///   确保多次 codegen 生成一致的 shader，缓存 SPIR-V / PipelineLayout 不会失效。

#include <hgl/shadergen/DescriptorRequirement.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace hgl::graph
{

/// 分配诊断条目（警告 / 错误）
struct BindingAllocDiag
{
    enum class Level { Warning, Error };
    Level       level;
    std::string message;
};

/// 分配结果
struct BindingAllocResult
{
    bool                         ok   = true;
    std::vector<ResolvedBinding> bindings;     ///< 与输入 requirements 一一对应（同序）
    std::vector<BindingAllocDiag> diags;       ///< 诊断列表；含 Error 时 ok=false
};

/// Auto 策略使用的默认 set 号（MaterialBinding 类资源）
inline constexpr uint32_t kDefaultMaterialBindingSet = 3;

class BindingAllocator
{
public:
    BindingAllocator() = default;

    /// 向分配器注册一批需求（来自单个 ColorSource）。
    /// 必须在 Allocate() 前调用；可多次调用（多个 ColorSource 逐个注册）。
    void AddRequirements(const std::vector<DescriptorRequirement> &reqs);

    /// 执行分配。调用后禁止再调用 AddRequirements。
    ///
    /// 分配顺序：
    ///   1. FixedSetAndBinding 先占位（按 debug_name 字典序稳定处理冲突检测）
    ///   2. FixedSet 在各自 set 内按 debug_name 字典序递增分配 binding
    ///   3. Auto 全部进 kDefaultMaterialBindingSet，按 debug_name 字典序递增分配 binding
    ///
    /// 分配结果可通过 Resolve(debug_name) 查询，也可通过返回值整体获取。
    BindingAllocResult Allocate();

    /// 根据 debug_name 查询已分配结果（Allocate() 后有效）。
    /// 未找到返回 nullptr。
    const ResolvedBinding* Resolve(const std::string &debug_name) const;

    /// 重置，允许复用同一实例
    void Reset();

private:
    struct Entry
    {
        DescriptorRequirement req;
        ResolvedBinding       resolved{};
        bool                  allocated = false;
    };

    std::vector<Entry>                              entries_;
    std::unordered_map<std::string, size_t>         name_to_index_;  ///< debug_name → entries_ 下标
    bool                                            allocated_ = false;

    // 追踪各 set 已用 binding 号（用于 Auto / FixedSet 递增分配）
    std::unordered_map<uint32_t, uint32_t>          next_binding_;   ///< set → 下一个空闲 binding
};

} // namespace hgl::graph
