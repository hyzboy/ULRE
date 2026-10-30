#pragma once

/**
 * hgl::serialize::Archive —— 统一序列化游标
 *
 * 一次 `Serialize()` 遍历即可同时服务「读」与「写」两个方向，以及任意数量的
 * 格式后端。生成代码只认识这一个类型：
 *
 * ```cpp
 * template<> struct StructSerializer<SceneSpec>
 * {
 *     static constexpr bool defined = true;
 *     template<typename Self>
 *     static void Do(Archive &ar, Self &v)
 *     {
 *         ar.Field(1, "schema",    v.schema);
 *         ar.Field(2, "name",      v.name);
 *         ar.Field(3, "camera",    v.camera);      // 嵌套结构：自动递归
 *         ar.Field(7, "materials", v.materials);   // ManagedArray：按容器形态处理
 *     }
 * };
 * ```
 *
 * ── 为什么 `Do` 是模板（`Self` 而不是 `T &`）────────────────────────────────
 * `Self` 会被推导成 `T`（读）或 `const T`（写）。这样同一份遍历函数既能写回
 * 又能只读输出，不需要为「const 版本」再复制一遍字段列表。
 *
 * ── 编解码语义（与既有 TOML / FlatBuffers 契约逐条对齐）────────────────────
 *   · **读**：字段缺失 ⇒ 保留 C++ 类内初始值（不报错）；字段存在但类型/长度
 *     不符 ⇒ 记一次失败，`Ok()` 变 false，并留下字段名；
 *   · **写**：写出全部字段（含等于缺省值的字段），保证 FlatBuffers 是完整快照。
 */

#include <hgl/serialize/Value.h>
#include <hgl/serialize/Traits.h>
#include <hgl/serialize/Container.h>

#include <cstdint>
#include <type_traits>

namespace hgl::serialize
{
    enum class ArchiveMode : uint8_t
    {
        Read = 0,
        Write,
    };

    class Archive
    {
    public:
        Archive() = default;

        /// 写模式：以 `root` 为输出表。
        static Archive ForWrite(Value &root)
        {
            Archive ar;
            ar.write_ = &root;
            root.SetTable();
            return ar;
        }

        /// 读模式：以 `root` 为输入表。
        static Archive ForRead(const Value &root)
        {
            Archive ar;
            ar.read_ = &root;
            return ar;
        }

        bool IsWriting() const { return write_ != nullptr; }
        bool IsReading() const { return read_ != nullptr; }

        ArchiveMode Mode() const
        {
            return write_ ? ArchiveMode::Write : ArchiveMode::Read;
        }

        bool Ok() const { return ok_; }
        void MarkFailed() { ok_ = false; }

        /// 首个失败的字段名与原因；无失败时返回空串。
        const char *ErrorMessage() const
        {
            return error_.c_str() ? error_.c_str() : "";
        }

        const char *FailedField() const
        {
            return failed_field_.c_str() ? failed_field_.c_str() : "";
        }

        /// 清空失败状态（用于复用同一个 Archive 处理多份输入）。
        void ResetStatus()
        {
            ok_ = true;
            error_.Clear();
            failed_field_.Clear();
        }

        // ── 字段入口 ────────────────────────────────────────────────────────

        /**
         * 一个字段。`T` 可以是 const 限定（写方向），内部按
         * `std::remove_cv_t<T>` 查 traits。
         *
         * 读方向需要写回，因此在一个**受控位置**去掉 const：调用方永远只在写
         * 方向传入 const 引用，而写方向本身不会修改任何值。
         */
        template <typename T>
        void Field(const uint32_t id, const char *name, T &value)
        {
            if (!ok_)
                return;

            using U = std::remove_cv_t<T>;

            if (write_)
            {
                Value *node = write_->AddField(name, id);
                if (!node)
                {
                    Fail(name, "无法创建字段节点");
                    return;
                }
                WriteValue(*node, static_cast<const U &>(value), name);
                return;
            }

            if (read_)
            {
                const Value *node = read_->FindField(name, id);
                // 缺键不是错误：保留 C++ 类内缺省值（与既有 TOML 语义一致）。
                if (!node)
                    return;

                U &target = const_cast<U &>(value);
                if (!ReadValue(*node, target))
                    Fail(name, "字段类型或取值不合法");
            }
        }

        /// 手动上报一次失败（供业务层补充跨字段校验）。
        void Fail(const char *field, const char *reason)
        {
            if (!ok_)
                return;
            ok_ = false;
            failed_field_ = AnsiString(field ? field : "");
            error_ = AnsiString(reason ? reason : "");
        }

    private:
        // ── 值 → 树（写方向，只读源对象）────────────────────────────────────

        template <typename T>
        void WriteValue(Value &node, const T &value, const char *where)
        {
            using U = std::remove_cv_t<T>;

            if constexpr (IsStringType<U>::value)
            {
                StoreString(node, value);
            }
            else if constexpr (std::is_same_v<U, bool>)
            {
                node.SetBool(value);
            }
            else if constexpr (std::is_enum_v<U>)
            {
                StoreEnum(node, value);
            }
            else if constexpr (std::is_integral_v<U>)
            {
                if constexpr (std::is_signed_v<U>)
                    node.SetInt(static_cast<int64_t>(value));
                else
                    node.SetUInt(static_cast<uint64_t>(value));
            }
            else if constexpr (std::is_floating_point_v<U>)
            {
                if constexpr (std::is_same_v<U, float>)
                    node.SetFloat(value);
                else
                    node.SetDouble(static_cast<double>(value));
            }
            else if constexpr (VectorTraits<U>::components > 0)
            {
                StoreVector(node, value);
            }
            else if constexpr (ContainerTraits<U>::kind == ContainerKind::Sequence)
            {
                WriteSequence(node, value, where);
            }
            else if constexpr (ContainerTraits<U>::kind == ContainerKind::Mapping)
            {
                WriteMapping(node, value, where);
            }
            else if constexpr (ContainerTraits<U>::kind == ContainerKind::Set)
            {
                WriteSet(node, value, where);
            }
            else if constexpr (ContainerTraits<U>::kind == ContainerKind::Tree)
            {
                WriteTree(node, value, where);
            }
            else if constexpr (StructSerializer<U>::defined)
            {
                node.SetTable();
                Archive sub;
                sub.write_ = &node;
                StructSerializer<U>::Do(sub, value);
                if (!sub.ok_)
                    Fail(where, sub.error_.c_str());
            }
            else
            {
                static_assert(StructSerializer<U>::defined,
                              "该类型没有序列化支持：请为它标注 HGL_ASSET_STRUCT 并重新运行 AssetGen");
            }
        }

        // ── 树 → 值（读方向，写回目标对象）──────────────────────────────────

        template <typename T>
        bool ReadValue(const Value &node, T &out)
        {
            using U = std::remove_cv_t<T>;

            if constexpr (IsStringType<U>::value)
            {
                return LoadString(node, out);
            }
            else if constexpr (std::is_same_v<U, bool>)
            {
                return node.GetBool(out);
            }
            else if constexpr (std::is_enum_v<U>)
            {
                return LoadEnum(node, out);
            }
            else if constexpr (std::is_integral_v<U>)
            {
                if constexpr (std::is_signed_v<U>)
                {
                    int64_t raw = 0;
                    if (!node.GetInt(raw))
                        return false;
                    // 窄化前先做范围检查，避免静默截断。
                    if (sizeof(U) < sizeof(int64_t))
                    {
                        const int bits = static_cast<int>(sizeof(U) * 8);
                        const int64_t min_v = -(int64_t(1) << (bits - 1));
                        const int64_t max_v = (int64_t(1) << (bits - 1)) - 1;
                        if (raw < min_v || raw > max_v)
                            return false;
                    }
                    out = static_cast<U>(raw);
                    return true;
                }
                else
                {
                    uint64_t raw = 0;
                    if (!node.GetUInt(raw))
                        return false;
                    if (sizeof(U) < sizeof(uint64_t))
                    {
                        const uint64_t max_v = (uint64_t(1) << (sizeof(U) * 8)) - 1;
                        if (raw > max_v)
                            return false;
                    }
                    out = static_cast<U>(raw);
                    return true;
                }
            }
            else if constexpr (std::is_floating_point_v<U>)
            {
                double raw = 0.0;
                if (!node.GetDouble(raw))
                    return false;
                out = static_cast<U>(raw);
                return true;
            }
            else if constexpr (VectorTraits<U>::components > 0)
            {
                return LoadVector(node, out);
            }
            else if constexpr (ContainerTraits<U>::kind == ContainerKind::Sequence)
            {
                return ReadSequence(node, out);
            }
            else if constexpr (ContainerTraits<U>::kind == ContainerKind::Mapping)
            {
                return ReadMapping(node, out);
            }
            else if constexpr (ContainerTraits<U>::kind == ContainerKind::Set)
            {
                return ReadSet(node, out);
            }
            else if constexpr (ContainerTraits<U>::kind == ContainerKind::Tree)
            {
                return ReadTree(node, out);
            }
            else if constexpr (StructSerializer<U>::defined)
            {
                if (!node.IsTable())
                    return false;
                Archive sub;
                sub.read_ = &node;
                StructSerializer<U>::Do(sub, out);
                if (!sub.ok_)
                {
                    Fail(sub.failed_field_.c_str(), sub.error_.c_str());
                    return false;
                }
                return true;
            }
            else
            {
                static_assert(StructSerializer<U>::defined,
                              "该类型没有序列化支持：请为它标注 HGL_ASSET_STRUCT 并重新运行 AssetGen");
                return false;
            }
        }

        // ── Sequence ────────────────────────────────────────────────────────

        template <typename C>
        void WriteSequence(Value &node, const C &container, const char *where)
        {
            using Adapter = typename ContainerTraits<C>::adapter;

            node.SetArray();
            const int count = Adapter::Count(container);
            for (int i = 0; i < count; ++i)
            {
                Value *slot = node.AddElement();
                if (!slot)
                {
                    Fail(where, "数组节点分配失败");
                    return;
                }
                const auto *element = Adapter::At(container, i);
                if (!element)
                    continue;
                WriteValue(*slot, *element, where);
            }
        }

        template <typename C>
        bool ReadSequence(const Value &node, C &container)
        {
            using Adapter = typename ContainerTraits<C>::adapter;

            if (!node.IsArray())
                return false;

            Adapter::Clear(container);
            const int count = node.GetElementCount();
            for (int i = 0; i < count; ++i)
            {
                const Value *slot = node.GetElement(i);
                auto *element = Adapter::AddDefault(container);
                if (!slot || !element)
                    return false;
                if (!ReadValue(*slot, *element))
                    return false;
            }
            return true;
        }

        // ── Mapping ─────────────────────────────────────────────────────────

        /// 字符串键可以无损映射为文本表；其它键统一降级为 `{key, value}` 条目数组。
        template <typename K>
        static constexpr bool IsStringKey()
        {
            return IsStringType<std::remove_cv_t<K>>::value;
        }

        template <typename C>
        void WriteMapping(Value &node, const C &container, const char *where)
        {
            using Traits = ContainerTraits<C>;
            using K = typename Traits::key_type;
            using V = typename Traits::mapped_type;

            if constexpr (IsStringKey<K>())
            {
                node.SetTable();
                Traits::ForEach(container,
                                [&](const K &key, const V &value)
                                {
                                    const U8String key_u8 = StringToU8(key);
                                    const char *key_text =
                                        reinterpret_cast<const char *>(key_u8.c_str());

                                    Value *slot = node.AddField(key_text, 0);
                                    if (!slot)
                                    {
                                        Fail(where, "映射字段节点分配失败");
                                        return;
                                    }
                                    WriteValue(*slot, value, where);
                                });
            }
            else
            {
                node.SetArray();
                Traits::ForEach(container,
                                [&](const K &key, const V &value)
                                {
                                    Value *entry = node.AddElement();
                                    if (!entry)
                                    {
                                        Fail(where, "映射条目分配失败");
                                        return;
                                    }
                                    entry->SetTable();

                                    Value *key_slot = entry->AddField("key", 0);
                                    Value *val_slot = entry->AddField("value", 0);
                                    if (key_slot)
                                        WriteValue(*key_slot, key, where);
                                    if (val_slot)
                                        WriteValue(*val_slot, value, where);
                                });
            }
        }

        template <typename C>
        bool ReadMapping(const Value &node, C &container)
        {
            using Traits = ContainerTraits<C>;
            using K = typename Traits::key_type;
            using V = typename Traits::mapped_type;

            Traits::Clear(container);

            if constexpr (IsStringKey<K>())
            {
                if (!node.IsTable())
                    return false;

                const int count = node.GetFieldCount();
                for (int i = 0; i < count; ++i)
                {
                    const char *name = node.GetFieldName(i);
                    const Value *slot = node.GetFieldValue(i);
                    if (!name || !slot)
                        continue;

                    K key{};
                    Value key_node;
                    key_node.SetString(name);
                    if (!LoadString(key_node, key))
                        return false;

                    V value{};
                    if (!ReadValue(*slot, value))
                        return false;
                    Traits::Add(container, key, value);
                }
                return true;
            }
            else
            {
                if (!node.IsArray())
                    return false;

                const int count = node.GetElementCount();
                for (int i = 0; i < count; ++i)
                {
                    const Value *entry = node.GetElement(i);
                    if (!entry || !entry->IsTable())
                        return false;

                    const Value *key_slot = entry->FindField("key");
                    const Value *val_slot = entry->FindField("value");
                    if (!key_slot || !val_slot)
                        return false;

                    K key{};
                    V value{};
                    if (!ReadValue(*key_slot, key))
                        return false;
                    if (!ReadValue(*val_slot, value))
                        return false;
                    Traits::Add(container, key, value);
                }
                return true;
            }
        }

        // ── Set ─────────────────────────────────────────────────────────────

        template <typename C>
        void WriteSet(Value &node, const C &container, const char *where)
        {
            using Traits = ContainerTraits<C>;
            using T = typename Traits::value_type;

            node.SetArray();
            const int count = Traits::Count(container);
            for (int i = 0; i < count; ++i)
            {
                T value{};
                if (!Traits::Get(container, i, value))
                    continue;
                Value *slot = node.AddElement();
                if (!slot)
                {
                    Fail(where, "集合节点分配失败");
                    return;
                }
                WriteValue(*slot, value, where);
            }
        }

        template <typename C>
        bool ReadSet(const Value &node, C &container)
        {
            using Traits = ContainerTraits<C>;
            using T = typename Traits::value_type;

            if (!node.IsArray())
                return false;

            Traits::Clear(container);
            const int count = node.GetElementCount();
            for (int i = 0; i < count; ++i)
            {
                const Value *slot = node.GetElement(i);
                if (!slot)
                    return false;

                T value{};
                if (!ReadValue(*slot, value))
                    return false;
                Traits::Add(container, value);
            }
            return true;
        }

        // ── Tree ────────────────────────────────────────────────────────────

        /**
         * 双拓扑树降级为「扁平节点数组 + parent 索引」：
         * 文本后端下每个元素是 `{ parent = -1, data = { ... } }`；
         * 读回时按数组顺序 `CreateNode(value, parent)` 重建层级拓扑。
         */
        template <typename C>
        void WriteTree(Value &node, const C &container, const char *where)
        {
            using Traits = ContainerTraits<C>;

            node.SetArray();
            const int count = Traits::Count(container);
            for (int i = 0; i < count; ++i)
            {
                Value *entry = node.AddElement();
                if (!entry)
                {
                    Fail(where, "树节点分配失败");
                    return;
                }
                entry->SetTable();

                Value *parent_slot = entry->AddField("parent", 0);
                Value *data_slot = entry->AddField("data", 0);
                if (parent_slot)
                    parent_slot->SetInt(static_cast<int64_t>(Traits::ParentAt(container, i)));
                if (data_slot)
                    WriteValue(*data_slot, *Traits::At(container, i), where);
            }
        }

        template <typename C>
        bool ReadTree(const Value &node, C &container)
        {
            using Traits = ContainerTraits<C>;
            using T = typename Traits::value_type;

            if (!node.IsArray())
                return false;

            Traits::Clear(container);
            const int count = node.GetElementCount();
            for (int i = 0; i < count; ++i)
            {
                const Value *entry = node.GetElement(i);
                if (!entry || !entry->IsTable())
                    return false;

                const Value *parent_slot = entry->FindField("parent");
                const Value *data_slot = entry->FindField("data");
                if (!data_slot)
                    return false;

                int64_t parent = -1;
                if (parent_slot && !parent_slot->GetInt(parent))
                    return false;

                T value{};
                if (!ReadValue(*data_slot, value))
                    return false;

                const int32_t index = Traits::CreateNode(
                    container, value, static_cast<int32_t>(parent));
                if (index < 0)
                    return false;
            }
            return true;
        }

        Value *write_ = nullptr;
        const Value *read_ = nullptr;
        bool ok_ = true;
        AnsiString failed_field_;
        AnsiString error_;
    };
}//namespace hgl::serialize
