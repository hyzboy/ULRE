#include <hgl/graph/asset/MaterialDataReferenceSetFile.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/log/Log.h>

using namespace hgl;
using namespace hgl::graph;

int main()
{
    MaterialDataReferenceSet original;
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
        || decoded.instances.GetCount() != 3
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
    original.schema = 2;
    if (EncodeMaterialDataReferenceSet(original, encoded, error) || error.IsEmpty())
    {
        GLogError(u8"Unsupported schema was not rejected");
        return 6;
    }
    original.schema = 1;

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
                           && unchanged.instances.GetCount() == 3 && unchanged.instances[0]->row == 3;
    const bool removed = saved && filesystem::FileDelete(path);
    if (!loaded_ok || !unchanged_ok || !removed || loaded.instances.GetCount() != 3
        || loaded.instances[2]->row != 3 || loaded.instances[2]->uri != first->uri)
    {
        GLogError(u8"Material reference set file round trip failed: %s", error.c_str());
        return 5;
    }
    return 0;
}
