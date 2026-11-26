#include <hgl/graph/asset/MaterialTextureConfigurationFile.h>
#include <hgl/graph/asset/TextureURI.h>
#include <hgl/asset/AssetID.h>
#include <hgl/asset/generated/MaterialTextureConfigurationSerialize.gen.h>
#include <hgl/serialize/FlatCodec.h>
#include "FlatMaterialAssetFile.h"

namespace hgl::graph
{
    namespace
    {
        constexpr char kIdentifier[] = "MTCF";

        bool Validate(const MaterialTextureConfiguration &configuration, AnsiString &error)
        {
            if (configuration.schema != MaterialTextureConfigurationSchema)
            {
                error = "unsupported material texture configuration schema";
                return false;
            }
            asset::AssetID id;
            if (!asset::AssetID::Parse(configuration.id, id))
            {
                error = "invalid material texture configuration asset ID";
                return false;
            }
            for (int i = 0; i < configuration.textures.GetCount(); ++i)
            {
                const MaterialTextureAssetReference *texture = configuration.textures.At(i);
                if (!texture || (!texture->uri.IsEmpty() && !TextureURI::Parse(texture->uri).IsValid()))
                {
                    error = AnsiString("invalid texture reference at slot ") + AnsiString::numberOf(i);
                    return false;
                }
            }
            return true;
        }
    }

    bool EncodeMaterialTextureConfiguration(const MaterialTextureConfiguration &configuration,
                                            ValueArray<uint8> &out, AnsiString &error)
    {
        out.Clear();
        error.Clear();
        if (!Validate(configuration, error))
            return false;
        if (!serialize::fb::EncodeToFlatBuffers(out, configuration, kIdentifier, &error))
        {
            if (error.IsEmpty())
                error = "failed to encode material texture configuration";
            return false;
        }
        if (out.GetCount() > detail::MaterialAssetMaximumFileSize)
        {
            out.Clear();
            error = "material texture configuration exceeds maximum file size";
            return false;
        }
        return true;
    }

    bool DecodeMaterialTextureConfiguration(const void *data, size_t size,
                                            MaterialTextureConfiguration &out, AnsiString &error)
    {
        error.Clear();
        MaterialTextureConfiguration decoded;
        if (!data || size > static_cast<size_t>(detail::MaterialAssetMaximumFileSize)
            || !serialize::fb::VerifyAndDecodeFlatBuffers(data, size, decoded, kIdentifier, &error))
        {
            if (error.IsEmpty())
                error = "invalid material texture configuration buffer";
            return false;
        }
        if (!Validate(decoded, error))
            return false;
        out = static_cast<MaterialTextureConfiguration &&>(decoded);
        return true;
    }

    bool LoadMaterialTextureConfiguration(const OSString &path,
                                          MaterialTextureConfiguration &out, AnsiString &error)
    {
        return detail::LoadFlatMaterialAsset(path, out, error, DecodeMaterialTextureConfiguration);
    }

    bool CreateMaterialTextureConfigurationFile(const OSString &path,
                                                const MaterialTextureConfiguration &configuration,
                                                AnsiString &error)
    {
        ValueArray<uint8> bytes;
        if (!EncodeMaterialTextureConfiguration(configuration, bytes, error))
            return false;
        return detail::CreateFlatMaterialAsset(path, bytes, error);
    }
}
