#include<hgl/asset/AssetFactory.h>

#include<hgl/log/Log.h>

DEFINE_LOGGER_MODULE(AssetFactory)
USE_MODULE_LOGGER(AssetFactory)

namespace hgl::asset
{
    // ── GenericAssetObject ──────────────────────────────────────────────────

    GenericAssetObject::GenericAssetObject(const AssetURI &u)
        : uri(u)
    {
        // 资产名取 target 的 ANSI 视图：降级对象不读内容，只能给出定位本身。
        asset_name = AssetURISegmentToAnsi(uri.target);
    }

    // ── AssetFactory ────────────────────────────────────────────────────────

    AssetFactory &AssetFactory::Instance()
    {
        static AssetFactory instance;
        return instance;
    }

    bool AssetFactory::Register(AssetType type, AssetLoaderFn fn)
    {
        if (fn == nullptr)
            return false;

        if (!RangeCheck<AssetType>(type))
            return false;

        loaders[static_cast<uint8_t>(type)] = fn;
        return true;
    }

    bool AssetFactory::Unregister(AssetType type)
    {
        if (!RangeCheck<AssetType>(type))
            return false;

        loaders[static_cast<uint8_t>(type)] = nullptr;
        return true;
    }

    bool AssetFactory::IsRegistered(AssetType type) const
    {
        if (!RangeCheck<AssetType>(type))
            return false;

        return loaders[static_cast<uint8_t>(type)] != nullptr;
    }

    uint32_t AssetFactory::GetRegisteredCount() const
    {
        uint32_t count = 0;

        for (uint8_t i = 0; i < static_cast<uint8_t>(AssetType::RANGE_SIZE); i++)
            if (loaders[i])
                count++;

        return count;
    }

    void AssetFactory::Clear()
    {
        for (uint8_t i = 0; i < static_cast<uint8_t>(AssetType::RANGE_SIZE); i++)
            loaders[i] = nullptr;
    }

    AssetObjectPtr AssetFactory::Create(const AssetURI &uri,
                                        const AssetLoadOptions &options,
                                        AssetLoadStatus *out_status)
    {
        AssetLoadStatus status = AssetLoadStatus::Fallback;

        AssetType type = uri.type;

        if (type == AssetType::Unknown)
            type = options.default_type;

        AssetLoaderFn fn = nullptr;

        if (RangeCheck<AssetType>(type))
            fn = loaders[static_cast<uint8_t>(type)];

        if (fn)
        {
            AssetObjectPtr obj;
            AnsiString error;

            if (fn(uri, obj, error))
            {
                if (out_status)
                    *out_status = AssetLoadStatus::Ok;

                return obj;
            }

            MLogError(AssetFactory,
                      u8"资产加载失败（type=\"%s\" target=\"%s\"）：%s",
                      GetAssetTypeName(type),
                      AssetURISegmentToAnsi(uri.target).c_str(),
                      error.c_str());

            if (out_status)
                *out_status = AssetLoadStatus::ParseFailed;

            return AssetObjectPtr();
        }

        // 类型未注册：降级而不是报错。理由见头文件类注释。
        if (out_status)
            *out_status = AssetLoadStatus::Fallback;

        if (!options.allow_fallback)
        {
            MLogWarning(AssetFactory,
                        u8"资产类型未注册且禁止降级（type=\"%s\"，原始未注册名=\"%s\"）",
                        GetAssetTypeName(type),
                        uri.type_name.c_str());

            return AssetObjectPtr();
        }

        return AssetObjectPtr(new GenericAssetObject(uri));
    }

    AssetObjectPtr AssetFactory::Create(const OSString &uri_text,
                                        const AssetLoadOptions &options,
                                        AssetLoadStatus *out_status)
    {
        return Create(AssetURI::Parse(uri_text, options.default_type),
                      options,
                      out_status);
    }

    uint32_t AssetFactory::Create(const OSString *uri_texts, uint32_t count,
                                  AssetObjectPtr *out_objects,
                                  const AssetLoadOptions &options)
    {
        if (uri_texts == nullptr || count == 0)
            return 0;

        uint32_t ok_count = 0;

        for (uint32_t i = 0; i < count; i++)
        {
            AssetLoadStatus status = AssetLoadStatus::ParseFailed;
            AssetObjectPtr obj = Create(uri_texts[i], options, &status);

            if (obj)
                ok_count++;

            if (out_objects)
                out_objects[i] = obj;
        }

        return ok_count;
    }
}//namespace hgl::asset