#include <hgl/shadergen/ShaderArtifactStore.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/type/StrChar.h>
#include <hgl/utf.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        OSString MakeStageFilename(const ShaderStageKey &key)
        {
            const AnsiString key_name = key.ToString();
            return ToOSString(key_name.c_str()) + OS_TEXT(".spv");
        }

        OSString MakeStageDirectory(
            const OSString &root,
            const ShaderArtifactCacheNamespace cache_namespace)
        {
            if (!IsValidShaderArtifactCacheNamespace(cache_namespace))
                return {};

            filesystem::Path path(root);
            path /= ToOSString(ShaderArtifactCacheDirectory);

            const char *namespace_directory =
                GetShaderArtifactCacheNamespaceDirectory(cache_namespace);
            if (namespace_directory)
                path /= ToOSString(namespace_directory);

            path /= ToOSString(ShaderArtifactStageDirectory);
            return path.ToOSString();
        }
    }

    OSString ShaderArtifactStore::GetStagePath(const ShaderStageKey &key) const
    {
        const OSString directory = MakeStageDirectory(root_path, cache_namespace);
        if (directory.IsEmpty())
            return {};

        filesystem::Path path(directory);
        path /= MakeStageFilename(key);
        return path.ToOSString();
    }

    bool ShaderArtifactStore::HasStageSPV(const ShaderStageKey &key) const
    {
        return filesystem::FileExist(GetStagePath(key));
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
            const uint64 payload_hash = hgl::hash::FNV1aAppendBytes(
                hgl::hash::FNV1aInit<uint64>(), payload, static_cast<size_t>(header.payload_size));
            if (payload_hash != header.payload_hash)
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
         || !spv_data
         || spv_size == 0
         || spv_size > static_cast<uint64>(0x7fffffff))
            return false;

        const OSString directory = MakeStageDirectory(root_path, cache_namespace);
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
        header.payload_hash = hgl::hash::FNV1aAppendBytes(
            hgl::hash::FNV1aInit<uint64>(), spv_data, static_cast<size_t>(spv_size));

        ValueArray<uint8> file_data;
        file_data.Resize(static_cast<int>(sizeof(header) + spv_size));
        std::memcpy(file_data.GetData(), &header, sizeof(header));
        std::memcpy(file_data.GetData() + sizeof(header), spv_data, static_cast<size_t>(spv_size));

        const int64 written = filesystem::SaveMemoryToFile(
            GetStagePath(key), file_data.GetData(), static_cast<int64>(file_data.GetCount()));
        return written == static_cast<int64>(file_data.GetCount());
    }
}
