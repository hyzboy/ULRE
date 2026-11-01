#include <hgl/graph/asset/MaterialDataBlockAsset.h>

#include <hgl/asset/AssetURI.h>
#include <hgl/asset/generated/MaterialDataBlockMeta.gen.h>
#include <hgl/log/Log.h>
#include <hgl/utf.h>

#include <utility>

namespace hgl::graph::mtl
{
    MaterialDataBlockAsset::MaterialDataBlockAsset(MaterialDataBlockFileData &&in_data,
                                                   MaterialDataBlockSpec      &&in_spec,
                                                   const bool                   in_has_spec)
        : spec(in_has_spec ? std::move(in_spec) : MaterialDataBlockSpec{}),
          has_spec(in_has_spec),
          block(std::move(in_data.block)),
          parse_error(in_data.parse_error)
    {
        if (has_spec && !spec.name.IsEmpty())
            asset_name = spec.name;
    }

    const hgl::asset::ClassMetaInfo *MaterialDataBlockAsset::GetClassMetaInfo() const
    {
        return &hgl::asset::generated::Class_MaterialDataBlockSpec;
    }

    void MaterialDataBlockAsset::SetSource(const os_char *label)
    {
        // 类型段一律从类型表派生，避免和 `AssetTypeTable` 各写一份拼写。
        const U8String type_prefix(ToU8String(
            AnsiString(hgl::asset::GetAssetTypeName(hgl::asset::AssetType::MaterialData))));

        if (label && label[0])
            source_path = label;
        else
            source_path.Clear();

        if (!asset_name.IsEmpty())
        {
            resource_id = type_prefix;
            resource_id += u8":@";
            resource_id += ToU8String(asset_name);
            return;
        }

        // 无名块（`MDB1` 二进制、检查器生成物）用路径做身份，保证身份唯一。
        resource_id = type_prefix;
        resource_id += u8":file:";
        resource_id += ToU8String(source_path);
    }

    void MaterialDataBlockAsset::MarkFailed(const char *reason)
    {
        state = MaterialDataBlockAssetState::Failed;

        if (reason && reason[0])
            failure_reason = reason;
        else
            failure_reason.Clear();

        // 失败即放弃已占的行：留下半截区间比失败本身更难查。
        row_lease.Release();
    }

    bool MaterialDataBlockAsset::Upload(GlobalSSBOBufferRegistry *registry)
    {
        if (IsUploaded())
            return true;

        if (state == MaterialDataBlockAssetState::Failed)
            return false;

        if (!block.IsValid())
        {
            MarkFailed("material data block: payload invalid (row_bytes/row_count)");
            return false;
        }

        // 未绑池（无图形设备）不是数据错误 ⇒ 停在 `Parsed`，工具仍可读行。
        if (!registry)
        {
            failure_reason = "material data block: ssbo registry not bound";
            return false;
        }

        GlobalSSBORowRangeLease lease;
        if (!registry->UploadMaterialDataBlock(MakeBlockView(block), lease))
        {
            failure_reason = "material data block: upload rejected (row_bytes mismatch or pool exhausted)";
            GLogError(u8"[MaterialDataBlockAsset] 数据块上传失败: %s rows=%u row_bytes=%u",
                      asset_name.c_str(), block.GetRowCount(), block.row_bytes);
            return false;
        }

        row_lease = std::move(lease);
        state = MaterialDataBlockAssetState::Ready;
        failure_reason.Clear();

        GLogInfo(u8"[MaterialDataBlockAsset] 数据块已上传: %s type=%s rows=%u first=%u ssbo=%u",
                 asset_name.c_str(),
                 GetGlobalSSBOTypeName(block.type),
                 block.GetRowCount(),
                 row_lease.GetFirstRow(),
                 row_lease.GetSSBOId());

        return true;
    }
}//namespace hgl::graph::mtl
