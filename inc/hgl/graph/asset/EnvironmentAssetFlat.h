#pragma once

/**
 * 环境资产的 **FlatBuffers 二进制形态**（引擎自有序列化）。
 *
 * 遵循引擎统一序列化架构（Unified Serialize Archive）：
 *
 * ```
 * EnvironmentSpec ⇄ hgl::serialize 值树 ⇄ FlatBuffers 缓冲
 * ```
 *
 * 不使用手写 `.fbs` 或 `flatc`：反射元数据 `Class_EnvironmentSpec` 描述了
 * 字段名、类别与结构大小，`hgl::serialize::fb` 据此读写二进制表。
 */

#include <hgl/asset/generated/EnvironmentSerialize.gen.h>
#include <hgl/graph/asset/EnvironmentAssetSpec.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>
#include <hgl/log/Log.h>

#include <cstring>

namespace hgl::graph
{
    /**
     * @brief 环境资产规约 → FlatBuffers 缓冲（完整缓冲，含根偏移）。
     */
    inline bool EncodeEnvironmentFlat(const EnvironmentSpec &spec,
                                      ValueArray<uint8> &out_bytes,
                                      AnsiString *out_error = nullptr)
    {
        out_bytes.Clear();

        flatbuffers::FlatBufferBuilder fbb;
        if (!hgl::serialize::fb::EncodeToFlatBuffers<EnvironmentSpec>(
                fbb, spec, nullptr, out_error))
            return false;

        const int size = static_cast<int>(fbb.GetSize());
        if (size <= 0 || !out_bytes.Resize(size))
        {
            if (out_error)
                *out_error = "分配 FlatBuffers 输出缓冲失败";
            return false;
        }

        std::memcpy(out_bytes.GetData(), fbb.GetBufferPointer(),
                    static_cast<size_t>(size));
        return true;
    }

    /**
     * @brief FlatBuffers 缓冲 → 环境资产规约（先校验再解码）。
     */
    inline bool DecodeEnvironmentFlat(const uint8 *data,
                                      uint64 size,
                                      EnvironmentSpec &out_spec,
                                      AnsiString *out_error = nullptr)
    {
        out_spec = EnvironmentSpec{};

        if (!data || size < FLATBUFFERS_MIN_BUFFER_SIZE)
        {
            if (out_error)
                *out_error = "缓冲长度不足一个 FlatBuffers 根表";
            return false;
        }

        if (!hgl::serialize::fb::VerifyAndDecodeFlatBuffers<EnvironmentSpec>(
                data, static_cast<size_t>(size), out_spec, nullptr, out_error))
            return false;

        return true;
    }

    /**
     * @brief 将环境规约以 FlatBuffers 二进制保存至文件。
     */
    inline bool SaveEnvironmentSpecToFlatBuffers(const EnvironmentSpec &spec,
                                                 const OSString &filename)
    {
        ValueArray<uint8> bytes;
        AnsiString error;
        if (!EncodeEnvironmentFlat(spec, bytes, &error))
        {
            GLogError(u8"[EnvironmentAssetFlat] 编码失败: %s", error.c_str());
            return false;
        }

        hgl::io::FileOutputStream *fos = hgl::io::CreateFileOutputStream(filename);
        if (!fos)
        {
            GLogError(u8"[EnvironmentAssetFlat] 无法创建输出文件: %ls", filename.c_str());
            return false;
        }

        const int64 written = fos->Write(bytes.GetData(), bytes.GetCount());
        delete fos;
        return written == bytes.GetCount();
    }

    /**
     * @brief 从 FlatBuffers 二进制文件读取环境规约。
     */
    inline bool LoadEnvironmentSpecFromFlatBuffers(const OSString &filename,
                                                   EnvironmentSpec &out_spec)
    {
        hgl::io::OpenFileInputStream opener(filename);
        if (!opener)
            return false;

        const int64 size = opener->GetSize();
        if (size < FLATBUFFERS_MIN_BUFFER_SIZE || size > 64 * 1024 * 1024)
            return false;

        AutoDeleteArray<uint8> buffer(static_cast<size_t>(size));
        if (!buffer || opener->Read(buffer.data(), size) != size)
            return false;

        AnsiString error;
        return DecodeEnvironmentFlat(buffer.data(), static_cast<uint64>(size), out_spec, &error);
    }
}//namespace hgl::graph
