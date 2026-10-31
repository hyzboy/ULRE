#include <hgl/graph/asset/MaterialDataBlockRegistry.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/MaterialDataBlockFlat.h>
#include <hgl/type/Smart.h>
#include <hgl/type/ValueArray.h>
#include <hgl/utf.h>

#include <climits>

namespace hgl::graph::mtl
{
    namespace
    {
        bool IsTomlFile(const OSString &name)
        {
            return name.EndsWith(OS_TEXT(".material_data.toml"))
                || name.EndsWith(OS_TEXT(".mtldata.toml"));
        }

        bool IsMdbFile(const OSString &name)
        {
            return name.EndsWith(OS_TEXT(".material_data.mdb"))
                || name.EndsWith(OS_TEXT(".mtldata.mdb"))
                || name.EndsWith(OS_TEXT(".mdb"));
        }

        bool IsFlatBufferFile(const OSString &name)
        {
            return name.EndsWith(OS_TEXT(".material_data.fb"))
                || name.EndsWith(OS_TEXT(".mtldata.fb"));
        }

        bool IsAnyFile(const OSString &name)
        {
            return IsTomlFile(name) || IsMdbFile(name) || IsFlatBufferFile(name);
        }

        /// 解析失败时给出「人类可读且带定位」的原因（写进句柄的 `failure_reason`）
        AnsiString MakeFailureReason(const char *stage,
                                     const MaterialDataBlockParseResult result,
                                     const MaterialDataBlockParseError &error)
        {
            AnsiString reason(stage);
            reason += ": ";
            reason += GetMaterialDataBlockParseResultName(result);

            if (error.line > 0)
                reason += AnsiString(" line=") + AnsiString::numberOf(error.line);
            if (!error.key.IsEmpty())
                reason += AnsiString(" key=") + error.key;
            if (!error.value.IsEmpty())
                reason += AnsiString(" value=\"") + error.value + "\"";
            if (!error.detail.IsEmpty())
                reason += AnsiString(" detail=") + error.detail;

            return reason;
        }

        /// 读整个文件到字节缓冲（`AutoDeleteArray` 自带 +1 尾巴，TOML 需要 '\0'）
        bool ReadWholeFile(const OSString &path, AutoDeleteArray<uint8> &out_buffer, int64 &out_size)
        {
            out_size = 0;

            hgl::io::OpenFileInputStream opener(path);
            if (!opener)
            {
                GLogError(u8"[MaterialDataBlockRegistry] 文件打不开: %ls", path.c_str());
                return false;
            }

            const int64 size = opener->GetSize();
            if (size <= 0 || size > INT_MAX)
            {
                GLogError(u8"[MaterialDataBlockRegistry] 文件大小无效: %ls (%lld)",
                          path.c_str(), static_cast<long long>(size));
                return false;
            }

            if (!out_buffer.alloc(static_cast<size_t>(size) + 1))
            {
                GLogError(u8"[MaterialDataBlockRegistry] 文件缓冲分配失败: %ls (%lld)",
                          path.c_str(), static_cast<long long>(size));
                return false;
            }

            if (opener->Read(out_buffer.data(), size) != size)
            {
                GLogError(u8"[MaterialDataBlockRegistry] 文件读取失败: %ls", path.c_str());
                return false;
            }

            out_buffer[static_cast<size_t>(size)] = 0;
            out_size = size;
            return true;
        }
    }//namespace

    MaterialDataBlockAsset *MaterialDataBlockRegistry::Attach(MaterialDataBlockFileData &&data,
                                                              MaterialDataBlockSpec      &&spec,
                                                              const bool                   has_spec,
                                                              const os_char               *label)
    {
        MaterialDataBlockAsset *asset =
            blocks.Create(std::move(data), std::move(spec), has_spec);

        if (!asset)
        {
            GLogError(u8"[MaterialDataBlockRegistry] 无法分配数据块记录: %ls",
                      label ? label : OS_TEXT("<memory>"));
            return nullptr;
        }

        asset->SetSource(label);

        // 块名重复只告警不拒绝：数据块的身份首先由 URI / 路径决定，重名不会串数据，
        // 但会让 `FindByName` 只拿到第一个，属于作者该知道的事情。
        if (asset->HasSpec() && !asset->GetSpec().name.IsEmpty()
         && FindByName(asset->GetSpec().name.c_str()) != asset)
            GLogWarning(u8"[MaterialDataBlockRegistry] 数据块重名（先注册者优先）: %s source=%ls",
                        asset->GetSpec().name.c_str(), label ? label : OS_TEXT("<memory>"));

        return asset;
    }

    MaterialDataBlockAsset *MaterialDataBlockRegistry::RegisterFromToml(const os_char *label,
                                                                        const char *content,
                                                                        const int content_size)
    {
        const os_char *source_label = label ? label : OS_TEXT("<memory>");

        MaterialDataBlockFileData data;
        MaterialDataBlockSpec spec;

        const MaterialDataBlockParseResult result =
            ParseMaterialDataBlockFile(content, content_size, data, &spec);

        MaterialDataBlockAsset *asset =
            Attach(std::move(data), std::move(spec), true, source_label);

        if (!asset)
            return nullptr;

        if (result != MaterialDataBlockParseResult::OK)
        {
            const AnsiString reason =
                MakeFailureReason("material data block toml", result, asset->GetParseError());

            asset->MarkFailed(reason.c_str());

            GLogError(u8"[MaterialDataBlockRegistry] 数据块解析失败: %ls (%s)"
                      u8" line=%d key=%s value=\"%s\" detail=%s",
                      source_label,
                      GetMaterialDataBlockParseResultName(result),
                      asset->GetParseError().line,
                      asset->GetParseError().key.c_str(),
                      asset->GetParseError().value.c_str(),
                      asset->GetParseError().detail.c_str());

            return asset;
        }

        GLogInfo(u8"[MaterialDataBlockRegistry] 数据块已加载: %ls name=%s type=%s rows=%u row_bytes=%u",
                 source_label,
                 asset->GetSpec().name.c_str(),
                 GetGlobalSSBOTypeName(asset->GetGlobalSSBOType()),
                 asset->GetRowCount(),
                 asset->GetRowBytes());

        return asset;
    }

    MaterialDataBlockAsset *MaterialDataBlockRegistry::RegisterFromBinary(const os_char *label,
                                                                          const uint8 *data,
                                                                          const uint64 size)
    {
        const os_char *source_label = label ? label : OS_TEXT("<memory>");

        MaterialDataBlock block;
        AnsiString error;

        if (!ReadMaterialDataBlockBinary(data, size, block, error))
        {
            // 连块都建不起来 ⇒ 仍给一个 `Failed` 句柄，工具能显示「这个文件坏了」
            MaterialDataBlockFileData failed_data;
            MaterialDataBlockAsset *asset =
                Attach(std::move(failed_data), MaterialDataBlockSpec{}, false, source_label);

            if (asset)
            {
                AnsiString reason("material data block binary: ");
                reason += error;
                asset->MarkFailed(reason.c_str());
            }

            GLogError(u8"[MaterialDataBlockRegistry] 数据块二进制解析失败: %ls (%s)",
                      source_label, error.c_str());
            return asset;
        }

        MaterialDataBlockFileData file_data;
        file_data.block = std::move(block);

        MaterialDataBlockAsset *asset =
            Attach(std::move(file_data), MaterialDataBlockSpec{}, false, source_label);

        if (!asset)
            return nullptr;

        if (!asset->GetBlock().IsValid())
        {
            asset->MarkFailed("material data block binary: payload invalid (row_bytes/row_count)");
            return asset;
        }

        GLogInfo(u8"[MaterialDataBlockRegistry] 数据块二进制已加载（无授权形态）: %ls type=%s rows=%u row_bytes=%u",
                 source_label,
                 GetGlobalSSBOTypeName(asset->GetGlobalSSBOType()),
                 asset->GetRowCount(),
                 asset->GetRowBytes());

        return asset;
    }

    MaterialDataBlockAsset *MaterialDataBlockRegistry::RegisterFromFlat(const os_char *label,
                                                                        const uint8 *data,
                                                                        const uint64 size)
    {
        const os_char *source_label = label ? label : OS_TEXT("<memory>");

        MaterialDataBlockSpec spec;
        AnsiString error;

        if (!DecodeMaterialDataBlockFlat(data, size, spec, &error))
        {
            MaterialDataBlockFileData failed_data;
            MaterialDataBlockAsset *asset =
                Attach(std::move(failed_data), MaterialDataBlockSpec{}, false, source_label);

            if (asset)
            {
                AnsiString reason("material data block flatbuffers: ");
                reason += error;
                asset->MarkFailed(reason.c_str());
            }

            GLogError(u8"[MaterialDataBlockRegistry] 数据块 FlatBuffers 解析失败: %ls (%s)",
                      source_label, error.c_str());
            return asset;
        }

        MaterialDataBlock block;
        if (!BuildMaterialDataBlock(spec, block))
        {
            MaterialDataBlockFileData failed_data;
            MaterialDataBlockAsset *asset =
                Attach(std::move(failed_data), MaterialDataBlockSpec{}, true, source_label);

            if (asset)
                asset->MarkFailed("material data block flatbuffers: flatten failed");

            return asset;
        }

        MaterialDataBlockFileData file_data;
        file_data.block = std::move(block);

        return Attach(std::move(file_data), std::move(spec), true, source_label);
    }

    bool MaterialDataBlockRegistry::LoadFile(const OSString &path)
    {
        if (path.IsEmpty())
        {
            GLogError(u8"[MaterialDataBlockRegistry] LoadFile 收到空路径");
            return false;
        }

        AutoDeleteArray<uint8> buffer;
        int64 size = 0;

        if (!ReadWholeFile(path, buffer, size))
            return false;

        MaterialDataBlockAsset *asset = nullptr;

        if (IsTomlFile(path))
            asset = RegisterFromToml(path.c_str(), reinterpret_cast<const char *>(buffer.data()),
                                     static_cast<int>(size));
        else if (IsMdbFile(path))
            asset = RegisterFromBinary(path.c_str(), buffer.data(), static_cast<uint64>(size));
        else if (IsFlatBufferFile(path))
            asset = RegisterFromFlat(path.c_str(), buffer.data(), static_cast<uint64>(size));
        else
        {
            GLogError(u8"[MaterialDataBlockRegistry] 不是数据块文件（需 .material_data.toml/.mdb/.fb）: %ls",
                      path.c_str());
            return false;
        }

        return asset && !asset->IsFailed();
    }

    bool MaterialDataBlockRegistry::LoadDirectory(const OSString &directory,
                                                  int *out_file_count,
                                                  int *out_error_count)
    {
        if (!hgl::filesystem::Path(directory).IsDirectory())
        {
            GLogError(u8"[MaterialDataBlockRegistry] 数据块目录不存在或不可访问: %ls",
                      directory.c_str());
            return false;
        }

        hgl::ValueArray<hgl::filesystem::FileInfo> file_list;
        const int scan_count = hgl::filesystem::GetFileInfoList(
            file_list, directory, true, true, true);
        if (scan_count < 0)
        {
            GLogError(u8"[MaterialDataBlockRegistry] 数据块目录扫描失败: %ls (result=%d)",
                      directory.c_str(), scan_count);
            return false;
        }

        int file_count = 0;
        int error_count = 0;
        for (int i = 0; i < file_list.GetCount(); ++i)
        {
            const hgl::filesystem::FileInfo &file = file_list[i];
            if (!file.is_file || !IsAnyFile(OSString(file.name)))
                continue;

            if (LoadFile(OSString(file.fullname)))
                ++file_count;
            else
                ++error_count;
        }

        if (out_file_count)
            *out_file_count = file_count;
        if (out_error_count)
            *out_error_count = error_count;

        GLogInfo(u8"[MaterialDataBlockRegistry] 数据块目录加载完成: directory=%ls loaded=%d errors=%d total=%d",
                 directory.c_str(), file_count, error_count, blocks.GetCount());

        return true;
    }

    int MaterialDataBlockRegistry::UploadAll(GlobalSSBOBufferRegistry *registry)
    {
        if (!registry)
        {
            GLogWarning(u8"[MaterialDataBlockRegistry] UploadAll 未绑定 SSBO 注册表，%d 个数据块留在 Parsed",
                        blocks.GetCount());
            return 0;
        }

        int uploaded = 0;
        for (int i = 0; i < blocks.GetCount(); ++i)
        {
            MaterialDataBlockAsset *asset = blocks.At(i);
            if (!asset || asset->IsUploaded() || asset->IsFailed())
                continue;

            if (asset->Upload(registry))
                ++uploaded;
        }

        return uploaded;
    }

    MaterialDataBlockAsset *MaterialDataBlockRegistry::GetOrLoadURI(const OSString &uri)
    {
        if (uri.IsEmpty())
        {
            GLogError(u8"[MaterialDataBlockRegistry] GetOrLoadURI 收到空 URI");
            return nullptr;
        }

        const hgl::asset::AssetURI parsed =
            hgl::asset::AssetURI::Parse(uri, hgl::asset::AssetType::MaterialData);

        if (parsed.HasType() && parsed.type != hgl::asset::AssetType::MaterialData)
        {
            GLogError(u8"[MaterialDataBlockRegistry] URI 类型不是数据块：%s（实际 type=\"%s\"）",
                      hgl::asset::AssetURISegmentToAnsi(uri).c_str(),
                      hgl::asset::GetAssetTypeName(parsed.type));
            return nullptr;
        }

        // `mtldata:@<name>`（省略来源）⇒ 按名字查表；其余按路径。
        if (!parsed.IsFile() && !parsed.target.IsEmpty() && parsed.target[0] == OS_TEXT('@'))
        {
            const OSString name = parsed.target.SubString(1);

            MaterialDataBlockAsset *asset = FindByNameOS(name);

            if (!asset)
                GLogError(u8"[MaterialDataBlockRegistry] 数据块名未注册：%ls", name.c_str());

            return asset;
        }

        if (parsed.source == hgl::asset::AssetSourceKind::RenderTarget)
        {
            GLogError(u8"[MaterialDataBlockRegistry] 数据块不支持 rt: 来源：%s",
                      hgl::asset::AssetURISegmentToAnsi(uri).c_str());
            return nullptr;
        }

        MaterialDataBlockAsset *cached = FindAssetByPath(parsed.target);
        if (cached)
            return cached;

        LoadFile(parsed.target);

        return FindAssetByPath(parsed.target);
    }

    MaterialDataBlockAsset *MaterialDataBlockRegistry::FindByName(const char *name)
    {
        if (!name || !name[0])
            return nullptr;

        for (int i = 0; i < blocks.GetCount(); ++i)
        {
            MaterialDataBlockAsset *asset = blocks.At(i);
            if (asset && !asset->GetAssetName().IsEmpty()
             && asset->GetAssetName() == name)
                return asset;
        }

        return nullptr;
    }

    MaterialDataBlockAsset *MaterialDataBlockRegistry::FindByNameOS(const OSString &name)
    {
        if (name.IsEmpty())
            return nullptr;

        for (int i = 0; i < blocks.GetCount(); ++i)
        {
            MaterialDataBlockAsset *asset = blocks.At(i);
            if (asset && !asset->GetAssetName().IsEmpty()
             && ToOSString(asset->GetAssetName().c_str()) == name)
                return asset;
        }

        return nullptr;
    }

    MaterialDataBlockAsset *MaterialDataBlockRegistry::FindAssetByPath(const OSString &path)
    {
        if (path.IsEmpty())
            return nullptr;

        for (int i = 0; i < blocks.GetCount(); ++i)
        {
            MaterialDataBlockAsset *asset = blocks.At(i);
            if (asset && !asset->GetSourcePath().IsEmpty()
             && asset->GetSourcePath() == path)
                return asset;
        }

        return nullptr;
    }
}//namespace hgl::graph::mtl
