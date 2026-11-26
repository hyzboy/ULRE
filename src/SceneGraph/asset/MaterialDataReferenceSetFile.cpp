#include <hgl/graph/asset/MaterialDataReferenceSetFile.h>
#include <hgl/asset/AssetURI.h>
#include <hgl/asset/AssetID.h>
#include <hgl/asset/generated/MaterialDataReferenceSetSerialize.gen.h>
#include <hgl/serialize/FlatCodec.h>
#include "FlatMaterialAssetFile.h"

namespace hgl::graph
{
    namespace
    {
        constexpr char kIdentifier[] = "MDRS";
    }

    bool ValidateMaterialDataReferenceSet(const MaterialDataReferenceSet &set, AnsiString &error)
    {
        error.Clear();
        if (set.schema != MaterialDataReferenceSetSchema)
        {
            error = "unsupported material data reference set schema";
            return false;
        }
        asset::AssetID id;
        if (!asset::AssetID::Parse(set.id, id))
        {
            error = "invalid material data reference set asset ID";
            return false;
        }

        for (int i = 0; i < set.instances.GetCount(); ++i)
        {
            const MaterialDataReference *ref = set.instances.At(i);
            if (!ref || ref->uri.IsEmpty())
            {
                error = AnsiString("missing material data reference at instance ")
                      + AnsiString::numberOf(i);
                return false;
            }

            const asset::AssetURI parsed =
                asset::AssetURI::Parse(ref->uri, asset::AssetType::MaterialData);
            if (!parsed.IsValid() || parsed.type != asset::AssetType::MaterialData
                || parsed.source == asset::AssetSourceKind::RenderTarget
                || parsed.source == asset::AssetSourceKind::Builtin
                || (parsed.target == OS_TEXT("@")))
            {
                error = AnsiString("invalid material data URI at instance ")
                      + AnsiString::numberOf(i);
                return false;
            }
        }
        return true;
    }

    bool EncodeMaterialDataReferenceSet(const MaterialDataReferenceSet &set,
                                        ValueArray<uint8> &out, AnsiString &error)
    {
        out.Clear();
        error.Clear();
        if (!ValidateMaterialDataReferenceSet(set, error))
            return false;
        if (!serialize::fb::EncodeToFlatBuffers(out, set, kIdentifier, &error))
        {
            if (error.IsEmpty())
                error = "failed to encode material data reference set";
            return false;
        }
        if (out.GetCount() > detail::MaterialAssetMaximumFileSize)
        {
            out.Clear();
            error = "material data reference set exceeds maximum file size";
            return false;
        }
        return true;
    }

    bool DecodeMaterialDataReferenceSet(const void *data, size_t size,
                                        MaterialDataReferenceSet &out, AnsiString &error)
    {
        error.Clear();
        MaterialDataReferenceSet decoded;
        if (!data || size > static_cast<size_t>(detail::MaterialAssetMaximumFileSize)
            || !serialize::fb::VerifyAndDecodeFlatBuffers(data, size, decoded, kIdentifier, &error))
        {
            if (error.IsEmpty())
                error = "invalid material data reference set buffer";
            return false;
        }
        if (!ValidateMaterialDataReferenceSet(decoded, error))
            return false;
        out = static_cast<MaterialDataReferenceSet &&>(decoded);
        return true;
    }

    bool LoadMaterialDataReferenceSet(const OSString &path,
                                      MaterialDataReferenceSet &out, AnsiString &error)
    {
        return detail::LoadFlatMaterialAsset(path, out, error, DecodeMaterialDataReferenceSet);
    }

    bool CreateMaterialDataReferenceSetFile(const OSString &path,
                                            const MaterialDataReferenceSet &set, AnsiString &error)
    {
        ValueArray<uint8> bytes;
        if (!EncodeMaterialDataReferenceSet(set, bytes, error))
            return false;
        return detail::CreateFlatMaterialAsset(path, bytes, error);
    }
}
