#include <hgl/shadergen/ShaderArtifactStore.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/type/StrChar.h>
#include <hgl/utf.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
    namespace
    {
        OSString MakeStageFilename(const ShaderStageKey &key)
        {
            const AnsiString key_name = key.ToString();
            return ToOSString(key_name.c_str()) + OS_TEXT(".spv");
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

        void AppendU32(ValueArray<uint8> &bytes, const uint32 value)
        {
            bytes.Add(static_cast<uint8>(value));
            bytes.Add(static_cast<uint8>(value >> 8));
            bytes.Add(static_cast<uint8>(value >> 16));
            bytes.Add(static_cast<uint8>(value >> 24));
        }

        void AppendU64(ValueArray<uint8> &bytes, const uint64 value)
        {
            AppendU32(bytes, static_cast<uint32>(value));
            AppendU32(bytes, static_cast<uint32>(value >> 32));
        }

        bool ReadU32(
            const uint8 *&cursor,
            const uint8 *end,
            uint32 &out_value)
        {
            if (!cursor || end - cursor < 4)
                return false;
            out_value = uint32(cursor[0])
                | (uint32(cursor[1]) << 8)
                | (uint32(cursor[2]) << 16)
                | (uint32(cursor[3]) << 24);
            cursor += 4;
            return true;
        }

        bool ReadU64(
            const uint8 *&cursor,
            const uint8 *end,
            uint64 &out_value)
        {
            uint32 low = 0;
            uint32 high = 0;
            if (!ReadU32(cursor, end, low)
             || !ReadU32(cursor, end, high))
                return false;
            out_value = uint64(low) | (uint64(high) << 32);
            return true;
        }

        void SerializeProgramMetadata(
            const ShaderProgramArtifactMetadata &metadata,
            ValueArray<uint8> &out_payload)
        {
            out_payload.Clear();
            out_payload.Reserve(ShaderProgramMetadataPayloadSize);
            AppendU64(out_payload, metadata.program_key_digest);
            AppendU64(out_payload, metadata.resolved_module_graph_hash);
            AppendU64(out_payload, metadata.shader_interface_hash);
            AppendU64(out_payload, metadata.output_contract_hash);
            AppendU64(out_payload, metadata.vertex_stage_digest);
            AppendU64(out_payload, metadata.fragment_stage_digest);
            AppendU64(out_payload, metadata.compiler_profile_hash);
            AppendU64(out_payload, metadata.device_target_hash);
            AppendU64(out_payload, metadata.generated_source_digest);
        }

        bool DeserializeProgramMetadata(
            const uint8 *payload,
            const uint64 payload_size,
            ShaderProgramArtifactMetadata &out_metadata)
        {
            out_metadata = {};
            if (!payload
             || payload_size != ShaderProgramMetadataPayloadSize)
                return false;

            const uint8 *cursor = payload;
            const uint8 *end = payload + payload_size;
            return ReadU64(cursor, end, out_metadata.program_key_digest)
                && ReadU64(
                    cursor,
                    end,
                    out_metadata.resolved_module_graph_hash)
                && ReadU64(cursor, end, out_metadata.shader_interface_hash)
                && ReadU64(cursor, end, out_metadata.output_contract_hash)
                && ReadU64(cursor, end, out_metadata.vertex_stage_digest)
                && ReadU64(cursor, end, out_metadata.fragment_stage_digest)
                && ReadU64(cursor, end, out_metadata.compiler_profile_hash)
                && ReadU64(cursor, end, out_metadata.device_target_hash)
                && ReadU64(cursor, end, out_metadata.generated_source_digest)
                && cursor == end;
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

    bool ShaderArtifactStore::LoadStageSPV(const ShaderStageKey &key, ValueArray<uint8> &out_spv) const
    {
        out_spv.Clear();

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
                                           const uint64 spv_size)
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
             || header.payload_size
                != ShaderProgramMetadataPayloadSize
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
             || !DeserializeProgramMetadata(
                    payload,
                    header.payload_size,
                    out_metadata)
             || !IsValidShaderProgramArtifactMetadata(out_metadata)
             || out_metadata.program_key_digest
                    != program_key.GetDigest()
             || out_metadata.vertex_stage_digest
                    != link.vertex_stage.GetDigest()
             || out_metadata.fragment_stage_digest
                    != link.fragment_stage.GetDigest()
             || out_metadata.compiler_profile_hash
                    != link.compiler_hash)
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
         || metadata.program_key_digest != link.BuildKey().GetDigest()
         || metadata.vertex_stage_digest
            != link.vertex_stage.GetDigest()
         || metadata.fragment_stage_digest
            != link.fragment_stage.GetDigest()
         || metadata.compiler_profile_hash != link.compiler_hash)
            return false;

        const OSString directory =
            MakeProgramDirectory(root_path);
        if (directory.IsEmpty())
            return false;
        if (!filesystem::IsDirectory(directory)
         && !filesystem::MakePath(directory))
            return false;

        ValueArray<uint8> payload;
        SerializeProgramMetadata(metadata, payload);
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
        ValueArray<uint8> &out_fragment_spv) const
    {
        out_vertex_spv.Clear();
        out_fragment_spv.Clear();

        ShaderProgramArtifactMetadata stored_metadata{};
        if (!LoadProgramMetadata(link, stored_metadata)
         || !(stored_metadata == expected_metadata)
         || !LoadStageSPV(link.vertex_stage, out_vertex_spv)
         || !LoadStageSPV(link.fragment_stage, out_fragment_spv))
        {
            out_vertex_spv.Clear();
            out_fragment_spv.Clear();
            return false;
        }
        return true;
    }
}
