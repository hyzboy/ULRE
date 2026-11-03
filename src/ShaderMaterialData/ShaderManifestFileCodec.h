// ULRE.ShaderMaterialData —— 两份程序清单（graphics / compute）的共享磁盘编解码。
//
// 文件格式 = ShaderArtifactFileHeader + AssetGen 反射表驱动的 FlatBuffers 载荷
// （FNV-1a 校验和）。两份清单只在「载荷形状 / kind / stage / format_version /
// 逐条 entry 校验谓词」上不同，全部由模板参数与实参注入；字节序列与提取前的
// 两份手写实现逐字段一致 —— 持久化格式不变，不触发 golden 漂移。
//
// 归属：这是两份 .cpp 的实现细节（不是跨层 API），故为模块内部头，不放 inc/。
#ifndef _HGL_SHADER_MANIFEST_FILE_CODEC_H_
#define _HGL_SHADER_MANIFEST_FILE_CODEC_H_

#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/ShaderArtifactContract.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::mtl
{
    /// 一段字节的 FNV-1a（空指针或零长 = 初值哈希）。
    inline uint64 ManifestHashBytes(const void *data, const uint64 byte_size)
    {
        hgl::hash::FNV1aHasher64 hasher;
        if (data && byte_size > 0)
            hasher.AppendBytes(data, static_cast<size_t>(byte_size));
        return static_cast<uint64>(hasher);
    }

    /// 载荷编码 + header 组装 → 完整文件字节序列写 path（父目录自动创建）。
    /// label 只进错误日志（如 "program manifest"）。
    template <typename PayloadT>
    bool WriteManifestArtifactFile(const OSString &path,
                                   ShaderArtifactKind kind,
                                   uint32 stage,
                                   uint64 key_digest,
                                   uint32 source_digest,
                                   const PayloadT &payload_struct,
                                   const char *label)
    {
        if (path.IsEmpty())
            return false;

        filesystem::Path file_path(path);
        const OSString directory = file_path.GetParent().ToOSString();
        if (!directory.IsEmpty()
         && !filesystem::IsDirectory(directory)
         && !filesystem::MakePath(directory))
            return false;

        ValueArray<uint8> payload;
        AnsiString encode_error;
        if (!hgl::serialize::fb::EncodeToFlatBuffers(payload, payload_struct,
                                                     nullptr, &encode_error))
        {
            GLogError(u8"%s 编码失败：%s", label, encode_error.c_str());
            return false;
        }

        ShaderArtifactFileHeader header{};
        header.kind = kind;
        header.stage = stage;
        header.key_digest = key_digest;
        header.source_digest = source_digest;
        header.payload_size = static_cast<uint64>(payload.GetCount());
        header.payload_hash = ManifestHashBytes(payload.GetData(), header.payload_size);

        ValueArray<uint8> file_data;
        file_data.Resize(static_cast<int>(sizeof(header) + payload.GetCount()));
        std::memcpy(file_data.GetData(), &header, sizeof(header));
        if (!payload.IsEmpty())
            std::memcpy(file_data.GetData() + sizeof(header),
                        payload.GetData(),
                        static_cast<size_t>(payload.GetCount()));

        const int64 written = filesystem::SaveMemoryToFile(
            path, file_data.GetData(), static_cast<int64>(file_data.GetCount()));
        return written == static_cast<int64>(file_data.GetCount());
    }

    /// 读 path 并验完「头合法 + kind 匹配 + 文件长度 + payload 哈希 + FlatBuffers
    /// 解码 + format_version」后，把每条 entry 交给 acceptor。acceptor 返回
    /// false（条目不满足不变量）⇒ 整个文件判废。成功时 out_header 带回
    /// key_digest / source_digest 供调用方取戳。
    ///
    /// 磁盘上的东西不可信：任何一环失败都不产出半成品 —— 半个清单比没有清单更危险。
    template <typename PayloadT, typename Acceptor>
    bool ReadManifestArtifactFile(const OSString &path,
                                  ShaderArtifactKind kind,
                                  uint32 expected_format_version,
                                  PayloadT &decoded,
                                  ShaderArtifactFileHeader &out_header,
                                  Acceptor acceptor)
    {
        if (path.IsEmpty())
            return false;

        int64 file_size = 0;
        void *file_data = filesystem::LoadFileToMemory(path, file_size);
        if (!file_data)
            return false;

        bool success = false;
        do
        {
            if (file_size < static_cast<int64>(sizeof(ShaderArtifactFileHeader)))
                break;

            ShaderArtifactFileHeader header{};
            std::memcpy(&header, file_data, sizeof(header));

            if (!IsValidShaderArtifactFileHeader(header) || header.kind != kind)
                break;

            const uint64 expected_file_size =
                static_cast<uint64>(header.header_size) + header.payload_size;
            if (expected_file_size != static_cast<uint64>(file_size))
                break;

            const auto *payload = static_cast<const uint8 *>(file_data) + header.header_size;

            if (ManifestHashBytes(payload, header.payload_size) != header.payload_hash)
                break;

            if (!hgl::serialize::fb::VerifyAndDecodeFlatBuffers(
                    payload, static_cast<size_t>(header.payload_size), decoded))
                break;

            if (decoded.format_version != expected_format_version)
                break;

            // 逐条重验不变量；坏一条即整体判废。
            bool decode_ok = true;
            for (const auto &entry : decoded.entries)
            {
                if (!acceptor(entry))
                {
                    decode_ok = false;
                    break;
                }
            }

            if (!decode_ok)
                break;

            out_header = header;
            success = true;
        }
        while (false);

        // LoadFileToMemory 分配的是 char[]：配对 char* 释放（uint8* 双关为 UB）。
        delete[] static_cast<char *>(file_data);

        return success;
    }
}//namespace hgl::graph::mtl

#endif
