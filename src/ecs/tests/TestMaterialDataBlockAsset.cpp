// TestMaterialDataBlockAsset.cpp —— 材质数据块**资产层**门禁（无需图形设备）：
//
//   1. 三种文件形态（TOML / `MDB1` / FlatBuffers）都落到同一份句柄 + 同一份物理行；
//   2. 授权形态的有无如实反映在 `HasSpec()`（TOML/FB 有，`MDB1` 没有）；
//   3. 坏输入留在表里但标 `Failed` 且给出定位，而不是静默消失；
//   4. 没有 SSBO 注册表时上传是**软失败**：数据仍在、句柄停在 `Parsed`、不标 `Failed`；
//   5. URI 门面（`mtldata:@<name>` / 路径 / 类型不符）与 `AssetFactory` 加载器接线；
//   6. `AssetType::MaterialData` 的类型段拼写从类型表派生。
//
// 真机上船（行区间连续、首行号、字节写进映射区）由 `TestGlobalSSBORowRange` 以源码
// 契约 + 无效态 fail-closed 的方式覆盖，这里不重复。

#include <hgl/asset/AssetFactory.h>
#include <hgl/asset/AssetURI.h>
#include <hgl/asset/generated/MaterialDataBlockMeta.gen.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/graph/asset/MaterialDataBlockObject.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/mtl/MaterialDataBlockFile.h>
#include <hgl/mtl/MaterialDataBlockFlat.h>
#include <hgl/vk/buffer/ActiveRowPool.h>

#include <cstring>

using namespace hgl;

namespace asset = hgl::asset;
namespace mtl = hgl::graph::mtl;

namespace
{
    int g_failed = 0;

    void Check(const bool condition, const char *what)
    {
        if (condition)
            return;

        GLogError(u8"TestMaterialDataBlockAsset 失败: %s", what);
        ++g_failed;
    }

    const char *kPbrBlock =
        "schema = 1\n"
        "name = \"pbr_grid\"\n"
        "target = \"PBRSurface\"\n"
        "\n"
        "[[rows]]\n"
        "base_color = [0.0, 0.0, 0.0, 1.0]\n"
        "metallic = 0.0\n"
        "roughness = 0.05\n"
        "\n"
        "[[rows]]\n"
        "base_color = [0.5, 0.5, 0.5, 1.0]\n"
        "metallic = 0.5\n"
        "roughness = 0.5\n"
        "\n"
        "[[rows]]\n"
        "base_color = [1.0, 1.0, 1.0, 1.0]\n"
        "metallic = 1.0\n"
        "roughness = 1.0\n";

    const char *kBadBlock =
        "schema = 1\n"
        "name = \"broken\"\n"
        "target = \"PBRSurface\"\n"
        "\n"
        "[[rows]]\n"
        "metallic_v2 = 0.5\n";

    /// 参照物：直接走文件层解析得到的规约 + 物理行
    bool ParseReference(hgl::graph::MaterialDataBlockSpec &out_spec, mtl::MaterialDataBlock &out_block)
    {
        mtl::MaterialDataBlockFileData data;
        const auto size = static_cast<int>(strlen(kPbrBlock));

        if (mtl::ParseMaterialDataBlockFile(kPbrBlock, size, data, &out_spec)
            != mtl::MaterialDataBlockParseResult::OK)
            return false;

        out_block = std::move(data.block);
        return out_block.IsValid();
    }

    /// 两个块的 payload 逐字节相同
    bool SamePayload(const mtl::MaterialDataBlock &a, const mtl::MaterialDataBlock &b)
    {
        if (a.type != b.type || a.row_bytes != b.row_bytes)
            return false;
        if (a.GetByteSize() != b.GetByteSize())
            return false;

        return a.GetByteSize() == 0
            || memcmp(a.GetData(), b.GetData(), a.GetByteSize()) == 0;
    }

    bool TestMetaWiring()
    {
        // 句柄对检查器宣告的是「这类资产本该长什么样」，与来源形态无关。
        mtl::MaterialDataBlockRegistry registry;
        mtl::MaterialDataBlockAsset *handle = registry.RegisterFromToml(
            OS_TEXT("<meta>"), kPbrBlock, static_cast<int>(strlen(kPbrBlock)));

        if (!handle)
            return false;

        const asset::ClassMetaInfo *meta = handle->GetClassMetaInfo();

        return meta == &asset::generated::Class_MaterialDataBlockSpec
            && meta->property_count == 4
            && meta->struct_size == sizeof(hgl::graph::MaterialDataBlockSpec);
    }

    bool TestTomlHandle()
    {
        hgl::graph::MaterialDataBlockSpec ref_spec;
        mtl::MaterialDataBlock ref_block;

        if (!ParseReference(ref_spec, ref_block))
            return false;

        mtl::MaterialDataBlockRegistry registry;
        mtl::MaterialDataBlockAsset *handle = registry.RegisterFromToml(
            OS_TEXT("res/material/pbr_grid.material_data.toml"),
            kPbrBlock, static_cast<int>(strlen(kPbrBlock)));

        if (!handle)
            return false;

        if (handle->IsFailed() || handle->GetState() != mtl::MaterialDataBlockAssetState::Parsed)
            return false;

        // 授权形态就在这里：名字、行数、行宽都能回读。
        if (!handle->HasSpec())
            return false;
        if (handle->GetSpec().name != "pbr_grid")
            return false;
        if (handle->GetGlobalSSBOType() != hgl::graph::GlobalSSBOType::PBRSurface)
            return false;
        if (handle->GetRowBytes() != 32)
            return false;
        if (handle->GetRowCount() != 3 || handle->GetByteSize() != 96)
            return false;

        // 物理行必须与文件层直通结果逐字节相同（句柄不得自己再摊平一遍）。
        if (!SamePayload(handle->GetBlock(), ref_block))
            return false;

        // 未上船 ⇒ 没有行区间，且不是 Failed。
        if (handle->IsUploaded() || handle->IsReady())
            return false;
        if (handle->GetFirstRow() != hgl::graph::ActiveRowPool::InvalidRowID
         || handle->RowIDAt(0) != hgl::graph::ActiveRowPool::InvalidRowID)
            return false;

        // 有名字 ⇒ 身份走 `mtldata:@<name>`，来源路径仍记着。
        if (handle->GetAssetName() != AnsiString("pbr_grid"))
            return false;
        if (handle->GetResourceId() != U8String(u8"mtldata:@pbr_grid"))
            return false;

        return handle->GetSourcePath()
            == OSString(OS_TEXT("res/material/pbr_grid.material_data.toml"));
    }

    bool TestBinaryHandle()
    {
        hgl::graph::MaterialDataBlockSpec ref_spec;
        mtl::MaterialDataBlock ref_block;

        if (!ParseReference(ref_spec, ref_block))
            return false;

        hgl::ValueArray<uint8> bytes;
        if (!mtl::WriteMaterialDataBlockBinary(ref_block, bytes) || bytes.GetCount() == 0)
            return false;

        mtl::MaterialDataBlockRegistry registry;
        mtl::MaterialDataBlockAsset *handle = registry.RegisterFromBinary(
            OS_TEXT("res/material/pbr_grid.material_data.mdb"),
            bytes.GetData(), static_cast<uint64>(bytes.GetCount()));

        if (!handle || handle->IsFailed())
            return false;

        // `MDB1` 就是「一坨字节」：没有字段语义，但行信息完整。
        if (handle->HasSpec())
            return false;
        if (handle->GetRowCount() != 3 || handle->GetRowBytes() != 32)
            return false;
        if (!SamePayload(handle->GetBlock(), ref_block))
            return false;

        // 无名 ⇒ 身份退化成路径形态，保证唯一。
        if (!handle->GetAssetName().IsEmpty())
            return false;

        return handle->GetResourceId()
            == U8String(u8"mtldata:file:res/material/pbr_grid.material_data.mdb");
    }

    bool TestFlatHandle()
    {
        hgl::graph::MaterialDataBlockSpec ref_spec;
        mtl::MaterialDataBlock ref_block;

        if (!ParseReference(ref_spec, ref_block))
            return false;

        hgl::ValueArray<uint8> bytes;
        if (!mtl::EncodeMaterialDataBlockFlat(ref_spec, bytes) || bytes.GetCount() == 0)
            return false;

        mtl::MaterialDataBlockRegistry registry;
        mtl::MaterialDataBlockAsset *handle = registry.RegisterFromFlat(
            OS_TEXT("res/material/pbr_grid.material_data.fb"),
            bytes.GetData(), static_cast<uint64>(bytes.GetCount()));

        if (!handle || handle->IsFailed())
            return false;

        // FlatBuffers 是带字段的，因此授权形态在。
        if (!handle->HasSpec() || handle->GetSpec().name != "pbr_grid")
            return false;
        if (handle->GetRowCount() != ref_block.GetRowCount())
            return false;

        return SamePayload(handle->GetBlock(), ref_block);
    }

    bool TestBadInputs()
    {
        mtl::MaterialDataBlockRegistry registry;

        // 坏 TOML：句柄**留着**（工具要显示诊断），但必须是 Failed。
        mtl::MaterialDataBlockAsset *bad_toml = registry.RegisterFromToml(
            OS_TEXT("<bad_toml>"), kBadBlock, static_cast<int>(strlen(kBadBlock)));

        if (!bad_toml || !bad_toml->IsFailed())
            return false;
        if (bad_toml->GetFailureReason().IsEmpty())
            return false;

        // 坏 `MDB1`（magic 不对）⇒ Failed，不是「静默空块」。
        const uint8 garbage[32] = {'B', 'A', 'D', '!'};
        mtl::MaterialDataBlockAsset *bad_bin = registry.RegisterFromBinary(
            OS_TEXT("<bad_bin>"), garbage, sizeof(garbage));

        if (!bad_bin || !bad_bin->IsFailed())
            return false;
        if (bad_bin->GetFailureReason().IsEmpty())
            return false;

        // 坏 FlatBuffers（不够一个最小缓冲）⇒ Failed。
        const uint8 tiny[2] = {0, 0};
        mtl::MaterialDataBlockAsset *bad_flat = registry.RegisterFromFlat(
            OS_TEXT("<bad_flat>"), tiny, sizeof(tiny));

        if (!bad_flat || !bad_flat->IsFailed())
            return false;

        // 失败者也进表（三态都可见）。
        return registry.GetCount() == 3;
    }

    bool TestRegistryLookup()
    {
        mtl::MaterialDataBlockRegistry registry;

        // 注册时给的 label 同时就是来源路径：文件已读进内存、路径仍要保留，
        // 这样按路径去重/定位才成立。
        const os_char *label = OS_TEXT("<mem>");

        mtl::MaterialDataBlockAsset *by_name =
            registry.RegisterFromToml(label, kPbrBlock,
                                      static_cast<int>(strlen(kPbrBlock)));

        if (!by_name)
            return false;

        // 无名查询、空名字都不该命中。
        if (registry.FindByName(nullptr) || registry.FindByName(""))
            return false;
        if (registry.FindByName("pbr_grid_v2"))
            return false;

        // URI 门面：`mtldata:@<name>` 查表。
        if (registry.GetOrLoadURI(OS_TEXT("mtldata:@pbr_grid")) != by_name)
            return false;

        // 空 URI / 类型不符都明确拒绝，不当路径去读盘。
        if (registry.GetOrLoadURI(OSString()))
            return false;
        if (registry.GetOrLoadURI(OS_TEXT("geo:@pbr_grid")))
            return false;

        // 注册时给了 label ⇒ 该 label 同时就是来源路径。
        return registry.FindAssetByPath(OSString(label)) == by_name
            && registry.FindAssetByPath(OS_TEXT("<other>")) == nullptr
            && registry.GetAssetAt(0) == by_name
            && registry.GetAssetAt(1) == nullptr
            && registry.GetCount() == 1;
    }

    bool TestNoDeviceUpload()
    {
        mtl::MaterialDataBlockRegistry registry;

        mtl::MaterialDataBlockAsset *handle = registry.RegisterFromToml(
            OS_TEXT("<mem>"), kPbrBlock, static_cast<int>(strlen(kPbrBlock)));

        if (!handle)
            return false;

        // 未绑池：批量上传一个都不成，但**不**把数据判成坏的。
        if (registry.UploadAll(nullptr) != 0)
            return false;
        if (handle->IsFailed() || handle->GetState() != mtl::MaterialDataBlockAssetState::Parsed)
            return false;

        // 单块上传同样是软失败：数据仍在（行数可读），原因写明。
        if (handle->Upload(nullptr))
            return false;
        if (handle->IsFailed() || handle->IsUploaded())
            return false;
        if (handle->GetRowCount() != 3)
            return false;

        return !handle->GetFailureReason().IsEmpty();
    }

    /// 把文本写成临时文件（扩展名分派要真文件才测得出来）
    bool WriteTempFile(const OSString &path, const void *data, const uint64 size)
    {
        hgl::io::FileOutputStream *fos = hgl::io::CreateFileOutputStream(path);

        if (!fos)
            return false;

        const bool ok = fos->Write(data, size) == static_cast<int64>(size);

        delete fos;
        return ok;
    }

    bool TestLoadFileDispatch()
    {
        const OSString toml_path(OS_TEXT("build/out/Windows_64_Release/__t_grid.material_data.toml"));
        const OSString mdb_path(OS_TEXT("build/out/Windows_64_Release/__t_grid.material_data.mdb"));
        const OSString unknown_path(OS_TEXT("build/out/Windows_64_Release/__t_grid.txt"));

        hgl::graph::MaterialDataBlockSpec ref_spec;
        mtl::MaterialDataBlock ref_block;

        if (!ParseReference(ref_spec, ref_block))
            return false;

        hgl::ValueArray<uint8> binary;
        if (!mtl::WriteMaterialDataBlockBinary(ref_block, binary))
            return false;

        bool ok = WriteTempFile(toml_path, kPbrBlock, strlen(kPbrBlock))
               && WriteTempFile(mdb_path, binary.GetData(),
                                static_cast<uint64>(binary.GetCount()))
               && WriteTempFile(unknown_path, "x", 1);

        if (ok)
        {
            mtl::MaterialDataBlockRegistry registry;

            // 扩展名决定读法：TOML 有授权形态，`MDB1` 没有，两者行字节相同。
            ok = registry.LoadFile(toml_path)
              && registry.FindAssetByPath(toml_path) != nullptr
              && registry.FindAssetByPath(toml_path)->HasSpec()
              && registry.LoadFile(mdb_path)
              && registry.FindAssetByPath(mdb_path) != nullptr
              && !registry.FindAssetByPath(mdb_path)->HasSpec()
              && SamePayload(registry.FindAssetByPath(toml_path)->GetBlock(),
                             registry.FindAssetByPath(mdb_path)->GetBlock());

            // 不认识的后缀、不存在的文件都是明确失败。
            if (ok)
                ok = !registry.LoadFile(unknown_path)
                  && !registry.LoadFile(OS_TEXT("build/out/Windows_64_Release/__t_missing.mdb"));

            // URI 门面的 File 形态：首次按需加载，第二次命中缓存。
            if (ok)
            {
                const OSString uri = OSString(OS_TEXT("mtldata:file:")) + toml_path;
                mtl::MaterialDataBlockAsset *first = registry.GetOrLoadURI(uri);

                ok = first != nullptr && first->IsReady() == false
                  && !first->IsFailed()
                  && registry.GetOrLoadURI(uri) == first;
            }
        }

        hgl::filesystem::FileDelete(toml_path);
        hgl::filesystem::FileDelete(mdb_path);
        hgl::filesystem::FileDelete(unknown_path);

        return ok;
    }

    bool TestAssetFactory()
    {
        mtl::MaterialDataBlockRegistry registry;

        mtl::MaterialDataBlockAsset *handle = registry.RegisterFromToml(
            OS_TEXT("<mem>"), kPbrBlock, static_cast<int>(strlen(kPbrBlock)));

        if (!handle)
            return false;

        if (!mtl::RegisterMaterialDataBlockAssetLoader(&registry))
            return false;

        bool ok = asset::AssetFactory::Instance().IsRegistered(asset::AssetType::MaterialData);

        if (ok)
        {
            asset::AssetLoadStatus status = asset::AssetLoadStatus::ParseFailed;
            asset::AssetObjectPtr object =
                asset::AssetFactory::Instance().Create(OSString(OS_TEXT("mtldata:@pbr_grid")),
                                                       asset::AssetLoadOptions(), &status);

            ok = object != nullptr
              && status == asset::AssetLoadStatus::Ok
              && object->GetAssetName() == AnsiString("pbr_grid");

            if (ok)
            {
                mtl::MaterialDataBlockObject *shell =
                    dynamic_cast<mtl::MaterialDataBlockObject *>(
                        static_cast<asset::AssetObject *>(object));

                // 外壳不拥有句柄：必须指回注册表里那一个，且未上船时 IsReady 为假。
                ok = shell != nullptr
                  && shell->GetMaterialDataBlockAsset() == handle
                  && !shell->IsReady()
                  && shell->GetURI().type == asset::AssetType::MaterialData;
            }

            // 查不到的名字必须失败，不能糊出一个空壳。
            if (ok)
                ok = asset::AssetFactory::Instance().Create(OSString(OS_TEXT("mtldata:@nope")))
                     == nullptr;
        }

        ok = ok && mtl::UnregisterMaterialDataBlockAssetLoader()
                && !asset::AssetFactory::Instance().IsRegistered(
                       asset::AssetType::MaterialData);

        return ok;
    }

    bool TestAssetTypeTable()
    {
        // 类型段拼写只有一处真源（`AssetTypeTable`）：`major_name` 是 name，可解析。
        return AnsiString(asset::GetAssetTypeName(asset::AssetType::MaterialData))
                == AnsiString("mtldata")
            && asset::ParseAssetType("mtldata") == asset::AssetType::MaterialData
            && AnsiString(asset::GetAssetTypeLabel(asset::AssetType::MaterialData))
                == AnsiString("material_data");
    }
}//namespace

int main(int, char **)
{
    hgl::logger::InitLogger(OS_TEXT("TestMaterialDataBlockAsset"));

    Check(TestMetaWiring(), "句柄向检查器宣告块规约元数据");
    Check(TestTomlHandle(), "TOML 来源：授权形态 + 物理行 + 身份");
    Check(TestBinaryHandle(), "MDB1 来源：无授权形态但行信息完整");
    Check(TestFlatHandle(), "FlatBuffers 来源：授权形态与摊平行一致");
    Check(TestBadInputs(), "坏输入留在表里并标 Failed（带定位）");
    Check(TestRegistryLookup(), "注册表按名字/路径/URI 定位");
    Check(TestNoDeviceUpload(), "无 SSBO 注册表时上传软失败");
    Check(TestLoadFileDispatch(), "扩展名分派与按需加载");
    Check(TestAssetFactory(), "AssetFactory 加载器接线与外壳所有权");
    Check(TestAssetTypeTable(), "AssetType::MaterialData 类型段拼写");

    if (g_failed > 0)
    {
        GLogError(u8"TestMaterialDataBlockAsset: %d 项失败", g_failed);
        return 1;
    }

    GLogInfo(u8"TestMaterialDataBlockAsset: 全部通过");
    return 0;
}



