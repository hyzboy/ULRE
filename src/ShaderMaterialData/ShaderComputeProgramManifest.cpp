// ULRE.ShaderMaterialData —— compute 程序清单（数据面）。
//
// 与 ShaderProgramManifest.cpp 同一套磁盘机制（小端手写序列化 + 同一份
// ShaderArtifactFileHeader），差别只在记录形状和键的来源。
#include <hgl/mtl/ShaderComputeProgramManifest.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/ShaderArtifactStore.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
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

        uint64 HashBytes(const void *data, const uint64 byte_size)
        {
            hgl::hash::FNV1aHasher64 hasher;
            if (data && byte_size > 0)
                hasher.AppendBytes(data, static_cast<size_t>(byte_size));
            return static_cast<uint64>(hasher);
        }
    }//namespace

    void ShaderComputeProgramManifest::Clear()
    {
        compiler_profile_hash = 0;
        codegen_generation = 0;
        entries.Clear();
    }

    const ShaderComputeProgramManifestEntry *
    ShaderComputeProgramManifest::Find(const uint64 program_key) const
    {
        if (entries.IsEmpty() || program_key == 0)
            return nullptr;

        const int count = entries.GetCount();
        const ShaderComputeProgramManifestEntry *data = entries.GetData();
        for (int i = 0; i < count; ++i)
            if (data[i].program_key == program_key)
                return data + i;

        return nullptr;
    }

    bool ShaderComputeProgramManifest::Upsert(const ShaderComputeProgramManifestEntry &entry)
    {
        if (!entry.IsValid())
            return false;

        const int count = entries.GetCount();
        ShaderComputeProgramManifestEntry *data = entries.GetData();
        for (int i = 0; i < count; ++i)
        {
            if (data[i].program_key != entry.program_key)
                continue;

            if (data[i] == entry)
                return false;

            data[i] = entry;
            return true;
        }

        entries.Add(entry);
        return true;
    }

    bool ShaderComputeProgramManifest::Save(const OSString &path) const
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
          + ShaderComputeProgramManifestEntryPayloadSize * GetEntryCount()));

        AppendU32(payload, ShaderComputeProgramManifestFormatVersion);
        AppendU32(payload, GetEntryCount());

        for (const auto &entry : entries)
        {
            AppendU64(payload, entry.program_key);
            AppendStageKey(payload, entry.compute_stage);
            AppendU32(payload, entry.push_constant_size);
            AppendU32(payload, 0u);   // flags：保留
        }

        ShaderArtifactFileHeader header{};
        header.kind = ShaderArtifactKind::ComputeProgramManifest;
        header.stage = static_cast<uint32>(ShaderStage::Compute);
        header.key_digest = compiler_profile_hash;
        header.source_digest = codegen_generation;
        header.payload_size = static_cast<uint64>(payload.GetCount());
        header.payload_hash = HashBytes(payload.GetData(), header.payload_size);

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

    bool ShaderComputeProgramManifest::Load(const OSString &path)
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
             || header.kind != ShaderArtifactKind::ComputeProgramManifest)
                break;

            const uint64 expected_file_size =
                static_cast<uint64>(header.header_size) + header.payload_size;
            if (expected_file_size != static_cast<uint64>(file_size))
                break;

            const auto *payload = static_cast<const uint8 *>(file_data) + header.header_size;
            const uint8 *cursor = payload;
            const uint8 *end = payload + header.payload_size;

            if (HashBytes(payload, header.payload_size) != header.payload_hash)
                break;

            uint32 format_version = 0;
            uint32 entry_count = 0;
            if (!ReadU32(cursor, end, format_version)
             || !ReadU32(cursor, end, entry_count)
             || format_version != ShaderComputeProgramManifestFormatVersion)
                break;

            const uint64 required_payload =
                static_cast<uint64>(kManifestPayloadHeaderSize)
              + static_cast<uint64>(entry_count) * ShaderComputeProgramManifestEntryPayloadSize;
            if (required_payload != header.payload_size)
                break;

            compiler_profile_hash = header.key_digest;
            codegen_generation = static_cast<uint32>(header.source_digest);

            bool decode_ok = true;
            for (uint32 i = 0; i < entry_count && decode_ok; ++i)
            {
                ShaderComputeProgramManifestEntry entry{};
                uint32 flags = 0;

                decode_ok =
                       ReadU64(cursor, end, entry.program_key)
                    && ReadStageKey(cursor, end, entry.compute_stage)
                    && ReadU32(cursor, end, entry.push_constant_size)
                    && ReadU32(cursor, end, flags);

                if (!decode_ok)
                    break;

                if (!entry.IsValid())
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

    uint64 ComputeShaderComputeProgramManifestKey(const char *program_name)
    {
        if (!program_name || !*program_name)
            return 0;

        hgl::hash::FNV1aHasher64 hasher;
        hasher.AppendBytes(program_name, std::strlen(program_name));

        const uint64 key = static_cast<uint64>(hasher);
        return key ? key : 1;   // 0 保留给「没有键」，撞上就挪一格
    }

    ShaderStageKey MakeComputeStageKey(
        const char *program_name,
        const void *spv_data,
        const uint64 spv_size,
        const uint64 compiler_hash)
    {
        // compute 没有声明闭包，这五个字段不表示「声明哈希」，只表示
        // 「这段 SPV 是哪一份」。填法：名字定身份、内容定版本、编译器定兼容性，
        // 于是「改了 GLSL 但名字没变」自然换文件名，旧文件退场。
        ShaderStageKey key{};

        key.stage = ShaderStage::Compute;
        key.definition_hash = ComputeShaderComputeProgramManifestKey(program_name);
        key.glsl_module_graph_hash = HashBytes(spv_data, spv_size);
        key.interface_hash = compiler_hash;
        key.resource_hash = spv_size + 1;   // +1：0 值在别的键位上代表「没填」
        key.compiler_hash = compiler_hash;

        return key;
    }

    OSString MakeShaderComputeProgramManifestPath(const OSString &cache_root)
    {
        if (cache_root.IsEmpty())
            return OSString();

        filesystem::Path path(cache_root);
        path /= ToOSString(ShaderArtifactCacheDirectory);
        path /= ToOSString(ShaderArtifactComputeManifestFilename);
        return path.ToOSString();
    }

    bool RecordShaderComputeProgramArtifact(
        ShaderArtifactStore &store,
        const char *program_name,
        const void *spv_data,
        const uint64 spv_size,
        const uint32 push_constant_size,
        const uint64 compiler_hash)
    {
        if (store.GetCacheMode() == ShaderCacheMode::ReadOnly)
            return false;

        const uint64 program_key = ComputeShaderComputeProgramManifestKey(program_name);
        if (program_key == 0 || !spv_data || spv_size == 0)
            return false;

        const uint64 effective_compiler_hash = compiler_hash
            ? compiler_hash
            : contract::GetShaderCompilerProfileHash(nullptr);

        ShaderComputeProgramManifestEntry entry{};
        entry.program_key = program_key;
        entry.compute_stage = MakeComputeStageKey(
            program_name, spv_data, spv_size, effective_compiler_hash);
        entry.push_constant_size = push_constant_size;

        if (!entry.IsValid())
        {
            GLogError("[ShaderComputeManifest] invalid entry for %s (push constant %u)",
                      program_name, push_constant_size);
            return false;
        }

        if (!store.SaveStageSPV(entry.compute_stage, spv_data, spv_size))
        {
            GLogError("[ShaderComputeManifest] failed to save compute SPV for %s", program_name);
            return false;
        }

        const OSString path = MakeShaderComputeProgramManifestPath(store.GetRootPath());

        ShaderComputeProgramManifest manifest;
        // 读不到不算错：第一次烘焙时文件还不存在。代际不符则整体重建 ——
        // 旧代数的条目与当前 layout 约定不保证兼容。
        if (manifest.Load(path)
            && manifest.GetCodegenGeneration() != contract::SHADERGEN_CODEGEN_GENERATION)
        {
            GLogWarning("[ShaderComputeManifest] codegen generation %u != %u; rebuilding manifest",
                        manifest.GetCodegenGeneration(),
                        contract::SHADERGEN_CODEGEN_GENERATION);
            manifest.Clear();
        }

        manifest.SetStamps(effective_compiler_hash, contract::SHADERGEN_CODEGEN_GENERATION);

        if (!manifest.Upsert(entry))
            return true;   // 原样命中，不需要重写

        if (!manifest.Save(path))
        {
            GLogError("[ShaderComputeManifest] failed to save %s",
                      ToU8String(path).c_str());
            return false;
        }

        return true;
    }

    bool LoadShaderComputeProgramArtifact(
        ShaderArtifactStore &store,
        const char *program_name,
        ValueArray<uint8> &out_spv,
        uint32 *out_push_constant_size)
    {
        out_spv.Clear();
        if (out_push_constant_size)
            *out_push_constant_size = 0;

        const uint64 program_key = ComputeShaderComputeProgramManifestKey(program_name);
        if (program_key == 0)
            return false;

        const OSString path = MakeShaderComputeProgramManifestPath(store.GetRootPath());

        ShaderComputeProgramManifest manifest;
        if (!manifest.Load(path))
        {
            GLogError("[ShaderComputeManifest] cannot load %s", ToU8String(path).c_str());
            return false;
        }

        if (manifest.GetCodegenGeneration() != contract::SHADERGEN_CODEGEN_GENERATION)
        {
            GLogError("[ShaderComputeManifest] codegen generation mismatch: manifest=%u runtime=%u (%s)",
                      manifest.GetCodegenGeneration(),
                      contract::SHADERGEN_CODEGEN_GENERATION,
                      program_name);
            return false;
        }

        const ShaderComputeProgramManifestEntry *entry = manifest.Find(program_key);
        if (!entry)
        {
            GLogError("[ShaderComputeManifest] no baked artifact for compute program '%s'",
                      program_name);
            return false;
        }

        if (!store.LoadStageSPV(entry->compute_stage, out_spv))
        {
            GLogError("[ShaderComputeManifest] baked SPV missing or corrupt for compute program '%s'",
                      program_name);
            return false;
        }

        if (out_push_constant_size)
            *out_push_constant_size = entry->push_constant_size;

        return true;
    }
}//namespace hgl::graph::mtl
