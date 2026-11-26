#pragma once

#include <hgl/filesystem/FileSystem.h>
#include <hgl/graph/asset/AssetSourceMount.h>
#include <hgl/io/InputStream.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/type/Smart.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::detail
{
    constexpr int64 MaterialAssetMaximumFileSize = 64 * 1024 * 1024;

    template<typename T>
    bool LoadFlatMaterialAsset(const OSString &path, T &out, AnsiString &error,
                               bool (*decode)(const void *, size_t, T &, AnsiString &))
    {
        error.Clear();
        const asset::AssetURI uri = asset::AssetURI::Parse(path, asset::AssetType::Unknown);
        if (uri.HasType())
        {
            error = "material asset URI has an incompatible type";
            return false;
        }
        AutoDelete<io::InputStream> input(OpenAssetInputStream(path, asset::AssetType::Unknown, &error));
        if (!input)
        {
            if (error.IsEmpty())
                error = "cannot open material asset";
            return false;
        }
        const int64 size = input->GetSize();
        if (size <= 0 || size > MaterialAssetMaximumFileSize)
        {
            error = "invalid material asset file size";
            return false;
        }
        AutoDeleteArray<uint8> bytes(static_cast<size_t>(size));
        if (!bytes || input->Read(bytes.data(), size) != size)
        {
            error = "cannot read material asset";
            return false;
        }
        T decoded;
        if (!decode(bytes.data(), static_cast<size_t>(size), decoded, error))
            return false;
        asset::AssetID requested;
        if (uri.IsPack() && asset::ParseAssetIDTarget(ToU8String(uri.target), requested))
        {
            asset::AssetID actual;
            if (!asset::AssetID::Parse(decoded.id, actual) || actual != requested)
            {
                error = "material asset ID does not match the requested ID";
                return false;
            }
        }
        out = static_cast<T &&>(decoded);
        return true;
    }

    inline bool CreateFlatMaterialAsset(const OSString &path, const ValueArray<uint8> &bytes,
                                        AnsiString &error)
    {
        AutoDelete<io::FileOutputStream> output(
            io::CreateFileOutputStream(path, io::FileOpenMode::Create));
        if (!output)
        {
            error = "cannot create material asset";
            return false;
        }
        if (output->Write(bytes.GetData(), bytes.GetCount()) != bytes.GetCount())
        {
            error = "cannot write material asset";
            output->Close();
            if (!filesystem::FileDelete(path))
                error += "; cannot remove incomplete file";
            return false;
        }
        return true;
    }
}
