// ULRE.ShaderMaterialData —— compute 程序清单（数据面）。
//
// 与 ShaderProgramManifest.cpp 同一套磁盘机制（AssetGen 反射表驱动的
// FlatBuffers 载荷 + 同一份 ShaderArtifactFileHeader），差别只在记录形状
// 和键的来源。
#include <hgl/mtl/ShaderComputeProgramManifest.h>

#include <hgl/asset/generated/ShaderCacheSerialize.gen.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/ShaderArtifactStore.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/utf.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
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

        // 载荷 = 生成的反射表驱动的 FlatBuffers（与 program 清单同一套机制）。
        ShaderComputeProgramManifestPayload payload_struct;
        payload_struct.entries = entries;

        ValueArray<uint8> payload;
        AnsiString encode_error;
        if (!hgl::serialize::fb::EncodeToFlatBuffers(payload, payload_struct,
                                                     nullptr, &encode_error))
        {
            GLogError(u8"compute manifest 编码失败：%s", encode_error.c_str());
            return false;
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

            if (HashBytes(payload, header.payload_size) != header.payload_hash)
                break;

            ShaderComputeProgramManifestPayload decoded;
            if (!hgl::serialize::fb::VerifyAndDecodeFlatBuffers(
                    payload, static_cast<size_t>(header.payload_size), decoded))
                break;

            if (decoded.format_version != ShaderComputeProgramManifestFormatVersion)
                break;

            // 逐条重验不变量：磁盘上的东西不可信，半个清单比没有清单更危险。
            bool decode_ok = true;
            for (const auto &entry : decoded.entries)
            {
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
        // compute 没有声明闭包，这四个字段不表示「声明哈希」，只表示
        // 「这段 SPV 是哪一份」。填法：名字定身份、内容定版本、编译器定兼容性，
        // 于是「改了 GLSL 但名字没变」自然换文件名，旧文件退场。
        //
        // 字段语义按 stage 键的统一含义重新对齐：
        //   definition_hash     = 名字键（没有 GLSL 文本，名字就是定义身份）
        //   compile_inputs_hash = SPV 内容哈希（本 stage 的编译产物身份）
        //   interface_hash      = SPV 字节数 + 1（0 值在别的键位上代表「没填」）
        //   compiler_hash       = 编译器 profile 哈希
        // 原先把 compiler_hash 同时塞给 interface_hash，等于让两个维度退化成一个；
        // 现在 interface 取字节数，纯粹用于把同名不同体积的旧产物错开。
        ShaderStageKey key{};

        key.stage = ShaderStage::Compute;
        key.definition_hash = ComputeShaderComputeProgramManifestKey(program_name);
        key.compile_inputs_hash = HashBytes(spv_data, spv_size);
        key.interface_hash = spv_size + 1;
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
            && manifest.GetCodegenGeneration() != contract::MATERIAL_CODEGEN_GENERATION)
        {
            GLogWarning("[ShaderComputeManifest] codegen generation %u != %u; rebuilding manifest",
                        manifest.GetCodegenGeneration(),
                        contract::MATERIAL_CODEGEN_GENERATION);
            manifest.Clear();
        }

        manifest.SetStamps(effective_compiler_hash, contract::MATERIAL_CODEGEN_GENERATION);

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

        if (manifest.GetCodegenGeneration() != contract::MATERIAL_CODEGEN_GENERATION)
        {
            GLogError("[ShaderComputeManifest] codegen generation mismatch: manifest=%u runtime=%u (%s)",
                      manifest.GetCodegenGeneration(),
                      contract::MATERIAL_CODEGEN_GENERATION,
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
