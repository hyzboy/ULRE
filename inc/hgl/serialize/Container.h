#pragma once

/**
 * HGL 容器 → 序列化统一适配层。
 *
 * 上层 `Serialize` 只写 `ar.Field(id, "nodes", v.nodes)`，不区分容器的物理实现；
 * 这里把每种 HGL 容器归一成四类**抽象形态**，后端再决定各自的落地表示：
 *
 * | 抽象形态 | HGL 容器 | 文本后端（TOML/JSON） | 二进制后端（FlatBuffers） |
 * |---|---|---|---|
 * | Sequence | `ValueArray` / `PtrArray` / `ManagedArray` | 数组 | `vector` |
 * | Mapping  | `OrderedMap` / `UnorderedMap` | 字符串键 → 原生表；其它键 → `{key,value}` 条目数组 | 按 key 排序的表向量 |
 * | Set      | `OrderedSet` / `FlatOrderedSet` | 数组 | `vector` |
 * | Tree     | `FlatTreeContainer` | 节点数组 + `parent` 索引 | 节点向量 + `parent` 向量 |
 *
 * ── 为什么「降级」放在这一层 ──────────────────────────────────────────────
 * 文本格式天然只有「表 / 数组 / 标量」三种形状，二进制格式额外能表达
 * `sorted vector`。把这些差异收在适配层，业务结构声明处就永远不需要写
 * `#ifdef`，也不会因为换后端而改资产结构。
 */

#include <hgl/type/ValueArray.h>
#include <hgl/type/PtrArray.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/OrderedMap.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/type/OrderedSet.h>
#include <hgl/type/FlatOrderedSet.h>
#include <hgl/type/FlatTreeContainer.h>

#include <cstdint>
#include <type_traits>

namespace hgl::serialize
{
    /// 容器的抽象形态。
    enum class ContainerKind : uint8_t
    {
        None = 0,
        Sequence,
        Mapping,
        Set,
        Tree,
    };

    template <typename C>
    struct ContainerTraits
    {
        static constexpr ContainerKind kind = ContainerKind::None;
    };

    // ── Sequence：ValueArray / PtrArray / ManagedArray ──────────────────────

    /// 三种序列容器共同的访问约定。
    template <typename C>
    struct SequenceAdapter;

    template <typename T>
    struct SequenceAdapter<ValueArray<T>>
    {
        using value_type = T;

        static int Count(const ValueArray<T> &c) { return c.GetCount(); }
        static void Clear(ValueArray<T> &c) { c.Clear(); }
        static T *AddDefault(ValueArray<T> &c) { return c.Add(); }
        static T *At(ValueArray<T> &c, const int i) { return c.At(i); }
        static const T *At(const ValueArray<T> &c, const int i) { return c.At(i); }
    };

    template <typename T>
    struct SequenceAdapter<PtrArray<T>>
    {
        using value_type = T;

        static int Count(const PtrArray<T> &c) { return c.GetCount(); }
        static void Clear(PtrArray<T> &c) { c.Clear(); }
        static T *AddDefault(PtrArray<T> &c) { return c.Add(); }
        static T *At(PtrArray<T> &c, const int i) { return c.At(i); }
        static const T *At(const PtrArray<T> &c, const int i) { return c.At(i); }
    };

    template <typename T>
    struct SequenceAdapter<ManagedArray<T>>
    {
        using value_type = T;

        static int Count(const ManagedArray<T> &c) { return c.GetCount(); }
        static void Clear(ManagedArray<T> &c) { c.Clear(); }
        static T *AddDefault(ManagedArray<T> &c) { return c.Create(); }
        static T *At(ManagedArray<T> &c, const int i) { return c.At(i); }
        static const T *At(const ManagedArray<T> &c, const int i) { return c.At(i); }
    };

    template <typename T>
    struct ContainerTraits<ValueArray<T>>
    {
        static constexpr ContainerKind kind = ContainerKind::Sequence;
        using adapter = SequenceAdapter<ValueArray<T>>;
    };

    template <typename T>
    struct ContainerTraits<PtrArray<T>>
    {
        static constexpr ContainerKind kind = ContainerKind::Sequence;
        using adapter = SequenceAdapter<PtrArray<T>>;
    };

    template <typename T>
    struct ContainerTraits<ManagedArray<T>>
    {
        static constexpr ContainerKind kind = ContainerKind::Sequence;
        using adapter = SequenceAdapter<ManagedArray<T>>;
    };

    // ── Mapping：OrderedMap / UnorderedMap ──────────────────────────────────

    /**
     * 映射容器统一走 `EnumKV`（有序/无序实现都有）与 `Add`。
     * 这样不必为每种实现分别列举 `GetKeyAt` / `GetValuePointerAt` 等各异的下标接口。
     */
    template <typename C>
    struct MappingAdapter
    {
        template <typename F>
        static void ForEach(const C &c, F &&fn) { c.EnumKV(fn); }
    };

    template <typename K, typename V>
    struct ContainerTraits<OrderedMap<K, V>>
    {
        static constexpr ContainerKind kind = ContainerKind::Mapping;
        using key_type = K;
        using mapped_type = V;

        static int Count(const OrderedMap<K, V> &c) { return c.GetCount(); }
        static void Clear(OrderedMap<K, V> &c) { c.Clear(); }
        static void Add(OrderedMap<K, V> &c, const K &k, const V &v) { c.Add(k, v); }

        template <typename F>
        static void ForEach(const OrderedMap<K, V> &c, F &&fn) { c.EnumKV(fn); }
    };

    template <typename K, typename V, typename H, typename E>
    struct ContainerTraits<UnorderedMap<K, V, H, E>>
    {
        static constexpr ContainerKind kind = ContainerKind::Mapping;
        using key_type = K;
        using mapped_type = V;

        static int Count(const UnorderedMap<K, V, H, E> &c) { return c.GetCount(); }
        static void Clear(UnorderedMap<K, V, H, E> &c) { c.Clear(); }
        static void Add(UnorderedMap<K, V, H, E> &c, const K &k, const V &v) { c.Add(k, v); }

        template <typename F>
        static void ForEach(const UnorderedMap<K, V, H, E> &c, F &&fn) { c.EnumKV(fn); }
    };

    // ── Set：OrderedSet / FlatOrderedSet ────────────────────────────────────

    template <typename T>
    struct ContainerTraits<OrderedSet<T>>
    {
        static constexpr ContainerKind kind = ContainerKind::Set;
        using value_type = T;

        static int Count(const OrderedSet<T> &c) { return static_cast<int>(c.GetCount()); }
        static void Clear(OrderedSet<T> &c) { c.Clear(); }
        static void Add(OrderedSet<T> &c, const T &v) { c.Add(v); }
        static bool Get(const OrderedSet<T> &c, const int i, T &out)
        {
            return c.Get(static_cast<int64>(i), out);
        }
    };

    template <typename T>
    struct ContainerTraits<FlatOrderedSet<T>>
    {
        static constexpr ContainerKind kind = ContainerKind::Set;
        using value_type = T;

        static int Count(const FlatOrderedSet<T> &c) { return static_cast<int>(c.GetCount()); }
        static void Clear(FlatOrderedSet<T> &c) { c.Clear(); }
        static void Add(FlatOrderedSet<T> &c, const T &v) { c.Add(v); }
        static bool Get(const FlatOrderedSet<T> &c, const int i, T &out)
        {
            return c.Get(static_cast<int64>(i), out);
        }
    };

    // ── Tree：FlatTreeContainer ─────────────────────────────────────────────

    /**
     * 双拓扑树降级为「扁平节点数组 + parent 索引」：
     *   · 写：遍历数据数组，逐节点记录 `parent`（`INVALID_INDEX` 表示根）；
     *   · 读：按数组中出现的顺序 `CreateNode(value, parent)` 重建层级拓扑，
     *         线性拓扑由容器内部按 `m_linear_dirty` 惰性重算。
     * 该表示同时兼容文本与二进制后端，且无需递归。
     */
    template <typename T>
    struct ContainerTraits<FlatTreeContainer<T>>
    {
        static constexpr ContainerKind kind = ContainerKind::Tree;
        using value_type = T;

        static int Count(const FlatTreeContainer<T> &c) { return static_cast<int>(c.GetCount()); }

        static void Clear(FlatTreeContainer<T> &c) { c.Clear(); }

        static const T *At(const FlatTreeContainer<T> &c, const int i) { return &c.GetData()[i]; }

        static int32 ParentAt(const FlatTreeContainer<T> &c, const int i)
        {
            const auto *topology = c.GetHierarchyTopologyData();
            return topology ? topology[i].parent : FlatTreeContainer<T>::INVALID_INDEX;
        }

        static int32 CreateNode(FlatTreeContainer<T> &c, const T &v, const int32 parent)
        {
            return c.CreateNode(v, parent);
        }
    };
}//namespace hgl::serialize
