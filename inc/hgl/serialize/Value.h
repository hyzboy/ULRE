#pragma once

/**
 * hgl::serialize::Value —— 通用序列化值树（Document Object Model）
 *
 * ── 为什么需要这层中间树 ──────────────────────────────────────────────────
 * 各格式的物理约束互不相容：
 *   · TOML / JSON 是「键 → 值」的字典，可以边遍历边写；
 *   · FlatBuffers 的缓冲区**从后往前**生长，父表的每个子串 / 子表 / 向量必须
 *     在父表开始构建之前全部建好，因此**无法**用「一次深度优先遍历、逐字段
 *     追加」的流式写入器直接产出。
 *
 * 若为每种格式各写一套「遍历 + 校验」逻辑，就是当前 `Toml.gen.h` 与
 * `Flat.gen.h` 两份代码并存、且将来每加一种格式再翻一倍的原因。
 *
 * 解法是把**语义**（哪个字段、什么类型、缺不缺、超不超范围）与**布局**（键名
 * 还是 voffset、字典还是向后增长的缓冲）彻底分开：
 *
 *     Serialize(Archive&, T&)  ──►  Value 树  ──►  格式后端（TOML / FlatBuffers / …）
 *
 * 遍历与校验只发生一次（在 `Serialize` 里），格式后端只负责「树 ↔ 自家表示」的
 * 机械搬运。新增 JSON 后端时不需要重新生成任何资产代码。
 *
 * ── 语义约定 ──────────────────────────────────────────────────────────────
 *   · 表（Table）是**有序**的：字段顺序 = 声明顺序 = FlatBuffers voffset 顺序；
 *   · 字段同时携带 `id`（二进制布局用）与 `name`（文本键名用），二者都可选；
 *   · 字符串统一按 UTF-8（`U8String`）保存，文本后端直接输出，平台字符串在
 *     边界处转换。
 *
 * ── 关于孩子数组为什么不用 ManagedArray ────────────────────────────────────
 * `ManagedArray<T>` / `PtrArray<T>` 在**类实例化时**就 `static_assert` 要求 T
 * 是完整类型（`is_trivially_copyable_v<T>` 需要对完整类型求值）。而 `Value`
 * 的孩子必然是 `Value` 自身，成员声明处 T 尚不完整，两者无法共存。
 * 因此孩子改为 `ValueArray<Value *>` + 本类自持所有权（构造/析构全部在类外
 * 定义，此时 `Value` 已完整）。这与 `ManagedArray::Create()` 内部的做法一致：
 * 所有权容器自己的实现负责 `new` / `delete`，调用方永不手工管理。
 */

#include <hgl/type/String.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/ValueArray.h>
#include <hgl/utf.h>

#include <cstdint>

namespace hgl::serialize
{
    /// 值树的节点类别。
    enum class ValueKind : uint8_t
    {
        Null = 0,   ///< 未设置
        Bool,
        Int,        ///< int64 有符号整数
        UInt,       ///< uint64 无符号整数
        Float,      ///< 单精度（保留区分，避免 double 往返丢精度诊断）
        Double,
        String,     ///< UTF-8 文本
        Array,      ///< 纯值序列（无键）
        Table,      ///< 有序键值表
    };

    inline const char *ValueKindName(const ValueKind kind)
    {
        switch (kind)
        {
        case ValueKind::Null:   return "null";
        case ValueKind::Bool:   return "bool";
        case ValueKind::Int:    return "int";
        case ValueKind::UInt:   return "uint";
        case ValueKind::Float:  return "float";
        case ValueKind::Double: return "double";
        case ValueKind::String: return "string";
        case ValueKind::Array:  return "array";
        case ValueKind::Table:  return "table";
        }
        return "unknown";
    }

    /**
     * 通用序列化值。
     *
     * 数组与表共用一份递归的孩子数组：表额外有并行的键名与字段 id 数组。
     */
    class Value
    {
    public:
        Value();
        Value(Value &&) noexcept;
        Value &operator=(Value &&) noexcept;
        ~Value();

        Value(const Value &) = delete;
        Value &operator=(const Value &) = delete;

        // ── 类别 ────────────────────────────────────────────────────────────

        ValueKind Kind() const { return kind_; }

        bool IsNull() const { return kind_ == ValueKind::Null; }
        bool IsBool() const { return kind_ == ValueKind::Bool; }
        bool IsInt() const { return kind_ == ValueKind::Int; }
        bool IsUInt() const { return kind_ == ValueKind::UInt; }
        bool IsFloat() const { return kind_ == ValueKind::Float; }
        bool IsDouble() const { return kind_ == ValueKind::Double; }
        bool IsString() const { return kind_ == ValueKind::String; }
        bool IsArray() const { return kind_ == ValueKind::Array; }
        bool IsTable() const { return kind_ == ValueKind::Table; }

        /// 是否为整型（有符号或无符号）。
        bool IsInteger() const { return kind_ == ValueKind::Int || kind_ == ValueKind::UInt; }

        /// 是否为浮点（单精度或双精度）。
        bool IsFloating() const { return kind_ == ValueKind::Float || kind_ == ValueKind::Double; }

        // ── 写入（会把节点整体重置为目标类别）──────────────────────────────

        void SetNull();
        void SetBool(bool v);
        void SetInt(int64_t v);
        void SetUInt(uint64_t v);
        void SetFloat(float v);
        void SetDouble(double v);
        void SetString(const AnsiString &v);
        void SetString(const char *v);
        void SetString(const U8String &v);
        void SetString(const OSString &v);
        void SetArray();
        void SetTable();

        // ── 读取 ────────────────────────────────────────────────────────────

        bool GetBool(bool &out) const;
        /// 允许 int / uint / float / double / bool 之间的宽化读取。
        bool GetDouble(double &out) const;
        bool GetInt(int64_t &out) const;
        bool GetUInt(uint64_t &out) const;
        /// 取出 UTF-8 文本；只对 String 节点成立。
        const U8String &GetString() const { return string_; }

        // ── 数组 ────────────────────────────────────────────────────────────

        int GetElementCount() const;
        /// 追加空元素并返回其指针。
        Value *AddElement();
        const Value *GetElement(int index) const;
        Value *GetElement(int index);

        // ── 表 ──────────────────────────────────────────────────────────────

        int GetFieldCount() const;

        /// 追加字段；同名（或仅给 id 时同 id）已存在则返回既有节点。
        Value *AddField(const char *name, uint32_t id);

        /// 按名字查表；`name` 为空时退回按 id 查。
        const Value *FindField(const char *name, uint32_t id = 0) const;
        Value *FindField(const char *name, uint32_t id = 0);

        const char *GetFieldName(int index) const;
        uint32_t GetFieldId(int index) const;
        const Value *GetFieldValue(int index) const;
        Value *GetFieldValue(int index);

        // ── 整体 ────────────────────────────────────────────────────────────

        /// 丢弃全部内容，恢复为 Null。
        void Reset();

        /// 深拷贝（`Value` 自身禁拷贝，这里显式提供）。
        void CopyFrom(const Value &other);

    private:
        void ResetPayload();
        Value *AppendChild();

        ValueKind kind_ = ValueKind::Null;

        bool bool_ = false;
        int64_t int_ = 0;
        uint64_t uint_ = 0;
        double number_ = 0.0;          ///< Float / Double 共用
        U8String string_;

        /// 表的键名；数组为空。
        ManagedArray<AnsiString> keys_;
        /// 表的字段 id；允许为空（纯文本后端不必填）。
        ValueArray<uint32_t> ids_;
        /// 数组元素或表的字段值；本类拥有所指对象的生命周期。
        ValueArray<Value *> children_;
    };

    // ── 生命周期（必须在 Value 完整之后定义）────────────────────────────────

    inline Value::Value() = default;
    inline Value::Value(Value &&) noexcept = default;
    inline Value &Value::operator=(Value &&) noexcept = default;

    inline Value::~Value()
    {
        ResetPayload();
    }

    // ── 内联实现 ────────────────────────────────────────────────────────────

    inline void Value::ResetPayload()
    {
        const int count = children_.GetCount();
        for (int i = 0; i < count; ++i)
            delete children_[i];
        children_.Clear();

        bool_ = false;
        int_ = 0;
        uint_ = 0;
        number_ = 0.0;
        string_.Clear();
        keys_.Clear();
        ids_.Clear();
    }

    inline void Value::Reset()
    {
        ResetPayload();
        kind_ = ValueKind::Null;
    }

    inline void Value::SetNull() { Reset(); }

    inline void Value::SetBool(const bool v)
    {
        ResetPayload();
        kind_ = ValueKind::Bool;
        bool_ = v;
    }

    inline void Value::SetInt(const int64_t v)
    {
        ResetPayload();
        kind_ = ValueKind::Int;
        int_ = v;
    }

    inline void Value::SetUInt(const uint64_t v)
    {
        ResetPayload();
        kind_ = ValueKind::UInt;
        uint_ = v;
    }

    inline void Value::SetFloat(const float v)
    {
        ResetPayload();
        kind_ = ValueKind::Float;
        number_ = static_cast<double>(v);
    }

    inline void Value::SetDouble(const double v)
    {
        ResetPayload();
        kind_ = ValueKind::Double;
        number_ = v;
    }

    inline void Value::SetString(const char *v)
    {
        ResetPayload();
        kind_ = ValueKind::String;
        if (v)
            string_ = U8String(reinterpret_cast<const u8char *>(v));
    }

    inline void Value::SetString(const AnsiString &v)
    {
        ResetPayload();
        kind_ = ValueKind::String;
        if (v.Length() > 0)
            string_ = U8String(reinterpret_cast<const u8char *>(v.c_str()),
                               v.Length());
    }

    inline void Value::SetString(const U8String &v)
    {
        ResetPayload();
        kind_ = ValueKind::String;
        string_ = v;
    }

    inline void Value::SetString(const OSString &v)
    {
        ResetPayload();
        kind_ = ValueKind::String;
        string_ = ToU8String(v);
    }

    inline void Value::SetArray()
    {
        ResetPayload();
        kind_ = ValueKind::Array;
    }

    inline void Value::SetTable()
    {
        ResetPayload();
        kind_ = ValueKind::Table;
    }

    inline bool Value::GetBool(bool &out) const
    {
        switch (kind_)
        {
        case ValueKind::Bool:   out = bool_; return true;
        case ValueKind::Int:    out = int_ != 0; return true;
        case ValueKind::UInt:   out = uint_ != 0; return true;
        case ValueKind::Float:
        case ValueKind::Double: out = number_ != 0.0; return true;
        default: return false;
        }
    }

    inline bool Value::GetDouble(double &out) const
    {
        switch (kind_)
        {
        case ValueKind::Float:
        case ValueKind::Double: out = number_; return true;
        case ValueKind::Int:    out = static_cast<double>(int_); return true;
        case ValueKind::UInt:   out = static_cast<double>(uint_); return true;
        case ValueKind::Bool:   out = bool_ ? 1.0 : 0.0; return true;
        default: return false;
        }
    }

    inline bool Value::GetInt(int64_t &out) const
    {
        switch (kind_)
        {
        case ValueKind::Int:    out = int_; return true;
        case ValueKind::UInt:
            if (uint_ > static_cast<uint64_t>(INT64_MAX))
                return false;
            out = static_cast<int64_t>(uint_);
            return true;
        case ValueKind::Float:
        case ValueKind::Double:
        {
            // 只接受无损转换，避免把 1.5 静默截成 1。
            const int64_t truncated = static_cast<int64_t>(number_);
            if (static_cast<double>(truncated) != number_)
                return false;
            out = truncated;
            return true;
        }
        case ValueKind::Bool:   out = bool_ ? 1 : 0; return true;
        default: return false;
        }
    }

    inline bool Value::GetUInt(uint64_t &out) const
    {
        switch (kind_)
        {
        case ValueKind::UInt:   out = uint_; return true;
        case ValueKind::Int:
            if (int_ < 0)
                return false;
            out = static_cast<uint64_t>(int_);
            return true;
        case ValueKind::Float:
        case ValueKind::Double:
        {
            if (number_ < 0.0)
                return false;
            const uint64_t truncated = static_cast<uint64_t>(number_);
            if (static_cast<double>(truncated) != number_)
                return false;
            out = truncated;
            return true;
        }
        case ValueKind::Bool:   out = bool_ ? 1u : 0u; return true;
        default: return false;
        }
    }

    inline int Value::GetElementCount() const
    {
        return children_.GetCount();
    }

    inline int Value::GetFieldCount() const
    {
        return keys_.GetCount();
    }

    inline Value *Value::AppendChild()
    {
        Value *slot = new Value();
        children_.Add(slot);
        return slot;
    }

    inline Value *Value::AddElement()
    {
        if (kind_ != ValueKind::Array)
            SetArray();

        return AppendChild();
    }

    inline const Value *Value::GetElement(const int index) const
    {
        if (index < 0 || index >= children_.GetCount())
            return nullptr;
        return children_[index];
    }

    inline Value *Value::GetElement(const int index)
    {
        if (index < 0 || index >= children_.GetCount())
            return nullptr;
        return children_[index];
    }

    inline Value *Value::AddField(const char *name, const uint32_t id)
    {
        if (kind_ != ValueKind::Table)
            SetTable();

        const int count = keys_.GetCount();

        if (name && name[0])
        {
            for (int i = 0; i < count; ++i)
            {
                const AnsiString *existing = keys_.At(i);
                if (existing && hgl::stricmp(existing->c_str(), name) == 0)
                    return children_[i];
            }
        }
        else if (id != 0)
        {
            for (int i = 0; i < count; ++i)
            {
                if (i < ids_.GetCount() && ids_[i] == id)
                    return children_[i];
            }
        }

        keys_.Create(AnsiString(name ? name : ""));
        ids_.Add(id);
        return AppendChild();
    }

    inline const Value *Value::FindField(const char *name, const uint32_t id) const
    {
        if (kind_ != ValueKind::Table)
            return nullptr;

        const int count = keys_.GetCount();

        if (name && name[0])
        {
            for (int i = 0; i < count; ++i)
            {
                const AnsiString *key = keys_.At(i);
                if (key && hgl::stricmp(key->c_str(), name) == 0)
                    return children_[i];
            }
            return nullptr;
        }

        if (id == 0)
            return nullptr;

        for (int i = 0; i < count; ++i)
        {
            if (i < ids_.GetCount() && ids_[i] == id)
                return children_[i];
        }
        return nullptr;
    }

    inline Value *Value::FindField(const char *name, const uint32_t id)
    {
        return const_cast<Value *>(
            static_cast<const Value *>(this)->FindField(name, id));
    }

    inline const char *Value::GetFieldName(const int index) const
    {
        if (index < 0 || index >= keys_.GetCount())
            return nullptr;
        const AnsiString *key = keys_.At(index);
        return key ? key->c_str() : nullptr;
    }

    inline uint32_t Value::GetFieldId(const int index) const
    {
        if (index < 0 || index >= ids_.GetCount())
            return 0;
        return ids_[index];
    }

    inline const Value *Value::GetFieldValue(const int index) const
    {
        if (index < 0 || index >= children_.GetCount())
            return nullptr;
        return children_[index];
    }

    inline Value *Value::GetFieldValue(const int index)
    {
        if (index < 0 || index >= children_.GetCount())
            return nullptr;
        return children_[index];
    }

    inline void Value::CopyFrom(const Value &other)
    {
        if (this == &other)
            return;

        ResetPayload();
        kind_ = other.kind_;
        bool_ = other.bool_;
        int_ = other.int_;
        uint_ = other.uint_;
        number_ = other.number_;
        string_ = other.string_;

        const int key_count = other.keys_.GetCount();
        for (int i = 0; i < key_count; ++i)
        {
            const AnsiString *key = other.keys_.At(i);
            keys_.Create(key ? *key : AnsiString());
        }

        const int id_count = other.ids_.GetCount();
        for (int i = 0; i < id_count; ++i)
            ids_.Add(other.ids_[i]);

        const int child_count = other.children_.GetCount();
        for (int i = 0; i < child_count; ++i)
        {
            Value *slot = AppendChild();
            const Value *source = other.children_[i];
            if (slot && source)
                slot->CopyFrom(*source);
        }
    }
}//namespace hgl::serialize
