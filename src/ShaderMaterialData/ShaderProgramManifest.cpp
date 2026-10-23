#include <hgl/mtl/ShaderProgramManifest.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/log/Log.h>
#include <hgl/utf.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        constexpr uint32 kManifestPayloadHeaderSize =
            sizeof(uint32) + sizeof(uint32);   // format_version + entry_count

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

        bool ReadU32(const uint8 *&cursor, const uint8 *end, uint32 &out_value)
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

        bool ReadU64(const uint8 *&cursor, const uint8 *end, uint64 &out_value)
        {
            uint32 low = 0;
            uint32 high = 0;
            if (!ReadU32(cursor, end, low) || !ReadU32(cursor, end, high))
                return false;
            out_value = uint64(low) | (uint64(high) << 32);
            return true;
        }

        void AppendStageKey(ValueArray<uint8> &bytes, const ShaderStageKey &key)
        {
            AppendU32(bytes, static_cast<uint32>(key.stage));
            AppendU64(bytes, key.definition_hash);
            AppendU64(bytes, key.glsl_module_graph_hash);
            AppendU64(bytes, key.interface_hash);
            AppendU64(bytes, key.resource_hash);
            AppendU64(bytes, key.compiler_hash);
        }

        bool ReadStageKey(const uint8 *&cursor, const uint8 *end, ShaderStageKey &out_key)
        {
            uint32 stage = 0;
            if (!ReadU32(cursor, end, stage))
                return false;

            out_key.stage = static_cast<ShaderStage>(stage);

            return ReadU64(cursor, end, out_key.definition_hash)
                && ReadU64(cursor, end, out_key.glsl_module_graph_hash)
                && ReadU64(cursor, end, out_key.interface_hash)
                && ReadU64(cursor, end, out_key.resource_hash)
                && ReadU64(cursor, end, out_key.compiler_hash);
        }

        bool SameEntry(
            const ShaderProgramManifestEntry &lhs,
            const ShaderProgramManifestEntry &rhs) noexcept
        {
            return lhs.lookup_key == rhs.lookup_key
                && lhs.program_link.mesh_stage == rhs.program_link.mesh_stage
                && lhs.program_link.fragment_stage == rhs.program_link.fragment_stage
                && lhs.program_link.resource_layout_hash == rhs.program_link.resource_layout_hash
                && lhs.program_link.vertex_input_hash == rhs.program_link.vertex_input_hash
                && lhs.program_link.render_target_hash == rhs.program_link.render_target_hash
                && lhs.program_link.compiler_hash == rhs.program_link.compiler_hash
                && lhs.requires_recipe_runtime_rows == rhs.requires_recipe_runtime_rows
                && lhs.requires_recipe_payload_row == rhs.requires_recipe_payload_row;
        }
    }//namespace

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

            if (SameEntry(data[i], entry))
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

        ValueArray<uint8> payload;
        payload.Reserve(static_cast<int>(
            kManifestPayloadHeaderSize
          + ShaderProgramManifestEntryPayloadSize * GetEntryCount()));

        AppendU32(payload, ShaderProgramManifestFormatVersion);
        AppendU32(payload, GetEntryCount());

        for (const auto &entry : entries)
        {
            AppendU64(payload, entry.lookup_key);
            AppendStageKey(payload, entry.program_link.mesh_stage);
            AppendStageKey(payload, entry.program_link.fragment_stage);
            AppendU64(payload, entry.program_link.resource_layout_hash);
            AppendU64(payload, entry.program_link.vertex_input_hash);
            AppendU64(payload, entry.program_link.render_target_hash);
            AppendU64(payload, entry.program_link.compiler_hash);
            AppendU32(payload, (entry.requires_recipe_runtime_rows
                ? ShaderProgramManifestFlagRequiresRecipeRows : 0u)
              | (entry.requires_recipe_payload_row
                ? ShaderProgramManifestFlagRequiresRecipePayloadRow : 0u));
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
            const uint8 *cursor = payload;
            const uint8 *end = payload + header.payload_size;

            hgl::hash::FNV1aHasher64 hasher;
            hasher.AppendBytes(payload, static_cast<size_t>(header.payload_size));
            if (static_cast<uint64>(hasher) != header.payload_hash)
                break;

            uint32 format_version = 0;
            uint32 entry_count = 0;
            if (!ReadU32(cursor, end, format_version)
             || !ReadU32(cursor, end, entry_count)
             || format_version != ShaderProgramManifestFormatVersion)
                break;

            const uint64 required_payload =
                static_cast<uint64>(kManifestPayloadHeaderSize)
              + static_cast<uint64>(entry_count) * ShaderProgramManifestEntryPayloadSize;
            if (required_payload != header.payload_size)
                break;

            compiler_profile_hash = header.key_digest;
            codegen_generation = static_cast<uint32>(header.source_digest);

            bool decode_ok = true;
            for (uint32 i = 0; i < entry_count && decode_ok; ++i)
            {
                ShaderProgramManifestEntry entry{};
                uint32 flags = 0;

                decode_ok =
                       ReadU64(cursor, end, entry.lookup_key)
                    && ReadStageKey(cursor, end, entry.program_link.mesh_stage)
                    && ReadStageKey(cursor, end, entry.program_link.fragment_stage)
                    && ReadU64(cursor, end, entry.program_link.resource_layout_hash)
                    && ReadU64(cursor, end, entry.program_link.vertex_input_hash)
                    && ReadU64(cursor, end, entry.program_link.render_target_hash)
                    && ReadU64(cursor, end, entry.program_link.compiler_hash)
                    && ReadU32(cursor, end, flags);

                if (!decode_ok)
                    break;

                entry.requires_recipe_runtime_rows =
                    (flags & ShaderProgramManifestFlagRequiresRecipeRows) != 0;
                entry.requires_recipe_payload_row =
                    (flags & ShaderProgramManifestFlagRequiresRecipePayloadRow) != 0;

                if (entry.lookup_key == 0 || !entry.program_link.IsValid())
                {
                    decode_ok = false;
                    break;
                }

                if (!entry.IsConsistent())
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
