#include <hgl/graph/asset/EnvironmentAssetRegistry.h>

#include <hgl/asset/AssetURI.h>
#include <hgl/asset/generated/EnvironmentSerialize.gen.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/type/Smart.h>
#include <hgl/utf.h>

#include <climits>

namespace hgl::graph
{
    namespace
    {
        /// `sh9.bands` 的基函数个数（Z-up L2，见 `EnvironmentSH9BandSpec`）
        constexpr int kSH9BandCount = 9;

        /// 当前唯一支持的 `schema` 版本
        constexpr uint32 kEnvironmentSchemaVersion = 1;

        bool IsEnvironmentTomlFile(const OSString &name)
        {
            return name.EndsWith(OS_TEXT(".env.toml"))
                || name.EndsWith(OS_TEXT(".environment.toml"));
        }

        /// 读整个文件到字节缓冲（`AutoDeleteArray` 自带 +1 尾巴，TOML 需要 '\0'）
        bool ReadWholeFile(const OSString &path, AutoDeleteArray<uint8> &out_buffer, int64 &out_size)
        {
            out_size = 0;

            hgl::io::OpenFileInputStream opener(path);
            if (!opener)
            {
                GLogError(u8"[EnvironmentAssetRegistry] 文件打不开: %ls", path.c_str());
                return false;
            }

            const int64 size = opener->GetSize();
            if (size <= 0 || size > INT_MAX)
            {
                GLogError(u8"[EnvironmentAssetRegistry] 文件大小无效: %ls (%lld)",
                          path.c_str(), static_cast<long long>(size));
                return false;
            }

            if (!out_buffer.alloc(static_cast<size_t>(size) + 1))
            {
                GLogError(u8"[EnvironmentAssetRegistry] 文件缓冲分配失败: %ls (%lld)",
                          path.c_str(), static_cast<long long>(size));
                return false;
            }

            if (opener->Read(out_buffer.data(), size) != size)
            {
                GLogError(u8"[EnvironmentAssetRegistry] 文件读取失败: %ls", path.c_str());
                return false;
            }

            out_buffer[static_cast<size_t>(size)] = 0;
            out_size = size;
            return true;
        }

        /// 把「TOML 语法错」与「字段序列化错」拼成一句可直接展示的原因
        AnsiString MakeParseFailureReason(const hgl::serialize::Archive &archive,
                                          const hgl::serialize::TomlParseError &toml_error)
        {
            AnsiString reason("environment toml: ");

            if (toml_error.line > 0 || !toml_error.title.IsEmpty())
            {
                reason += toml_error.Format();
                return reason;
            }

            if (archive.FailedPath()[0])
                reason += AnsiString(" path=") + archive.FailedPath();
            else if (archive.FailedField()[0])
                reason += AnsiString(" field=") + archive.FailedField();

            if (archive.ErrorMessage()[0])
                reason += AnsiString(" detail=") + archive.ErrorMessage();

            return reason;
        }
    }//namespace

    bool EnvironmentAssetRegistry::Validate(const EnvironmentSpec &spec, AnsiString &out_error) const
    {
        if (spec.schema != kEnvironmentSchemaVersion)
        {
            out_error  = "environment schema: 只支持 ";
            out_error += AnsiString::numberOf(kEnvironmentSchemaVersion);
            out_error += "，实际 ";
            out_error += AnsiString::numberOf(spec.schema);
            return false;
        }

        const auto &bands = spec.sh9.bands;

        if (!bands.present)
            return true;

        // 显式系数只在 `auto_generate == false` 时被使用；此时条数必须齐。
        // `auto_generate` 为真还写了 bands 属于作者意图冲突 ⇒ 告警（按 auto 走）。
        if (!bands.value.GetCount())
            return true;        // 空数组等价于没写

        if (bands.value.GetCount() != kSH9BandCount)
        {
            out_error  = "sh9.bands: 必须正好 ";
            out_error += AnsiString::numberOf(kSH9BandCount);
            out_error += " 条，实际 ";
            out_error += AnsiString::numberOf(static_cast<int>(bands.value.GetCount()));
            return false;
        }

        const bool auto_generate = !spec.sh9.auto_generate.present || spec.sh9.auto_generate.value;
        if (auto_generate)
            GLogWarning(u8"[EnvironmentAssetRegistry] %s 同时写了 sh9.auto_generate=true 与 "
                        u8"sh9.bands（%d 条），bands 会被忽略",
                        spec.name.IsEmpty() ? "<unnamed>" : spec.name.c_str(),
                        static_cast<int>(bands.value.GetCount()));

        return true;
    }

    EnvironmentAsset *EnvironmentAssetRegistry::Attach(EnvironmentSpec &&spec, const os_char *label)
    {
        EnvironmentAsset *asset = assets.Create(std::move(spec));

        if (!asset)
        {
            GLogError(u8"[EnvironmentAssetRegistry] 无法分配环境资产记录: %ls",
                      label ? label : OS_TEXT("<memory>"));
            return nullptr;
        }

        asset->SetSource(label);

        // 重名只告警不拒绝：身份首先由 URI / 路径决定，重名不会串数据，
        // 但会让 `env:@Name` 只命中第一个 —— 作者该知道这件事。
        if (!asset->GetEnvironmentName().IsEmpty()
         && FindByName(asset->GetEnvironmentName().c_str()) != asset)
            GLogWarning(u8"[EnvironmentAssetRegistry] 环境重名（先注册者优先）: %s source=%ls",
                        asset->GetEnvironmentName().c_str(),
                        label ? label : OS_TEXT("<memory>"));

        return asset;
    }

    EnvironmentAsset *EnvironmentAssetRegistry::RegisterFromToml(const os_char *label,
                                                                 const char  *content,
                                                                 const int    size)
    {
        last_error.Clear();

        if (!content || size <= 0)
        {
            last_error = "environment toml: 内容为空";
            GLogError(u8"[EnvironmentAssetRegistry] %s", last_error.c_str());
            return nullptr;
        }

        EnvironmentSpec spec;
        hgl::serialize::Archive archive;
        hgl::serialize::TomlParseError toml_error;

        if (!hgl::serialize::LoadFromTomlText<EnvironmentSpec>(
                content, static_cast<size_t>(size), spec, &archive, &toml_error))
        {
            last_error = MakeParseFailureReason(archive, toml_error);
            GLogError(u8"[EnvironmentAssetRegistry] 环境配置解析失败: %ls (%s)",
                      label ? label : OS_TEXT("<memory>"), last_error.c_str());
            return nullptr;
        }

        if (!Validate(spec, last_error))
        {
            GLogError(u8"[EnvironmentAssetRegistry] 环境配置校验失败: %ls (%s)",
                      label ? label : OS_TEXT("<memory>"), last_error.c_str());
            return nullptr;
        }

        EnvironmentAsset *asset = Attach(std::move(spec), label);

        if (asset)
            GLogInfo(u8"[EnvironmentAssetRegistry] 环境已加载: %ls name=%s mode=%s",
                     label ? label : OS_TEXT("<memory>"),
                     asset->GetEnvironmentName().c_str(),
                     hgl::asset::generated::AssetEnumName_SkyMode(
                         asset->GetSpec().ambient.mode.present
                             ? asset->GetSpec().ambient.mode.value
                             : SkyMode::SHLightOnly));

        return asset;
    }

    EnvironmentAsset *EnvironmentAssetRegistry::LoadFile(const OSString &path)
    {
        last_error.Clear();

        if (path.IsEmpty())
        {
            last_error = "environment: LoadFile 收到空路径";
            GLogError(u8"[EnvironmentAssetRegistry] %s", last_error.c_str());
            return nullptr;
        }

        if (!IsEnvironmentTomlFile(path))
        {
            last_error  = "environment: 不是环境配置文件（需 .env.toml）：";
            last_error += AnsiString(reinterpret_cast<const char *>(ToU8String(path).c_str()));
            GLogError(u8"[EnvironmentAssetRegistry] %s", last_error.c_str());
            return nullptr;
        }

        AutoDeleteArray<uint8> buffer;
        int64 size = 0;

        if (!ReadWholeFile(path, buffer, size))
        {
            last_error = "environment: 读取失败";
            return nullptr;
        }

        return RegisterFromToml(path.c_str(), reinterpret_cast<const char *>(buffer.data()),
                                static_cast<int>(size));
    }

    EnvironmentAsset *EnvironmentAssetRegistry::GetOrLoadURI(const OSString &uri)
    {
        last_error.Clear();

        if (uri.IsEmpty())
        {
            last_error = "environment: GetOrLoadURI 收到空 URI";
            GLogError(u8"[EnvironmentAssetRegistry] %s", last_error.c_str());
            return nullptr;
        }

        const hgl::asset::AssetURI parsed =
            hgl::asset::AssetURI::Parse(uri, hgl::asset::AssetType::Environment);

        if (parsed.HasType() && parsed.type != hgl::asset::AssetType::Environment)
        {
            last_error  = "environment: URI 类型不是环境配置（实际 type=\"";
            last_error += hgl::asset::GetAssetTypeName(parsed.type);
            last_error += "\"）：";
            last_error += hgl::asset::AssetURISegmentToAnsi(uri);

            GLogError(u8"[EnvironmentAssetRegistry] %s", last_error.c_str());
            return nullptr;
        }

        // `env:@<name>`（省略来源）⇒ 按名字查表；其余按路径。
        if (!parsed.IsFile() && !parsed.target.IsEmpty() && parsed.target[0] == OS_TEXT('@'))
        {
            const OSString name = parsed.target.SubString(1);
            const AnsiString ansi_name = hgl::asset::AssetURISegmentToAnsi(name);

            EnvironmentAsset *asset = FindByName(ansi_name.c_str());

            if (!asset)
            {
                last_error  = "environment: 环境名未注册：";
                last_error += ansi_name;
                GLogError(u8"[EnvironmentAssetRegistry] %s", last_error.c_str());
            }

            return asset;
        }

        if (parsed.source == hgl::asset::AssetSourceKind::RenderTarget)
        {
            last_error  = "environment: 环境配置不支持 rt: 来源：";
            last_error += hgl::asset::AssetURISegmentToAnsi(uri);
            GLogError(u8"[EnvironmentAssetRegistry] %s", last_error.c_str());
            return nullptr;
        }

        if (EnvironmentAsset *cached = FindAssetByPath(parsed.target))
            return cached;

        LoadFile(parsed.target);

        // 读盘成功但句柄为空的唯一可能是解析/校验失败，`last_error` 已由上层写好
        return FindAssetByPath(parsed.target);
    }

    EnvironmentAsset *EnvironmentAssetRegistry::FindByName(const char *name) const
    {
        if (!name || !name[0])
            return nullptr;

        for (int i = 0; i < assets.GetCount(); ++i)
        {
            EnvironmentAsset *asset = assets.At(i);

            if (asset && !asset->GetEnvironmentName().IsEmpty()
             && hgl::stricmp(asset->GetEnvironmentName().c_str(), name) == 0)
                return asset;
        }

        return nullptr;
    }

    EnvironmentAsset *EnvironmentAssetRegistry::FindAssetByPath(const OSString &path) const
    {
        if (path.IsEmpty())
            return nullptr;

        for (int i = 0; i < assets.GetCount(); ++i)
        {
            EnvironmentAsset *asset = assets.At(i);

            if (asset && asset->GetSourcePath() == path)
                return asset;
        }

        return nullptr;
    }

    EnvironmentAsset *EnvironmentAssetRegistry::FindByResourceId(const U8String &resource_id) const
    {
        for (int i = 0; i < assets.GetCount(); ++i)
        {
            EnvironmentAsset *asset = assets.At(i);

            if (asset && asset->GetResourceId() == resource_id)
                return asset;
        }

        return nullptr;
    }

    EnvironmentAsset *EnvironmentAssetRegistry::GetAssetAt(const int index) const
    {
        if (index < 0 || index >= assets.GetCount())
            return nullptr;

        return assets.At(index);
    }

    void EnvironmentAssetRegistry::Clear()
    {
        last_error.Clear();
        assets.Clear();
    }
}//namespace hgl::graph
