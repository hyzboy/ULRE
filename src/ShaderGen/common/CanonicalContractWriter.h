#pragma once

#include <hgl/CoreType.h>
#include <hgl/type/ValueArray.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl::contract_detail
{
    using namespace hgl::graph::mtl;
    class CanonicalContractWriter
    {
        ValueArray<uint8> &bytes;

    public:
        explicit CanonicalContractWriter(ValueArray<uint8> &out_bytes)
            : bytes(out_bytes)
        {
            bytes.Clear();
        }

        void WriteU8(const uint8 value)
        {
            bytes.Add(value);
        }

        void WriteBool(const bool value)
        {
            WriteU8(value ? 1 : 0);
        }

        void WriteU16(const uint16 value)
        {
            WriteU8(static_cast<uint8>(value));
            WriteU8(static_cast<uint8>(value >> 8));
        }

        void WriteU32(const uint32 value)
        {
            WriteU8(static_cast<uint8>(value));
            WriteU8(static_cast<uint8>(value >> 8));
            WriteU8(static_cast<uint8>(value >> 16));
            WriteU8(static_cast<uint8>(value >> 24));
        }

        void WriteI32(const int32 value)
        {
            WriteU32(static_cast<uint32>(value));
        }

        void WriteU64(const uint64 value)
        {
            WriteU32(static_cast<uint32>(value));
            WriteU32(static_cast<uint32>(value >> 32));
        }
    };

    template<typename T, typename Less>
    void CanonicalSort(ValueArray<T> &values, const Less &less)
    {
        for (int i = 1; i < values.GetCount(); ++i)
        {
            const T value = values[i];
            int insert_at = i;
            while (insert_at > 0 && less(value, values[insert_at - 1]))
            {
                values[insert_at] = values[insert_at - 1];
                --insert_at;
            }
            values[insert_at] = value;
        }
    }

    inline uint64 HashCanonicalBytes(
        const ValueArray<uint8> &bytes) noexcept
    {
        if (bytes.IsEmpty())
            return 0;

        hgl::hash::FNV1aHasher64 h;
        h.AppendBytes(bytes.GetData(), static_cast<size_t>(bytes.GetCount()));
        return h;
    }
}
