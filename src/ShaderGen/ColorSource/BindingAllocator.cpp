#include "BindingAllocator.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>

namespace hgl::graph
{

// ─────────────────────────────────────────────────────────────────────────────
void BindingAllocator::AddRequirements(const std::vector<DescriptorRequirement> &reqs)
{
    assert(!allocated_ && "BindingAllocator::AddRequirements called after Allocate()");

    for (const auto &req : reqs)
    {
        const std::string &name = req.debug_name;

        if (name_to_index_.count(name))
        {
            // 同名 requirement 已存在：忽略重复（同一 ColorSource 可能被多次添加）
            // 如果类型不同则后续 Allocate() 的冲突检测会捕获
            continue;
        }

        const size_t idx = entries_.size();
        entries_.push_back(Entry{ req });
        if (!name.empty())
            name_to_index_[name] = idx;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
BindingAllocResult BindingAllocator::Allocate()
{
    assert(!allocated_ && "BindingAllocator::Allocate() called more than once");
    allocated_ = true;

    BindingAllocResult result;

    // ── 按 debug_name 字典序稳定排序索引，保证分配结果与加入顺序无关 ──────────
    std::vector<size_t> sorted_indices(entries_.size());
    for (size_t i = 0; i < entries_.size(); ++i) sorted_indices[i] = i;
    std::sort(sorted_indices.begin(), sorted_indices.end(),
              [this](size_t a, size_t b) {
                  return entries_[a].req.debug_name < entries_[b].req.debug_name;
              });

    // ── Phase 1：FixedSetAndBinding 先占位 ───────────────────────────────────
    // 记录已占用的 (set, binding) 对，用于冲突检测
    std::unordered_map<uint64_t, std::string> occupied; // key = (set<<32)|binding → debug_name

    auto occupy_key = [](uint32_t s, uint32_t b) -> uint64_t {
        return (uint64_t(s) << 32) | uint64_t(b);
    };

    for (size_t i : sorted_indices)
    {
        auto &e = entries_[i];
        if (e.req.binding_policy != BindingPolicy::FixedSetAndBinding)
            continue;

        const uint32_t s = e.req.fixed_set;
        const uint32_t b = e.req.fixed_binding;
        const uint64_t key = occupy_key(s, b);

        auto it = occupied.find(key);
        if (it != occupied.end())
        {
            std::ostringstream oss;
            oss << "[BindingAllocator] FATAL: (set=" << s << ", binding=" << b
                << ") collision between \"" << e.req.debug_name
                << "\" and \"" << it->second << "\"";
            result.diags.push_back({ BindingAllocDiag::Level::Error, oss.str() });
            result.ok = false;
        }
        else
        {
            occupied[key] = e.req.debug_name;
            e.resolved     = { s, b, e.req.debug_name };
            e.allocated    = true;
            // 推进 set 内 next_binding_ 越过已占用号，避免后续 Auto 碰到它
            auto &nb = next_binding_[s];
            if (nb <= b) nb = b + 1;
        }
    }

    // ── Phase 2：FixedSet — 在指定 set 内递增分配 ────────────────────────────
    for (size_t i : sorted_indices)
    {
        auto &e = entries_[i];
        if (e.req.binding_policy != BindingPolicy::FixedSet)
            continue;

        const uint32_t s = e.req.fixed_set;
        uint32_t &nb = next_binding_[s];

        // 跳过已被 FixedSetAndBinding 占用的 binding 号
        while (occupied.count(occupy_key(s, nb)))
            ++nb;

        const uint32_t b = nb++;
        occupied[occupy_key(s, b)] = e.req.debug_name;
        e.resolved  = { s, b, e.req.debug_name };
        e.allocated = true;
    }

    // ── Phase 3：Auto — 全部进 kDefaultMaterialBindingSet ────────────────────
    for (size_t i : sorted_indices)
    {
        auto &e = entries_[i];
        if (e.req.binding_policy != BindingPolicy::Auto)
            continue;

        const uint32_t s = kDefaultMaterialBindingSet;
        uint32_t &nb = next_binding_[s];

        while (occupied.count(occupy_key(s, nb)))
            ++nb;

        const uint32_t b = nb++;
        occupied[occupy_key(s, b)] = e.req.debug_name;
        e.resolved  = { s, b, e.req.debug_name };
        e.allocated = true;
    }

    // ── 输出（保持原始加入顺序） ─────────────────────────────────────────────
    result.bindings.resize(entries_.size());
    for (size_t i = 0; i < entries_.size(); ++i)
        result.bindings[i] = entries_[i].resolved;

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
const ResolvedBinding* BindingAllocator::Resolve(const std::string &debug_name) const
{
    auto it = name_to_index_.find(debug_name);
    if (it == name_to_index_.end())
        return nullptr;

    const Entry &e = entries_[it->second];
    if (!e.allocated)
        return nullptr;

    return &e.resolved;
}

// ─────────────────────────────────────────────────────────────────────────────
void BindingAllocator::Reset()
{
    entries_.clear();
    name_to_index_.clear();
    next_binding_.clear();
    allocated_ = false;
}

} // namespace hgl::graph
