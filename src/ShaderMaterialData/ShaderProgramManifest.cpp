#include <hgl/mtl/ShaderProgramManifest.h>

#include "ShaderManifestFileCodec.h"

#include <hgl/asset/generated/ShaderCacheSerialize.gen.h>
#include <hgl/filesystem/Path.h>
#include <hgl/utf.h>

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
        // 键 0 = 「没有键」（与 compute 清单同口径），表内不可能存在，直接拒。
        if (lookup_key == 0 || entries.IsEmpty())
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
        // 载荷 = 生成的反射表驱动的 FlatBuffers（真源是结构体本身，不再有第二份
        // 手写布局）。增删字段只改 ShaderProgramManifest.h 的结构并重跑生成器。
        // 文件组装 / header / 哈希走两份清单共享的 codec（ShaderManifestFileCodec.h）。
        ShaderProgramManifestPayload payload_struct;
        payload_struct.entries = entries;

        return WriteManifestArtifactFile(path,
                                         ShaderArtifactKind::ProgramManifest,
                                         /*stage=*/0,
                                         compiler_profile_hash,
                                         codegen_generation,
                                         payload_struct,
                                         "program manifest");
    }

    bool ShaderProgramManifest::Load(const OSString &path)
    {
        Clear();

        ShaderProgramManifestPayload decoded;
        ShaderArtifactFileHeader header{};
        const bool ok = ReadManifestArtifactFile(
            path, ShaderArtifactKind::ProgramManifest, ShaderProgramManifestFormatVersion,
            decoded, header,
            [this](const ShaderProgramManifestEntry &entry) -> bool
            {
                if (entry.lookup_key == 0 || !entry.program_link.IsValid()
                 || !entry.IsConsistent())
                    return false;

                entries.Add(entry);
                return true;
            });

        if (ok)
        {
            compiler_profile_hash = header.key_digest;
            codegen_generation = static_cast<uint32>(header.source_digest);
        }

        if (!ok)
            Clear();

        return ok;
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
