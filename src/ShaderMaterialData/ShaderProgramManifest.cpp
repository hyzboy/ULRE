#include <hgl/mtl/ShaderProgramManifest.h>

#include <hgl/asset/generated/ShaderCacheSerialize.gen.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/log/Log.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/utf.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::mtl
{
    void ShaderProgramManifest::Clear()
    {
        compiler_profile_hash = 0;
        codegen_generation = 0;
        entries.Clear();
    }

    const ShaderProgramManifestEntry *ShaderProgramManifest::Find(const uint64 lookup_key) const
    {
        if (entries.IsEmpty())
            return nullptr;

        const int count = entries.GetCount();
        const ShaderProgramManifestEntry *data = entries.GetData();
        for (int i = 0; i < count; ++i)
            if (data[i].lookup_key == lookup_key)
                return data + i;

        return nullptr;
    }

    bool ShaderProgramManifest::Upsert(const ShaderProgramManifestEntry &entry)
    {
        if (entry.lookup_key == 0 || !entry.program_link.IsValid())
            return false;

        // 行需求不变量：要 payload 行必然也要行表。非法组合来自上游把两个信号
        // 填反，放进来只会延迟到运行期变成难查的「读到陈旧 SSBO」。
        if (!entry.IsConsistent())
            return false;

        const int count = entries.GetCount();
        ShaderProgramManifestEntry *data = entries.GetData();
        for (int i = 0; i < count; ++i)
        {
            if (data[i].lookup_key != entry.lookup_key)
                continue;

            if (data[i] == entry)
                return false;

            data[i] = entry;
            return true;
        }

        entries.Add(entry);
        return true;
    }

    bool ShaderProgramManifest::Save(const OSString &path) const
    {
        if (path.IsEmpty())
            return false;

        filesystem::Path file_path(path);
        const OSString directory = file_path.GetParent().ToOSString();
        if (!directory.IsEmpty()
         && !filesystem::IsDirectory(directory)
         && !filesystem::MakePath(directory))
            return false;

        // 载荷 = 生成的反射表驱动的 FlatBuffers（真源是结构体本身，不再有第二份
        // 手写布局）。增删字段只改 ShaderProgramManifest.h 的结构并重跑生成器。
        ShaderProgramManifestPayload payload_struct;
        payload_struct.entries = entries;

        ValueArray<uint8> payload;
        AnsiString encode_error;
        if (!hgl::serialize::fb::EncodeToFlatBuffers(payload, payload_struct,
                                                     nullptr, &encode_error))
        {
            GLogError(u8"program manifest 编码失败：%s", encode_error.c_str());
            return false;
        }

        ShaderArtifactFileHeader header{};
        header.kind = ShaderArtifactKind::ProgramManifest;
        header.stage = 0;
        header.key_digest = compiler_profile_hash;
        header.source_digest = codegen_generation;
        header.payload_size = static_cast<uint64>(payload.GetCount());
        hgl::hash::FNV1aHasher64 hasher;
        hasher.AppendBytes(payload.GetData(), static_cast<size_t>(payload.GetCount()));
        header.payload_hash = hasher;

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

    bool ShaderProgramManifest::Load(const OSString &path)
    {
        Clear();

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

            if (!IsValidShaderArtifactFileHeader(header)
             || header.kind != ShaderArtifactKind::ProgramManifest)
                break;

            const uint64 expected_file_size =
                static_cast<uint64>(header.header_size) + header.payload_size;
            if (expected_file_size != static_cast<uint64>(file_size))
                break;

            const auto *payload = static_cast<const uint8 *>(file_data) + header.header_size;

            hgl::hash::FNV1aHasher64 hasher;
            hasher.AppendBytes(payload, static_cast<size_t>(header.payload_size));
            if (static_cast<uint64>(hasher) != header.payload_hash)
                break;

            ShaderProgramManifestPayload decoded;
            if (!hgl::serialize::fb::VerifyAndDecodeFlatBuffers(
                    payload, static_cast<size_t>(header.payload_size), decoded))
                break;

            if (decoded.format_version != ShaderProgramManifestFormatVersion)
                break;

            // 逐条重验不变量：磁盘上的东西不可信，半个清单比没有清单更危险。
            bool decode_ok = true;
            for (const auto &entry : decoded.entries)
            {
                if (entry.lookup_key == 0 || !entry.program_link.IsValid()
                 || !entry.IsConsistent())
                {
                    decode_ok = false;
                    break;
                }

                entries.Add(entry);
            }

            if (!decode_ok)
            {
                Clear();
                break;
            }

            compiler_profile_hash = header.key_digest;
            codegen_generation = static_cast<uint32>(header.source_digest);

            success = true;
        }
        while (false);

        delete[] static_cast<uint8 *>(file_data);

        if (!success)
            Clear();

        return success;
    }

    OSString MakeShaderProgramManifestPath(const OSString &cache_root)
    {
        if (cache_root.IsEmpty())
            return OSString();

        // 与 stage/ program/ 同级，整份 shader-cache 目录可以直接打包发行。
        filesystem::Path path(cache_root);
        path /= ToOSString(ShaderArtifactCacheDirectory);
        path /= ToOSString(ShaderArtifactManifestFilename);
        return path.ToOSString();
    }
}//namespace hgl::graph::mtl
