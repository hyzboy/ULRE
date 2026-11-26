#include <hgl/graph/asset/MaterialDataReferenceSetFile.h>
#include <hgl/graph/asset/MaterialDataReferenceSetResolve.h>
#include <hgl/graph/asset/MaterialTextureConfigurationFile.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/graph/asset/AssetSourceMount.h>
#include <hgl/asset/AssetID.h>
#include <hgl/asset/AssetSource.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/io/MemoryInputStream.h>
#include <hgl/utf.h>

#include <cstring>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    class MemoryMaterialSource final : public asset::AssetSource
    {
        asset::AssetID set_id;
        asset::AssetID texture_id;
        asset::AssetID stale_set_id;
        asset::AssetID stale_texture_id;
        ValueArray<uint8> &set_bytes;
        ValueArray<uint8> &texture_bytes;

    public:
        MemoryMaterialSource(const asset::AssetID &set, const asset::AssetID &texture,
                             const asset::AssetID &stale_set, const asset::AssetID &stale_texture,
                             ValueArray<uint8> &set_data, ValueArray<uint8> &texture_data)
            : AssetSource("material_memory_test"), set_id(set), texture_id(texture),
              stale_set_id(stale_set), stale_texture_id(stale_texture),
              set_bytes(set_data), texture_bytes(texture_data) {}

        io::InputStream *Open(const asset::AssetID &id) override
        {
            ValueArray<uint8> *data = id == set_id || id == stale_set_id ? &set_bytes
                                    : id == texture_id || id == stale_texture_id ? &texture_bytes : nullptr;
            if (!data) return nullptr;
            ManagedArray<io::MemoryInputStream> streams;
            if (!streams.Create(data->GetData(), data->GetCount())) return nullptr;
            return streams.DetachAt(0);
        }
    };

    bool TestIndexedMemoryMaterialFiles()
    {
        const asset::AssetID set_id = asset::AssetID::Create();
        const asset::AssetID texture_id = asset::AssetID::Create();
        const asset::AssetID stale_set_id = asset::AssetID::Create();
        const asset::AssetID stale_texture_id = asset::AssetID::Create();
        MaterialDataReferenceSet set;
        set.id = set_id.ToString();
        set.instances.Create()->uri = OS_TEXT("mtldata:@oak");
        MaterialTextureConfiguration textures;
        textures.id = texture_id.ToString();
        textures.textures.Create()->uri = OS_TEXT("tex2d:oak.Tex2D");
        ValueArray<uint8> set_bytes, texture_bytes;
        AnsiString error;
        if (!EncodeMaterialDataReferenceSet(set, set_bytes, error)
            || !EncodeMaterialTextureConfiguration(textures, texture_bytes, error))
            return false;
        ManagedArray<MemoryMaterialSource> sources;
        auto *source = sources.Create(set_id, texture_id, stale_set_id, stale_texture_id,
                                      set_bytes, texture_bytes);
        if (!source || !asset::RegisterAssetsSource("material_memory_test", source))
            return false;
        sources.DetachAt(0);
        const OSString prefix = OS_TEXT("pack(material_memory_test):");
        MaterialDataReferenceSet loaded_set;
        MaterialTextureConfiguration loaded_textures;
        const bool ok =
            LoadMaterialDataReferenceSet(prefix + ToOSString(asset::MakeAssetIDTarget(set_id)), loaded_set, error)
            && LoadMaterialTextureConfiguration(prefix + ToOSString(asset::MakeAssetIDTarget(texture_id)), loaded_textures, error)
            && loaded_set.id == set.id && loaded_textures.id == textures.id
            && !LoadMaterialDataReferenceSet(prefix + ToOSString(asset::MakeAssetIDTarget(stale_set_id)), loaded_set, error)
            && error == "material asset ID does not match the requested ID"
            && loaded_set.id == set.id && loaded_set.instances.GetCount() == 1
            && !LoadMaterialTextureConfiguration(prefix + ToOSString(asset::MakeAssetIDTarget(stale_texture_id)), loaded_textures, error)
            && error == "material asset ID does not match the requested ID"
            && loaded_textures.id == textures.id && loaded_textures.textures.GetCount() == 1;
        return asset::UnregisterAssetsSource("material_memory_test") && ok;
    }

    bool TestMountedMaterialFiles()
    {
        const OSString root = OSString(OS_TEXT("material-files-"))
                            + ToOSString(asset::AssetID::Create().ToString());
        const OSString set_path = filesystem::JoinPathWithFilename(root, OS_TEXT("set.mdrs"));
        const OSString texture_path = filesystem::JoinPathWithFilename(root, OS_TEXT("textures.mtcf"));
        if (filesystem::IsDirectory(root) || filesystem::FileExist(root) || !filesystem::MakePath(root))
            return false;

        MaterialDataReferenceSet set;
        set.id = asset::AssetID::Create().ToString();
        set.instances.Create()->uri = OS_TEXT("mtldata:@oak");
        MaterialTextureConfiguration textures;
        textures.id = asset::AssetID::Create().ToString();
        textures.textures.Create()->uri = OS_TEXT("tex2d:oak.Tex2D");
        AnsiString error;
        bool ok = CreateMaterialDataReferenceSetFile(set_path, set, error)
               && CreateMaterialTextureConfigurationFile(texture_path, textures, error);
        AssetSourceMountConfig config;
        config.name = "material_files_test";
        config.root = root;
        config.load_manifest = false;
        config.save_manifest = false;
        const bool mounted = ok && MountAssetSource(config, &error);
        ok = mounted && ok;
        MaterialDataReferenceSet loaded_set;
        MaterialTextureConfiguration loaded_textures;
        if (ok)
            ok = LoadMaterialDataReferenceSet(OS_TEXT("pack(material_files_test):set.mdrs"), loaded_set, error)
              && LoadMaterialTextureConfiguration(OS_TEXT("pack(material_files_test):textures.mtcf"), loaded_textures, error)
              && loaded_set.id == set.id && loaded_set.instances.GetCount() == 1
              && loaded_textures.id == textures.id && loaded_textures.textures.GetCount() == 1
              && !LoadMaterialDataReferenceSet(OS_TEXT("pack(material_files_test):missing.mdrs"), loaded_set, error)
              && !error.IsEmpty() && loaded_set.id == set.id
              && !LoadMaterialDataReferenceSet(OS_TEXT("geo:pack(material_files_test):set.mdrs"), loaded_set, error)
              && !error.IsEmpty() && loaded_set.id == set.id;
        if (mounted)
            UnmountAssetSource(config.name);
        if (filesystem::FileExist(set_path))
            ok = filesystem::FileDelete(set_path) && ok;
        if (filesystem::FileExist(texture_path))
            ok = filesystem::FileDelete(texture_path) && ok;
        if (!filesystem::DeletePath(root))
            ok = false;
        return ok;
    }

    bool TestPersistentReference()
    {
        const asset::AssetID id = asset::AssetID::Create();
        const OSString root = OSString(OS_TEXT("material-ref-id-")) + ToOSString(id.ToString());
        const OSString first_path = filesystem::JoinPathWithFilename(root, OS_TEXT("first.material_data.toml"));
        const OSString second_path = filesystem::JoinPathWithFilename(root, OS_TEXT("renamed.material_data.toml"));
        if (filesystem::IsDirectory(root) || filesystem::FileExist(root)
            || !filesystem::MakePath(root))
            return false;

        const AnsiString text = AnsiString("id = \"") + id.ToString()
            + "\"\nschema = 1\nname = \"persistent\"\ntarget = \"PBRSurface\"\n"
              "[[rows]]\nmetallic = 0.1\n[[rows]]\nmetallic = 0.2\n";
        const int64 size = static_cast<int64>(strlen(text.c_str()));
        bool ok = filesystem::SaveMemoryToFile(first_path, text.c_str(), size) == size;

        AssetSourceMountConfig config;
        config.name = "material_ref_id_test";
        config.root = root;
        config.load_manifest = false;
        config.save_manifest = false;
        AnsiString error;
        const OSString uri = OSString(OS_TEXT("mtldata:pack(material_ref_id_test):"))
                           + ToOSString(asset::MakeAssetIDTarget(id));

        for (int pass = 0; ok && pass < 2; ++pass)
        {
            if (pass == 1)
                ok = filesystem::FileRename(second_path, first_path);
            if (!ok || !MountAssetSource(config, &error))
            {
                ok = false;
                break;
            }

            mtl::MaterialDataBlockRegistry registry;
            MaterialDataReferenceSet set;
            set.id = asset::AssetID::Create().ToString();
            set.instances.Create()->uri = uri;
            auto *second = set.instances.Create();
            second->uri = uri;
            second->row = 1;
            ValueArray<mtl::ResolvedMaterialDataReference> resolved;
            ok = mtl::ResolveMaterialDataReferenceSet(set, registry, resolved, error)
              && resolved.GetCount() == 2 && resolved[0].asset == resolved[1].asset
              && resolved[0].asset->GetSpec().id == id.ToString()
              && resolved[1].row == 1;
            if (!ok)
                GLogError(u8"Persistent reference pass %d failed: %s", pass, error.c_str());
            UnmountAssetSource(config.name);
        }

        if (filesystem::FileExist(first_path))
            ok = filesystem::FileDelete(first_path) && ok;
        if (filesystem::FileExist(second_path))
            ok = filesystem::FileDelete(second_path) && ok;
        return filesystem::DeletePath(root) && ok;
    }

    bool TestStaleBlockIndex()
    {
        const asset::AssetID indexed_id = asset::AssetID::Create();
        const asset::AssetID actual_id = asset::AssetID::Create();
        const OSString root = OSString(OS_TEXT("material-stale-index-"))
                            + ToOSString(indexed_id.ToString());
        const OSString path = filesystem::JoinPathWithFilename(root, OS_TEXT("block.material_data.toml"));
        if (filesystem::IsDirectory(root) || filesystem::FileExist(root) || !filesystem::MakePath(root))
            return false;
        const AnsiString suffix = "\"\nschema = 1\nname = \"block\"\ntarget = \"PBRSurface\"\n"
                                  "[[rows]]\nmetallic = 0.1\n";
        const AnsiString indexed = AnsiString("id = \"") + indexed_id.ToString() + suffix;
        const AnsiString actual = AnsiString("id = \"") + actual_id.ToString() + suffix;
        const int64 indexed_size = static_cast<int64>(strlen(indexed.c_str()));
        const int64 actual_size = static_cast<int64>(strlen(actual.c_str()));
        bool ok = filesystem::SaveMemoryToFile(path, indexed.c_str(), indexed_size) == indexed_size;
        AssetSourceMountConfig config;
        config.name = "material_stale_index_test";
        config.root = root;
        config.load_manifest = false;
        config.save_manifest = false;
        AnsiString error;
        const bool mounted = ok && MountAssetSource(config, &error);
        ok = mounted && filesystem::SaveMemoryToFile(path, actual.c_str(), actual_size) == actual_size;
        if (ok)
        {
            const OSString uri = OSString(OS_TEXT("mtldata:pack(material_stale_index_test):"))
                               + ToOSString(asset::MakeAssetIDTarget(indexed_id));
            mtl::MaterialDataBlockRegistry uncached;
            mtl::MaterialDataBlockRegistry cached;
            auto *physical = cached.GetOrLoadURI(path);
            ok = physical && !physical->IsFailed() && physical->GetSpec().id == actual_id.ToString()
              && !uncached.GetOrLoadURI(uri) && !cached.GetOrLoadURI(uri)
              && cached.GetOrLoadURI(path) == physical;
        }
        if (mounted)
            UnmountAssetSource(config.name);
        if (filesystem::FileExist(path))
            ok = filesystem::FileDelete(path) && ok;
        return filesystem::DeletePath(root) && ok;
    }
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestMaterialDataReferenceSet"));
    if (!TestMountedMaterialFiles() || !TestIndexedMemoryMaterialFiles())
    {
        GLogError(u8"Mounted material asset file loading failed");
        return 14;
    }
    MaterialDataReferenceSet original;
    original.id = asset::AssetID::Create().ToString();
    auto *first = original.instances.Create();
    first->uri = OS_TEXT("mtldata:@oak");
    first->row = 3;
    auto *second = original.instances.Create();
    second->uri = OS_TEXT("mtldata:file:assets/wood.material_data.fb");
    second->row = 1;
    auto *third = original.instances.Create();
    third->uri = first->uri;
    third->row = first->row;

    AnsiString error;
    ValueArray<uint8> encoded;
    MaterialDataReferenceSet decoded;
    if (!EncodeMaterialDataReferenceSet(original, encoded, error)
        || !DecodeMaterialDataReferenceSet(encoded.GetData(), encoded.GetCount(), decoded, error)
        || decoded.id != original.id || decoded.instances.GetCount() != 3
        || decoded.instances[0]->uri != first->uri || decoded.instances[0]->row != 3
        || decoded.instances[1]->uri != second->uri || decoded.instances[1]->row != 1
        || decoded.instances[2]->uri != first->uri || decoded.instances[2]->row != 3)
    {
        GLogError(u8"Material reference set round trip failed: %s", error.c_str());
        return 1;
    }

    if (DecodeMaterialDataReferenceSet(encoded.GetData(), 4, decoded, error)
        || decoded.instances.GetCount() != 3 || error.IsEmpty())
    {
        GLogError(u8"Truncated data was not rejected without changing the result");
        return 2;
    }
    original.instances[1]->uri = OS_TEXT("geo:builtin:cube");
    if (EncodeMaterialDataReferenceSet(original, encoded, error) || error.IsEmpty())
    {
        GLogError(u8"Wrong asset type was not rejected");
        return 3;
    }
    original.instances[1]->uri = OS_TEXT("mtldata:file:assets/wood.material_data.fb");
    original.schema = 1;
    if (EncodeMaterialDataReferenceSet(original, encoded, error) || error.IsEmpty())
    {
        GLogError(u8"Unsupported schema was not rejected");
        return 6;
    }
    original.schema = 2;
    const AnsiString saved_id = original.id;
    original.id = "invalid";
    if (EncodeMaterialDataReferenceSet(original, encoded, error) || error.IsEmpty())
    {
        GLogError(u8"Invalid material reference set asset ID was not rejected");
        return 13;
    }
    original.id = saved_id;

    const OSString path = OS_TEXT("TestMaterialDataReferenceSet.tmp");
    if (filesystem::FileExist(path))
    {
        GLogError(u8"Refusing to overwrite an existing test file");
        return 4;
    }
    const bool saved = CreateMaterialDataReferenceSetFile(path, original, error);
    MaterialDataReferenceSet loaded;
    const bool loaded_ok = saved && LoadMaterialDataReferenceSet(path, loaded, error);
    original.instances[0]->row = 42;
    const bool overwrite_rejected = saved && !CreateMaterialDataReferenceSetFile(path, original, error);
    MaterialDataReferenceSet unchanged;
    const bool unchanged_ok = overwrite_rejected && LoadMaterialDataReferenceSet(path, unchanged, error)
                           && unchanged.id == saved_id && unchanged.instances.GetCount() == 3
                           && unchanged.instances[0]->row == 3;
    const bool removed = saved && filesystem::FileDelete(path);
    if (!loaded_ok || !unchanged_ok || !removed || loaded.instances.GetCount() != 3
        || loaded.instances[2]->row != 3 || loaded.instances[2]->uri != first->uri)
    {
        GLogError(u8"Material reference set file round trip failed: %s", error.c_str());
        return 5;
    }

    const char block[] =
        "schema = 1\nname = \"oak\"\ntarget = \"PBRSurface\"\n"
        "[[rows]]\nmetallic = 0.1\n[[rows]]\nmetallic = 0.2\n";
    mtl::MaterialDataBlockRegistry registry;
    auto *asset = registry.RegisterFromToml(OS_TEXT("oak.material_data.toml"),
                                             block, static_cast<int>(strlen(block)));
    if (!asset || asset->IsFailed())
        return 7;
    MaterialDataReferenceSet refs;
    refs.id = asset::AssetID::Create().ToString();
    refs.instances.Create()->uri = OS_TEXT("mtldata:@oak");
    auto *other = refs.instances.Create();
    other->uri = OS_TEXT("mtldata:@oak");
    other->row = 1;
    refs.instances.Create()->uri = OS_TEXT("mtldata:@oak");
    ValueArray<mtl::ResolvedMaterialDataReference> resolved;
    if (!mtl::ResolveMaterialDataReferenceSet(refs, registry, resolved, error)
        || resolved.GetCount() != 3 || resolved[0].asset != asset
        || resolved[1].asset != asset || resolved[1].row != 1
        || resolved[2].asset != asset || resolved[2].row != 0)
    {
        GLogError(u8"Material reference resolution failed: %s", error.c_str());
        return 8;
    }
    const AnsiString refs_id = refs.id;
    refs.id = "invalid";
    if (mtl::ResolveMaterialDataReferenceSet(refs, registry, resolved, error)
        || error.IsEmpty() || resolved.GetCount() != 3 || resolved[1].asset != asset)
    {
        GLogError(u8"Invalid reference set ID did not preserve the previous result");
        return 14;
    }
    refs.id = refs_id;
    refs.schema = 1;
    if (mtl::ResolveMaterialDataReferenceSet(refs, registry, resolved, error)
        || error.IsEmpty() || resolved.GetCount() != 3 || resolved[1].asset != asset)
    {
        GLogError(u8"Unsupported reference set schema did not preserve the previous result");
        return 15;
    }
    refs.schema = MaterialDataReferenceSetSchema;
    other->row = 2;
    if (mtl::ResolveMaterialDataReferenceSet(refs, registry, resolved, error)
        || error.IsEmpty() || resolved.GetCount() != 3 || resolved[1].row != 1)
    {
        GLogError(u8"Out-of-range resolution did not preserve the previous result");
        return 9;
    }
    other->uri = OS_TEXT("mtldata:@missing");
    if (mtl::ResolveMaterialDataReferenceSet(refs, registry, resolved, error)
        || error.IsEmpty() || resolved.GetCount() != 3 || resolved[1].row != 1)
    {
        GLogError(u8"Missing asset did not preserve the previous result");
        return 10;
    }
    other->uri = OS_TEXT("mtldata:@oak");
    other->row = 1;
    registry.RegisterFromToml(OS_TEXT("other-oak.material_data.toml"),
                              block, static_cast<int>(strlen(block)));
    if (mtl::ResolveMaterialDataReferenceSet(refs, registry, resolved, error)
        || error.IsEmpty() || resolved.GetCount() != 3 || resolved[1].asset != asset)
    {
        GLogError(u8"Ambiguous asset name was not rejected");
        return 11;
    }
    refs.instances[0]->uri = OS_TEXT("mtldata:pack(res):@oak");
    if (mtl::ResolveMaterialDataReferenceSet(refs, registry, resolved, error)
        || error != "ambiguous material data name at instance 0"
        || resolved.GetCount() != 3 || resolved[0].asset != asset)
    {
        GLogError(u8"Ambiguous asset name with a pack source was not rejected");
        return 12;
    }
    if (!TestPersistentReference())
    {
        GLogError(u8"Persistent material data ID reference failed after remount or rename");
        return 16;
    }
    if (!TestStaleBlockIndex())
    {
        GLogError(u8"Stale material data block index was not rejected");
        return 17;
    }
    return 0;
}
