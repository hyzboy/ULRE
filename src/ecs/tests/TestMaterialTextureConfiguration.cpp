#include <hgl/graph/asset/MaterialTextureConfigurationFile.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/asset/AssetID.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/log/Log.h>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    bool TestMipmapHandleIdentity()
    {
        TextureManager manager(nullptr);
        TextureAsset *plain = manager.CreateTextureAsset(OS_TEXT("mipmap-test-a.Tex2D"), false);
        TextureAsset *generated = manager.CreateTextureAsset(OS_TEXT("mipmap-test-a.Tex2D"), true);
        TextureAsset *generated_first = manager.CreateTextureAsset(OS_TEXT("mipmap-test-b.Tex2D"), true);
        TextureAsset *plain_second = manager.CreateTextureAsset(OS_TEXT("mipmap-test-b.Tex2D"), false);
        const bool ok = plain && generated && generated_first && plain_second
            && plain != generated && generated_first != plain_second
            && !plain->GetAutoMipmaps() && generated->GetAutoMipmaps()
            && generated_first->GetAutoMipmaps() && !plain_second->GetAutoMipmaps()
            && plain->GetResourceId() != generated->GetResourceId()
            && generated_first->GetResourceId() != plain_second->GetResourceId()
            && manager.CreateTextureAsset(OS_TEXT("mipmap-test-a.Tex2D"), false) == plain
            && manager.CreateTextureAsset(OS_TEXT("mipmap-test-a.Tex2D"), true) == generated;
        manager.Release();
        return ok;
    }
}

int main()
{
    if (!TestMipmapHandleIdentity())
    {
        GLogError(u8"Mipmap loading options did not produce distinct texture identities");
        return 9;
    }
    MaterialTextureConfiguration original;
    original.id = asset::AssetID::Create().ToString();
    auto *first = original.textures.Create();
    first->uri = OS_TEXT("tex2d:assets/oak.Tex2D");
    first->auto_mipmaps = true;
    auto *second = original.textures.Create();
    second->uri = OS_TEXT("texarray:@bark");
    second->array_layer = 3;
    original.textures.Create();

    AnsiString error;
    ValueArray<uint8> encoded;
    MaterialTextureConfiguration decoded;
    if (!EncodeMaterialTextureConfiguration(original, encoded, error)
        || !DecodeMaterialTextureConfiguration(encoded.GetData(), encoded.GetCount(), decoded, error)
        || decoded.id != original.id || decoded.textures.GetCount() != 3
        || decoded.textures[0]->uri != first->uri || !decoded.textures[0]->auto_mipmaps
        || decoded.textures[1]->uri != second->uri || decoded.textures[1]->array_layer != 3
        || !decoded.textures[2]->uri.IsEmpty()
        || !HasSameTextureReferences(original, decoded))
    {
        GLogError(u8"Material texture configuration round trip failed: %s", error.c_str());
        return 1;
    }

    decoded.textures[0]->uri = OS_TEXT("tex2d:assets/willow.Tex2D");
    const bool detects_uri_change = !HasSameTextureReferences(original, decoded);
    decoded.textures[0]->uri = first->uri;
    decoded.textures[0]->auto_mipmaps = false;
    const bool detects_mipmap_change = !HasSameTextureReferences(original, decoded);
    decoded.textures[0]->auto_mipmaps = true;
    decoded.textures[1]->array_layer = 0;
    const bool detects_layer_change = !HasSameTextureReferences(original, decoded);
    decoded.textures[1]->array_layer = 3;
    MaterialTextureConfiguration shorter;
    shorter.textures.Create()->uri = first->uri;
    if (!detects_uri_change || !detects_mipmap_change || !detects_layer_change
        || !HasSameTextureReferences(original, decoded)
        || HasSameTextureReferences(original, shorter))
    {
        GLogError(u8"Material texture reference comparison did not compare the whole list");
        return 7;
    }

    if (DecodeMaterialTextureConfiguration(encoded.GetData(), 4, decoded, error)
        || decoded.textures.GetCount() != 3 || error.IsEmpty())
    {
        GLogError(u8"Truncated texture configuration was not rejected");
        return 2;
    }
    original.schema = 1;
    if (EncodeMaterialTextureConfiguration(original, encoded, error) || error.IsEmpty())
    {
        GLogError(u8"Unsupported texture configuration schema was not rejected");
        return 3;
    }
    original.schema = 2;
    const AnsiString saved_id = original.id;
    original.id = "invalid";
    if (EncodeMaterialTextureConfiguration(original, encoded, error) || error.IsEmpty())
    {
        GLogError(u8"Invalid texture configuration asset ID was not rejected");
        return 8;
    }
    original.id = saved_id;
    second->uri = OS_TEXT("texarray:");
    if (EncodeMaterialTextureConfiguration(original, encoded, error) || error.IsEmpty())
    {
        GLogError(u8"Invalid texture reference was not rejected");
        return 4;
    }
    second->uri = OS_TEXT("texarray:@bark");

    const OSString path = OS_TEXT("TestMaterialTextureConfiguration.tmp");
    if (filesystem::FileExist(path))
    {
        GLogError(u8"Refusing to overwrite an existing test file");
        return 5;
    }
    const bool saved = CreateMaterialTextureConfigurationFile(path, original, error);
    MaterialTextureConfiguration loaded;
    const bool loaded_ok = saved && LoadMaterialTextureConfiguration(path, loaded, error);
    original.textures[0]->uri = OS_TEXT("tex2d:assets/willow.Tex2D");
    const bool overwrite_rejected = saved && !CreateMaterialTextureConfigurationFile(path, original, error);
    MaterialTextureConfiguration unchanged;
    const bool unchanged_ok = overwrite_rejected && LoadMaterialTextureConfiguration(path, unchanged, error)
                           && unchanged.id == saved_id && unchanged.textures.GetCount() == 3
                           && unchanged.textures[0]->uri == OS_TEXT("tex2d:assets/oak.Tex2D");
    const bool removed = saved && filesystem::FileDelete(path);
    if (!loaded_ok || !unchanged_ok || !removed || loaded.textures.GetCount() != 3
        || loaded.textures[1]->array_layer != 3)
    {
        GLogError(u8"Material texture configuration file round trip failed: %s", error.c_str());
        return 6;
    }
    return 0;
}
