#pragma once

/**
 * FlatBuffers 后端 —— `hgl::serialize::Value` 树 ↔ FlatBuffers 缓冲。
 *
 * ── 为什么这里需要「布局描述」而不是再生成一遍逻辑 ──────────────────────────
 * FlatBuffers 的物理布局（voffset、内联 struct、向量元素类型）无法从值树本身
 * 推断，必须由 schema 决定。因此 AssetGen 额外吐出一份**纯数据**的
 * `TableDesc` 描述表（字段 id / 名字 / 类型 / voffset），由本文件通用地解释它。
 *
 * 关键差别：语义（缺省、校验、枚举名、容器降级）**只存在于** `Serialize()` 里一次；
 * 这里只有「按描述搬运字节」的机械逻辑。新增 JSON / Protobuf 后端时，同样只需
 * 一份自己的描述解释器，不需要重新生成任何资产代码。
 *
 * ── 写入顺序约束 ──────────────────────────────────────────────────────────
 * FlatBuffers 缓冲区向后增长，父表的子串 / 子表 / 向量必须在 `StartTable()` 之前
 * 建好。所以编码分两趟：
 *   1. 逐字段预建子对象，记下 offset 或内联 struct 字节；
 *   2. `StartTable()` 后逐个 `AddElement` / `AddOffset` / `AddStruct`，再 `EndTable()`。
 * 这正是值树这层中间表示存在的意义 —— 遍历顺序与缓冲布局顺序彻底解耦。
 *
 * ── 校验 ──────────────────────────────────────────────────────────────────
 * 通用表没有 flatc 生成的 `Verify()` 成员，无法在此复现 schema 级校验。
 * 需要强校验的调用方应先做
 * `flatbuffers::Verifier::VerifyBuffer<schema::Xxx>(nullptr)`（用的是已有生成头），
 * 再调用这里的通用解码。
 */

#include <hgl/serialize/Value.h>

#include <flatbuffers/flatbuffers.h>

#include <cstring>

namespace hgl::serialize::fb
{
    /// 字段在 FlatBuffers 里的类型。
    enum class FbType : uint8_t
    {
        None = 0,
        Bool,
        Int8, Int16, Int32, Int64,
        UInt8, UInt16, UInt32, UInt64,
        Float, Double,
        Enum,          ///< 有底层 32 位整数的 enum class；树上以名字字符串保存
        String,
        Struct,        ///< 内联定长数值结构（Float2 / Float3 / Float4）
        Table,         ///< 子表
        VectorScalar,  ///< vector<标量>
        VectorString,  ///< vector<string>
        VectorTable,   ///< vector<表>
    };

    struct TableDesc;

    /// 枚举名 ↔ 数值；由 AssetGen 生成，直接转发到生成的 AssetEnum* 函数。
    using EnumFromNameFn = bool (*)(const char *name, int64_t &out);
    using EnumToNameFn = const char *(*)(int64_t value);

    struct FieldDesc
    {
        uint32_t id = 0;                  ///< 值树里的字段 id（与 Serialize 的 id 一致）
        const char *name = nullptr;       ///< 值树里的字段名
        FbType type = FbType::None;
        uint16_t voffset = 0;             ///< FlatBuffers 字段偏移：4 + 2*声明序号
        FbType elem_type = FbType::None;  ///< 向量元素类型
        uint8_t components = 0;           ///< Struct 的分量数（2/3/4）
        const TableDesc *table = nullptr; ///< Table / VectorTable 的子表描述
        EnumFromNameFn enum_from_name = nullptr; ///< Enum 专用
        EnumToNameFn enum_to_name = nullptr;     ///< Enum 专用
    };

    struct TableDesc
    {
        const char *name = nullptr;
        const FieldDesc *fields = nullptr;
        int field_count = 0;

        const FieldDesc *Find(const uint32_t id) const
        {
            for (int i = 0; i < field_count; ++i)
            {
                if (fields[i].id == id)
                    return &fields[i];
            }
            return nullptr;
        }

        const FieldDesc *FindByName(const char *name) const
        {
            if (!name)
                return nullptr;
            for (int i = 0; i < field_count; ++i)
            {
                if (fields[i].name && hgl::stricmp(fields[i].name, name) == 0)
                    return &fields[i];
            }
            return nullptr;
        }
    };

    // ── 内联 struct 载体 ────────────────────────────────────────────────────
    //
    // FlatBuffers 的 struct 是「按最大成员对齐的裸字节块」。本工程用到的定长
    // 向量结构只有 2/3/4 个 32 位分量（Float2/3/4），用三个定宽 POD 即可完全
    // 覆盖，且对齐与 flatc 生成的结构一致（对齐 4）。

    struct Vec2f { float x, y; };
    struct Vec3f { float x, y, z; };
    struct Vec4f { float x, y, z, w; };

    union StructBuffer
    {
        float f[4];
        Vec2f v2;
        Vec3f v3;
        Vec4f v4;
        uint8_t raw[16];
    };

    namespace detail
    {
        template <typename T>
        inline void DecodeScalarVector(const flatbuffers::Table *t,
                                       const flatbuffers::voffset_t vo,
                                       Value &out)
        {
            const auto *vec = t->GetPointer<const flatbuffers::Vector<T> *>(vo);
            out.SetArray();
            if (!vec)
                return;
            for (uint32_t i = 0; i < vec->size(); ++i)
            {
                Value *slot = out.AddElement();
                if (!slot)
                    continue;
                const T v = vec->Get(i);
                if constexpr (std::is_floating_point_v<T>)
                    slot->SetDouble(static_cast<double>(v));
                else if constexpr (std::is_signed_v<T>)
                    slot->SetInt(static_cast<int64_t>(v));
                else
                    slot->SetUInt(static_cast<uint64_t>(v));
            }
        }

        /// 标量向量编码：把树上的数组逐项转成目标标量宽度后一次性建向量。
        inline flatbuffers::Offset<void> EncodeScalarVector(const FbType elem,
                                                            const Value &node,
                                                            flatbuffers::FlatBufferBuilder &fbb)
        {
            const int count = node.GetElementCount();

            switch (elem)
            {
            case FbType::Bool:
            case FbType::UInt8:
            {
                ValueArray<uint8_t> tmp;
                for (int i = 0; i < count; ++i)
                {
                    uint64_t raw = 0;
                    if (const Value *v = node.GetElement(i))
                        v->GetUInt(raw);
                    tmp.Add(static_cast<uint8_t>(raw));
                }
                return flatbuffers::Offset<void>(
                    fbb.CreateVector<uint8_t>(tmp.GetData(), static_cast<size_t>(count)).o);
            }
            case FbType::Int8:
            {
                ValueArray<int8_t> tmp;
                for (int i = 0; i < count; ++i)
                {
                    int64_t raw = 0;
                    if (const Value *v = node.GetElement(i))
                        v->GetInt(raw);
                    tmp.Add(static_cast<int8_t>(raw));
                }
                return flatbuffers::Offset<void>(
                    fbb.CreateVector<int8_t>(tmp.GetData(), static_cast<size_t>(count)).o);
            }
            case FbType::Int16:
            {
                ValueArray<int16_t> tmp;
                for (int i = 0; i < count; ++i)
                {
                    int64_t raw = 0;
                    if (const Value *v = node.GetElement(i))
                        v->GetInt(raw);
                    tmp.Add(static_cast<int16_t>(raw));
                }
                return flatbuffers::Offset<void>(
                    fbb.CreateVector<int16_t>(tmp.GetData(), static_cast<size_t>(count)).o);
            }
            case FbType::UInt16:
            {
                ValueArray<uint16_t> tmp;
                for (int i = 0; i < count; ++i)
                {
                    uint64_t raw = 0;
                    if (const Value *v = node.GetElement(i))
                        v->GetUInt(raw);
                    tmp.Add(static_cast<uint16_t>(raw));
                }
                return flatbuffers::Offset<void>(
                    fbb.CreateVector<uint16_t>(tmp.GetData(), static_cast<size_t>(count)).o);
            }
            case FbType::Int32:
            case FbType::Enum:
            {
                ValueArray<int32_t> tmp;
                for (int i = 0; i < count; ++i)
                {
                    int64_t raw = 0;
                    if (const Value *v = node.GetElement(i))
                        v->GetInt(raw);
                    tmp.Add(static_cast<int32_t>(raw));
                }
                return flatbuffers::Offset<void>(
                    fbb.CreateVector<int32_t>(tmp.GetData(), static_cast<size_t>(count)).o);
            }
            case FbType::UInt32:
            {
                ValueArray<uint32_t> tmp;
                for (int i = 0; i < count; ++i)
                {
                    uint64_t raw = 0;
                    if (const Value *v = node.GetElement(i))
                        v->GetUInt(raw);
                    tmp.Add(static_cast<uint32_t>(raw));
                }
                return flatbuffers::Offset<void>(
                    fbb.CreateVector<uint32_t>(tmp.GetData(), static_cast<size_t>(count)).o);
            }
            case FbType::Int64:
            {
                ValueArray<int64_t> tmp;
                for (int i = 0; i < count; ++i)
                {
                    int64_t raw = 0;
                    if (const Value *v = node.GetElement(i))
                        v->GetInt(raw);
                    tmp.Add(raw);
                }
                return flatbuffers::Offset<void>(
                    fbb.CreateVector<int64_t>(tmp.GetData(), static_cast<size_t>(count)).o);
            }
            case FbType::UInt64:
            {
                ValueArray<uint64_t> tmp;
                for (int i = 0; i < count; ++i)
                {
                    uint64_t raw = 0;
                    if (const Value *v = node.GetElement(i))
                        v->GetUInt(raw);
                    tmp.Add(raw);
                }
                return flatbuffers::Offset<void>(
                    fbb.CreateVector<uint64_t>(tmp.GetData(), static_cast<size_t>(count)).o);
            }
            case FbType::Float:
            {
                ValueArray<float> tmp;
                for (int i = 0; i < count; ++i)
                {
                    double raw = 0.0;
                    if (const Value *v = node.GetElement(i))
                        v->GetDouble(raw);
                    tmp.Add(static_cast<float>(raw));
                }
                return flatbuffers::Offset<void>(
                    fbb.CreateVector<float>(tmp.GetData(), static_cast<size_t>(count)).o);
            }
            default:
            {
                ValueArray<double> tmp;
                for (int i = 0; i < count; ++i)
                {
                    double raw = 0.0;
                    if (const Value *v = node.GetElement(i))
                        v->GetDouble(raw);
                    tmp.Add(raw);
                }
                return flatbuffers::Offset<void>(
                    fbb.CreateVector<double>(tmp.GetData(), static_cast<size_t>(count)).o);
            }
            }
        }

        /// 枚举字段的数值化：树上可能是名字字符串，也可能是整数。
        inline int64_t ResolveEnumValue(const Value &node, const FieldDesc &field)
        {
            int64_t raw = 0;
            if (node.IsString())
            {
                const U8String &utf8 = node.GetString();
                const char *name = reinterpret_cast<const char *>(utf8.c_str());
                if (field.enum_from_name && name)
                    field.enum_from_name(name, raw);
                return raw;
            }
            node.GetInt(raw);
            return raw;
        }
    }//namespace detail

    // ── 编码 ────────────────────────────────────────────────────────────────

    inline flatbuffers::Offset<void> EncodeTable(flatbuffers::FlatBufferBuilder &fbb,
                                                 const Value &node,
                                                 const TableDesc &desc);

    /**
     * 把值树按描述写成 FlatBuffers 表。
     * 值树里存在但描述里没有的字段会被忽略（例如仅编辑器使用的 EditOnly 字段）。
     */
    inline flatbuffers::Offset<void> EncodeTable(flatbuffers::FlatBufferBuilder &fbb,
                                                 const Value &node,
                                                 const TableDesc &desc)
    {
        ValueArray<flatbuffers::Offset<void>> offsets;
        ValueArray<uint8_t> has_offset;
        ValueArray<StructBuffer> structs;
        ValueArray<uint8_t> has_struct;

        for (int i = 0; i < desc.field_count; ++i)
        {
            offsets.Add(flatbuffers::Offset<void>());
            has_offset.Add(0);
            StructBuffer empty{};
            structs.Add(empty);
            has_struct.Add(0);
        }

        // ── 第一趟：预建所有子对象 ─────────────────────────────────────────
        for (int i = 0; i < desc.field_count; ++i)
        {
            const FieldDesc &field = desc.fields[i];
            const Value *value = node.FindField(field.name, field.id);
            if (!value)
                continue;

            switch (field.type)
            {
            case FbType::String:
            {
                const U8String &utf8 = value->GetString();
                const char *text = reinterpret_cast<const char *>(utf8.c_str());
                offsets[i] = flatbuffers::Offset<void>(
                    fbb.CreateString(text ? text : "").o);
                has_offset[i] = 1;
                break;
            }
            case FbType::Table:
            {
                if (!field.table || !value->IsTable())
                    break;
                offsets[i] = EncodeTable(fbb, *value, *field.table);
                has_offset[i] = 1;
                break;
            }
            case FbType::Struct:
            {
                StructBuffer buffer{};
                const int components = (field.components >= 2 && field.components <= 4)
                                           ? field.components
                                           : 3;
                for (int c = 0; c < components; ++c)
                {
                    double raw = 0.0;
                    if (const Value *item = value->GetElement(c))
                        item->GetDouble(raw);
                    buffer.f[c] = static_cast<float>(raw);
                }
                structs[i] = buffer;
                has_struct[i] = 1;
                break;
            }
            case FbType::VectorScalar:
            {
                offsets[i] = detail::EncodeScalarVector(field.elem_type, *value, fbb);
                has_offset[i] = 1;
                break;
            }
            case FbType::VectorTable:
            {
                if (!field.table || !value->IsArray())
                    break;

                const int count = value->GetElementCount();
                ValueArray<flatbuffers::Offset<void>> items;
                for (int k = 0; k < count; ++k)
                {
                    const Value *item = value->GetElement(k);
                    items.Add(item ? EncodeTable(fbb, *item, *field.table)
                                   : flatbuffers::Offset<void>());
                }
                offsets[i] = flatbuffers::Offset<void>(
                    fbb.CreateVector(items.GetData(),
                                     static_cast<size_t>(count)).o);
                has_offset[i] = 1;
                break;
            }
            default:
                break;
            }
        }

        // ── 第二趟：建表本体 ───────────────────────────────────────────────
        const flatbuffers::uoffset_t start = fbb.StartTable();

        for (int i = 0; i < desc.field_count; ++i)
        {
            const FieldDesc &field = desc.fields[i];
            const Value *value = node.FindField(field.name, field.id);
            if (!value)
                continue;

            switch (field.type)
            {
            case FbType::Bool:
            {
                bool b = false;
                value->GetBool(b);
                fbb.AddElement<uint8_t>(field.voffset, b ? 1u : 0u, 0);
                break;
            }
            case FbType::Int8:
            {
                int64_t raw = 0;
                value->GetInt(raw);
                fbb.AddElement<int8_t>(field.voffset, static_cast<int8_t>(raw), 0);
                break;
            }
            case FbType::Int16:
            {
                int64_t raw = 0;
                value->GetInt(raw);
                fbb.AddElement<int16_t>(field.voffset, static_cast<int16_t>(raw), 0);
                break;
            }
            case FbType::Int32:
            case FbType::Enum:
            {
                const int64_t raw = field.type == FbType::Enum
                                        ? detail::ResolveEnumValue(*value, field)
                                        : [&] { int64_t v = 0; value->GetInt(v); return v; }();
                fbb.AddElement<int32_t>(field.voffset, static_cast<int32_t>(raw), 0);
                break;
            }
            case FbType::Int64:
            {
                int64_t raw = 0;
                value->GetInt(raw);
                fbb.AddElement<int64_t>(field.voffset, raw, 0);
                break;
            }
            case FbType::UInt8:
            {
                uint64_t raw = 0;
                value->GetUInt(raw);
                fbb.AddElement<uint8_t>(field.voffset, static_cast<uint8_t>(raw), 0);
                break;
            }
            case FbType::UInt16:
            {
                uint64_t raw = 0;
                value->GetUInt(raw);
                fbb.AddElement<uint16_t>(field.voffset, static_cast<uint16_t>(raw), 0);
                break;
            }
            case FbType::UInt32:
            {
                uint64_t raw = 0;
                value->GetUInt(raw);
                fbb.AddElement<uint32_t>(field.voffset, static_cast<uint32_t>(raw), 0);
                break;
            }
            case FbType::UInt64:
            {
                uint64_t raw = 0;
                value->GetUInt(raw);
                fbb.AddElement<uint64_t>(field.voffset, raw, 0);
                break;
            }
            case FbType::Float:
            {
                double raw = 0.0;
                value->GetDouble(raw);
                fbb.AddElement<float>(field.voffset, static_cast<float>(raw), 0.0f);
                break;
            }
            case FbType::Double:
            {
                double raw = 0.0;
                value->GetDouble(raw);
                fbb.AddElement<double>(field.voffset, raw, 0.0);
                break;
            }
            case FbType::Struct:
                if (has_struct[i])
                {
                    const int components = (field.components >= 2 && field.components <= 4)
                                               ? field.components
                                               : 3;
                    if (components == 2)
                        fbb.AddStruct(field.voffset, &structs[i].v2);
                    else if (components == 3)
                        fbb.AddStruct(field.voffset, &structs[i].v3);
                    else
                        fbb.AddStruct(field.voffset, &structs[i].v4);
                }
                break;
            default:
                if (has_offset[i])
                    fbb.AddOffset(field.voffset, offsets[i]);
                break;
            }
        }

        const flatbuffers::uoffset_t end = fbb.EndTable(start);
        return flatbuffers::Offset<void>(end);
    }

    // ── 解码 ────────────────────────────────────────────────────────────────

    inline void DecodeTable(const flatbuffers::Table *t, const TableDesc &desc,
                            Value &out);

    inline void DecodeTable(const flatbuffers::Table *t, const TableDesc &desc,
                            Value &out)
    {
        out.SetTable();
        if (!t)
            return;

        for (int i = 0; i < desc.field_count; ++i)
        {
            const FieldDesc &field = desc.fields[i];
            const bool present = t->GetOptionalFieldOffset(field.voffset) != 0;

            // 与 TOML 的「缺键保留类内初始值」不同：FlatBuffers 是**完整快照**。
            // 标量字段即使等于 schema 缺省值（因此没写进缓冲）也必须覆盖目标，
            // 否则 C++ 类内初始值会盖过 schema 缺省值。
            const bool is_scalar_field =
                field.type != FbType::String && field.type != FbType::Table
                && field.type != FbType::Struct && field.type != FbType::VectorScalar
                && field.type != FbType::VectorString && field.type != FbType::VectorTable
                && field.type != FbType::None;

            if (!present && !is_scalar_field)
                continue;

            Value *slot = out.AddField(field.name, field.id);
            if (!slot)
                continue;

            switch (field.type)
            {
            case FbType::Bool:
                slot->SetBool(t->GetField<uint8_t>(field.voffset, 0) != 0);
                break;
            case FbType::Int8:
                slot->SetInt(t->GetField<int8_t>(field.voffset, 0));
                break;
            case FbType::Int16:
                slot->SetInt(t->GetField<int16_t>(field.voffset, 0));
                break;
            case FbType::Int32:
                slot->SetInt(t->GetField<int32_t>(field.voffset, 0));
                break;
            case FbType::Int64:
                slot->SetInt(t->GetField<int64_t>(field.voffset, 0));
                break;
            case FbType::UInt8:
                slot->SetUInt(t->GetField<uint8_t>(field.voffset, 0));
                break;
            case FbType::UInt16:
                slot->SetUInt(t->GetField<uint16_t>(field.voffset, 0));
                break;
            case FbType::UInt32:
                slot->SetUInt(t->GetField<uint32_t>(field.voffset, 0));
                break;
            case FbType::UInt64:
                slot->SetUInt(t->GetField<uint64_t>(field.voffset, 0));
                break;
            case FbType::Float:
                slot->SetFloat(t->GetField<float>(field.voffset, 0.0f));
                break;
            case FbType::Double:
                slot->SetDouble(t->GetField<double>(field.voffset, 0.0));
                break;
            case FbType::Enum:
            {
                const int64_t raw = t->GetField<int32_t>(field.voffset, 0);
                const char *name = field.enum_to_name ? field.enum_to_name(raw) : nullptr;
                if (name)
                    slot->SetString(name);
                else
                    slot->SetInt(raw);
                break;
            }
            case FbType::String:
            {
                const auto *str = t->GetPointer<const flatbuffers::String *>(field.voffset);
                slot->SetString(str ? str->c_str() : "");
                break;
            }
            case FbType::Struct:
            {
                const int components = (field.components >= 2 && field.components <= 4)
                                           ? field.components
                                           : 3;
                slot->SetArray();
                const float *p = nullptr;
                if (components == 2)
                {
                    const auto *s = t->GetStruct<const Vec2f *>(field.voffset);
                    if (s)
                        p = reinterpret_cast<const float *>(s);
                }
                else if (components == 3)
                {
                    const auto *s = t->GetStruct<const Vec3f *>(field.voffset);
                    if (s)
                        p = reinterpret_cast<const float *>(s);
                }
                else
                {
                    const auto *s = t->GetStruct<const Vec4f *>(field.voffset);
                    if (s)
                        p = reinterpret_cast<const float *>(s);
                }
                if (p)
                {
                    for (int c = 0; c < components; ++c)
                    {
                        Value *item = slot->AddElement();
                        if (item)
                            item->SetFloat(p[c]);
                    }
                }
                break;
            }
            case FbType::Table:
            {
                const auto *sub =
                    t->GetPointer<const flatbuffers::Table *>(field.voffset);
                if (field.table && sub)
                    DecodeTable(sub, *field.table, *slot);
                break;
            }
            case FbType::VectorScalar:
                switch (field.elem_type)
                {
                case FbType::Bool:
                case FbType::UInt8:
                    detail::DecodeScalarVector<uint8_t>(t, field.voffset, *slot);
                    break;
                case FbType::Int8:
                    detail::DecodeScalarVector<int8_t>(t, field.voffset, *slot);
                    break;
                case FbType::Int16:
                    detail::DecodeScalarVector<int16_t>(t, field.voffset, *slot);
                    break;
                case FbType::UInt16:
                    detail::DecodeScalarVector<uint16_t>(t, field.voffset, *slot);
                    break;
                case FbType::Int32:
                case FbType::Enum:
                    detail::DecodeScalarVector<int32_t>(t, field.voffset, *slot);
                    break;
                case FbType::UInt32:
                    detail::DecodeScalarVector<uint32_t>(t, field.voffset, *slot);
                    break;
                case FbType::Int64:
                    detail::DecodeScalarVector<int64_t>(t, field.voffset, *slot);
                    break;
                case FbType::UInt64:
                    detail::DecodeScalarVector<uint64_t>(t, field.voffset, *slot);
                    break;
                case FbType::Float:
                    detail::DecodeScalarVector<float>(t, field.voffset, *slot);
                    break;
                default:
                    detail::DecodeScalarVector<double>(t, field.voffset, *slot);
                    break;
                }
                break;
            case FbType::VectorTable:
            {
                const auto *vec = t->GetPointer<const flatbuffers::Vector<
                    flatbuffers::Offset<flatbuffers::Table>> *>(field.voffset);
                slot->SetArray();
                if (vec && field.table)
                {
                    for (uint32_t k = 0; k < vec->size(); ++k)
                    {
                        Value *item = slot->AddElement();
                        if (!item)
                            continue;
                        DecodeTable(vec->Get(k), *field.table, *item);
                    }
                }
                break;
            }
            default:
                break;
            }
        }
    }

    // ── 缓冲级入口 ──────────────────────────────────────────────────────────

    /// 值树 → 完整 FlatBuffers 缓冲（含根表偏移与可选的 file identifier）。
    inline void EncodeToBuffer(flatbuffers::FlatBufferBuilder &fbb,
                               const Value &root,
                               const TableDesc &desc,
                               const char *file_identifier = nullptr)
    {
        const flatbuffers::Offset<void> root_offset = EncodeTable(fbb, root, desc);
        if (file_identifier && file_identifier[0])
            fbb.Finish(root_offset, file_identifier);
        else
            fbb.Finish(root_offset);
    }

    /// 完整 FlatBuffers 缓冲 → 值树。强校验请先用 flatc 生成的根类型跑 Verifier。
    inline bool DecodeFromBuffer(const uint8_t *data, const size_t size,
                                 const TableDesc &desc,
                                 Value &out)
    {
        if (!data || size < FLATBUFFERS_MIN_BUFFER_SIZE)
            return false;

        const flatbuffers::Table *root =
            flatbuffers::GetRoot<flatbuffers::Table>(data);
        if (!root)
            return false;

        DecodeTable(root, desc, out);
        return true;
    }
}//namespace hgl::serialize::fb
