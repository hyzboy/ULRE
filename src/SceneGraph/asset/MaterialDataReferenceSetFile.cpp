#include <hgl/graph/asset/MaterialDataReferenceSetFile.h>
#include <hgl/asset/AssetURI.h>
#include <hgl/asset/generated/MaterialDataReferenceSetSerialize.gen.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/type/Smart.h>

namespace hgl::graph
{
    namespace
    {
        constexpr char kIdentifier[] = "MDRS";
        constexpr int64 kMaximumFileSize = 64 * 1024 * 1024;

        bool Validate(const MaterialDataReferenceSet &set, AnsiString &error)
        {
            if (set.schema != 1)
            {
                error = "unsupported material data reference set schema";
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
    }

    bool EncodeMaterialDataReferenceSet(const MaterialDataReferenceSet &set,
                                        ValueArray<uint8> &out, AnsiString &error)
    {
        out.Clear();
        error.Clear();
        if (!Validate(set, error))
            return false;
        if (!serialize::fb::EncodeToFlatBuffers(out, set, kIdentifier, &error))
        {
            if (error.IsEmpty())
                error = "failed to encode material data reference set";
            return false;
        }
        if (out.GetCount() > kMaximumFileSize)
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
        if (!data || size > static_cast<size_t>(kMaximumFileSize)
            || !serialize::fb::VerifyAndDecodeFlatBuffers(data, size, decoded, kIdentifier, &error))
        {
            if (error.IsEmpty())
                error = "invalid material data reference set buffer";
            return false;
        }
        if (!Validate(decoded, error))
            return false;
        out = static_cast<MaterialDataReferenceSet &&>(decoded);
        return true;
    }

    bool LoadMaterialDataReferenceSet(const OSString &path,
                                      MaterialDataReferenceSet &out, AnsiString &error)
    {
        error.Clear();
        io::OpenFileInputStream input(path);
        if (!input)
        {
            error = "cannot open material data reference set";
            return false;
        }
        const int64 size = input->GetSize();
        if (size <= 0 || size > kMaximumFileSize)
        {
            error = "invalid material data reference set file size";
            return false;
        }
        AutoDeleteArray<uint8> bytes(static_cast<size_t>(size));
        if (!bytes || input->Read(bytes.data(), size) != size)
        {
            error = "cannot read material data reference set";
            return false;
        }
        return DecodeMaterialDataReferenceSet(bytes.data(), static_cast<size_t>(size), out, error);
    }

    bool CreateMaterialDataReferenceSetFile(const OSString &path,
                                            const MaterialDataReferenceSet &set, AnsiString &error)
    {
        ValueArray<uint8> bytes;
        if (!EncodeMaterialDataReferenceSet(set, bytes, error))
            return false;
        AutoDelete<io::FileOutputStream> output(
            io::CreateFileOutputStream(path, io::FileOpenMode::Create));
        if (!output)
        {
            error = "cannot create material data reference set";
            return false;
        }
        if (output->Write(bytes.GetData(), bytes.GetCount()) != bytes.GetCount())
        {
            error = "cannot write material data reference set";
            output->Close();
            if (!filesystem::FileDelete(path))
                error += "; cannot remove incomplete file";
            return false;
        }
        return true;
    }
}
