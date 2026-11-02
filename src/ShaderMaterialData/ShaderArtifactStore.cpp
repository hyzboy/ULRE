#include <hgl/mtl/ShaderArtifactStore.h>

#include <hgl/asset/generated/ShaderCacheSerialize.gen.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/log/Log.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/type/StrChar.h>
#include <hgl/utf.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        // mesh shader 材质：顶点处理 stage 是 Mesh
        const ShaderStageKey &GetMeshStageKey(const ShaderLinkSpec &link)
        {
            return link.mesh_stage;
        }

        OSString MakeStageFilename(const ShaderStageKey &key)
        {
            const AnsiString key_name = key.ToString();
            return ToOSString(key_name.c_str()) + ToOSString(ShaderArtifactSPVExtension);
        }

        OSString MakeStageSourceFilename(const ShaderStageKey &key)
        {
            const char *extension = GetShaderArtifactStageSourceExtension(key.stage);
            if (!extension || !extension[0])
                return {};

            const AnsiString key_name = key.ToString();
            return ToOSString(key_name.c_str()) + ToOSString(extension);
        }

        OSString MakeStageDirectory(
            const OSString &root)
        {
            filesystem::Path path(root);
            path /= ToOSString(ShaderArtifactCacheDirectory);
            path /= ToOSString(ShaderArtifactStageDirectory);
            return path.ToOSString();
        }

        OSString MakeProgramDirectory(
            const OSString &root)
        {
            filesystem::Path path(root);
            path /= ToOSString(ShaderArtifactCacheDirectory);
            path /= ToOSString(ShaderArtifactProgramDirectory);
            return path.ToOSString();
        }

        bool EncodeProgramMetadata(
            const ShaderProgramArtifactMetadata &metadata,
            ValueArray<uint8> &out_payload)
        {
            AnsiString encode_error;
            if (!hgl::serialize::fb::EncodeToFlatBuffers(
                    out_payload, metadata, nullptr, &encode_error))
            {
                GLogError(u8"program metadata 编码失败：%s", encode_error.c_str());
                return false;
            }
            return true;
        }

        bool DecodeProgramMetadata(
            const void *payload,
            const uint64 payload_size,
            ShaderProgramArtifactMetadata &out_metadata)
        {
            out_metadata = {};
            if (!payload || payload_size == 0)
                return false;

            return hgl::serialize::fb::VerifyAndDecodeFlatBuffers(
                payload, static_cast<size_t>(payload_size), out_metadata);
        }

        bool IsValidSPVPayload(
            const void *data,
            const uint64 byte_size)
        {
            if (!data
             || byte_size < sizeof(uint32)
             || byte_size % sizeof(uint32) != 0
             || byte_size > static_cast<uint64>(0x7fffffff))
                return false;

            uint32 magic = 0;
            std::memcpy(&magic, data, sizeof(magic));
            return magic == ShaderArtifactSPVMagic;
        }
    }

    OSString ShaderArtifactStore::GetStagePath(const ShaderStageKey &key) const
    {
        const OSString directory = MakeStageDirectory(root_path);
        if (directory.IsEmpty())
            return {};

        filesystem::Path path(directory);
        path /= MakeStageFilename(key);
        return path.ToOSString();
    }

    OSString ShaderArtifactStore::GetProgramPath(
        const ShaderProgramKey &key) const
    {
        const OSString directory =
            MakeProgramDirectory(root_path);
        if (directory.IsEmpty())
            return {};

        filesystem::Path path(directory);
        path /= ToOSString(key.ToString().c_str())
            + ToOSString(ShaderArtifactMetadataExtension);
        return path.ToOSString();
    }

    bool ShaderArtifactStore::LoadStageSPV(
        const ShaderStageKey &key,
        ValueArray<uint8> &out_spv,
        uint64 *out_source_digest) const
    {
        out_spv.Clear();
        if (out_source_digest)
            *out_source_digest = 0;

        int64 file_size = 0;
        void *file_data = filesystem::LoadFileToMemory(GetStagePath(key), file_size);
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
             || header.kind != ShaderArtifactKind::StageSPV
             || header.stage != static_cast<uint32>(key.stage)
             || header.key_digest != key.GetDigest())
                break;

            const uint64 expected_file_size = static_cast<uint64>(header.header_size) + header.payload_size;
            if (expected_file_size != static_cast<uint64>(file_size)
             || header.payload_size == 0
             || header.payload_size > static_cast<uint64>(0x7fffffff))
                break;

            const auto *payload = static_cast<const uint8 *>(file_data) + header.header_size;
            hgl::hash::FNV1aHasher64 hasher;
            hasher.AppendBytes(payload, static_cast<size_t>(header.payload_size));
            const uint64 payload_hash = hasher;
            if (payload_hash != header.payload_hash
             || !IsValidSPVPayload(payload, header.payload_size))
                break;

            out_spv.Resize(static_cast<int>(header.payload_size));
            std::memcpy(out_spv.GetData(), payload, static_cast<size_t>(header.payload_size));
            if (out_source_digest)
                *out_source_digest = header.source_digest;
            success = true;
        }
        while (false);

        delete[] static_cast<uint8 *>(file_data);
        if (!success)
            out_spv.Clear();

        return success;
    }

    bool ShaderArtifactStore::SaveStageSPV(const ShaderStageKey &key,
                                           const void *spv_data,
                                           const uint64 spv_size,
                                           const uint64 source_digest)
    {
        if (cache_mode == ShaderCacheMode::ReadOnly
         || !IsValidSPVPayload(spv_data, spv_size))
            return false;

        const OSString directory = MakeStageDirectory(root_path);
        if (directory.IsEmpty())
            return false;

        if (!filesystem::IsDirectory(directory)
         && !filesystem::MakePath(directory))
            return false;

        ShaderArtifactFileHeader header{};
        header.kind = ShaderArtifactKind::StageSPV;
        header.stage = static_cast<uint32>(key.stage);
        header.key_digest = key.GetDigest();
        header.payload_size = spv_size;
        header.source_digest = source_digest;
        hgl::hash::FNV1aHasher64 hasher;
        hasher.AppendBytes(spv_data, static_cast<size_t>(spv_size));
        header.payload_hash = hasher;

        ValueArray<uint8> file_data;
        file_data.Resize(static_cast<int>(sizeof(header) + spv_size));
        std::memcpy(file_data.GetData(), &header, sizeof(header));
        std::memcpy(file_data.GetData() + sizeof(header), spv_data, static_cast<size_t>(spv_size));

        const int64 written = filesystem::SaveMemoryToFile(
            GetStagePath(key), file_data.GetData(), static_cast<int64>(file_data.GetCount()));
        return written == static_cast<int64>(file_data.GetCount());
    }

    bool ShaderArtifactStore::SaveStageGLSL(const ShaderStageKey &key,
                                            const void *glsl_text,
                                            const uint64 byte_size)
    {
        if (cache_mode == ShaderCacheMode::ReadOnly
         || !glsl_text
         || byte_size == 0)
            return false;

        const OSString directory = MakeStageDirectory(root_path);
        if (directory.IsEmpty())
            return false;

        if (!filesystem::IsDirectory(directory)
         && !filesystem::MakePath(directory))
            return false;

        const OSString file_name = MakeStageSourceFilename(key);
        if (file_name.IsEmpty())
            return false;

        filesystem::Path path(directory);
        path /= file_name;
        const OSString file_path = path.ToOSString();

        const int64 written = filesystem::SaveMemoryToFile(
            file_path, glsl_text, static_cast<int64>(byte_size));
        if (written != static_cast<int64>(byte_size))
        {
            GLogWarning(u8"[ShaderArtifactStore] failed to cache stage GLSL source: %s",
                        ToU8String(file_path).c_str());
            return false;
        }

        GLogInfo(u8"[ShaderArtifactStore] stage GLSL source cached: %s",
                 ToU8String(file_path).c_str());
        return true;
    }

    bool ShaderArtifactStore::HasProgramMetadata(
        const ShaderLinkSpec &link) const
    {
        return link.IsValid()
            && filesystem::FileExist(GetProgramPath(link.BuildKey()));
    }

    bool ShaderArtifactStore::LoadProgramMetadata(
        const ShaderLinkSpec &link,
        ShaderProgramArtifactMetadata &out_metadata) const
    {
        out_metadata = {};
        if (!link.IsValid())
            return false;

        int64 file_size = 0;
        void *file_data = filesystem::LoadFileToMemory(
            GetProgramPath(link.BuildKey()), file_size);
        if (!file_data)
            return false;

        bool success = false;
        do
        {
            if (file_size
                < static_cast<int64>(sizeof(ShaderArtifactFileHeader)))
                break;

            ShaderArtifactFileHeader header{};
            std::memcpy(&header, file_data, sizeof(header));
            const ShaderProgramKey program_key = link.BuildKey();
            if (!IsValidShaderArtifactFileHeader(header)
             || header.kind != ShaderArtifactKind::ProgramMetadata
             || header.stage != 0
             || header.key_digest != program_key.GetDigest()
             || header.payload_size == 0
             || uint64(header.header_size) + header.payload_size
                != static_cast<uint64>(file_size))
                break;

            const auto *payload =
                static_cast<const uint8 *>(file_data)
                + header.header_size;
            hgl::hash::FNV1aHasher64 hasher;
            hasher.AppendBytes(payload, static_cast<size_t>(header.payload_size));
            const uint64 payload_hash = hasher;
            if (payload_hash != header.payload_hash
             || !DecodeProgramMetadata(
                    payload,
                    header.payload_size,
                    out_metadata)
             || !IsValidShaderProgramArtifactMetadata(out_metadata)
             || out_metadata.program_key_digest
                    != program_key.GetDigest())
                break;

            success = true;
        }
        while (false);

        delete[] static_cast<uint8 *>(file_data);
        if (!success)
            out_metadata = {};
        return success;
    }

    bool ShaderArtifactStore::SaveProgramMetadata(
        const ShaderLinkSpec &link,
        const ShaderProgramArtifactMetadata &metadata)
    {
        if (cache_mode == ShaderCacheMode::ReadOnly
         || !link.IsValid()
         || !IsValidShaderProgramArtifactMetadata(metadata)
         || metadata.program_key_digest != link.BuildKey().GetDigest())
            return false;

        const OSString directory =
            MakeProgramDirectory(root_path);
        if (directory.IsEmpty())
            return false;
        if (!filesystem::IsDirectory(directory)
         && !filesystem::MakePath(directory))
            return false;

        ValueArray<uint8> payload;
        if (!EncodeProgramMetadata(metadata, payload))
            return false;
        ShaderArtifactFileHeader header{};
        header.kind = ShaderArtifactKind::ProgramMetadata;
        header.stage = 0;
        header.key_digest = link.BuildKey().GetDigest();
        header.payload_size = payload.GetCount();
        hgl::hash::FNV1aHasher64 hasher;
        hasher.AppendBytes(payload.GetData(), static_cast<size_t>(payload.GetCount()));
        header.payload_hash = hasher;

        ValueArray<uint8> file_data;
        file_data.Resize(
            static_cast<int>(sizeof(header)) + payload.GetCount());
        std::memcpy(file_data.GetData(), &header, sizeof(header));
        std::memcpy(
            file_data.GetData() + sizeof(header),
            payload.GetData(),
            static_cast<size_t>(payload.GetCount()));
        const int64 written = filesystem::SaveMemoryToFile(
            GetProgramPath(link.BuildKey()),
            file_data.GetData(),
            file_data.GetCount());
        return written == file_data.GetCount();
    }

    bool ShaderArtifactStore::LoadProgramArtifacts(
        const ShaderLinkSpec &link,
        const ShaderProgramArtifactMetadata &expected_metadata,
        ValueArray<uint8> &out_vertex_spv,
        ValueArray<uint8> &out_fragment_spv,
        uint64 *out_stored_source_digest) const
    {
        out_vertex_spv.Clear();
        out_fragment_spv.Clear();
        if (out_stored_source_digest)
            *out_stored_source_digest = 0;

        ShaderProgramArtifactMetadata stored_metadata{};
        if (!LoadProgramMetadata(link, stored_metadata)
         || !(stored_metadata == expected_metadata)
         || !LoadStageSPV(GetMeshStageKey(link), out_vertex_spv)
         || !LoadStageSPV(link.fragment_stage, out_fragment_spv))
        {
            out_vertex_spv.Clear();
            out_fragment_spv.Clear();
            return false;
        }

        // 命中判定不看文本摘要（见 ShaderArtifactContract.h 的 operator==），
        // 但把落盘时的值交给调用方做命中后校验。
        if (out_stored_source_digest)
            *out_stored_source_digest = stored_metadata.generated_source_digest;
        return true;
    }
}
