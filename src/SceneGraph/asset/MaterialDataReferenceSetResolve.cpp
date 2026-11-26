#include <hgl/graph/asset/MaterialDataReferenceSetResolve.h>
#include <hgl/graph/asset/MaterialDataReferenceSetFile.h>
#include <hgl/asset/AssetURI.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/utf.h>

namespace hgl::graph::mtl
{
    bool ResolveMaterialDataReferenceSet(const MaterialDataReferenceSet &set,
                                         MaterialDataBlockRegistry &registry,
                                         ValueArray<ResolvedMaterialDataReference> &out,
                                         AnsiString &error)
    {
        if (!ValidateMaterialDataReferenceSet(set, error))
            return false;

        ValueArray<ResolvedMaterialDataReference> resolved;
        resolved.Reserve(set.instances.GetCount());
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
            if (!parsed.IsFile()
                && !parsed.target.IsEmpty() && parsed.target[0] == OS_TEXT('@'))
            {
                const OSString name = parsed.target.SubString(1);
                int matches = 0;
                for (int j = 0; j < registry.GetCount(); ++j)
                {
                    const MaterialDataBlockAsset *candidate = registry.GetAssetAt(j);
                    if (candidate && !candidate->GetAssetName().IsEmpty()
                        && ToOSString(candidate->GetAssetName()) == name)
                        ++matches;
                }
                if (matches > 1)
                {
                    error = AnsiString("ambiguous material data name at instance ")
                          + AnsiString::numberOf(i);
                    return false;
                }
            }

            MaterialDataBlockAsset *asset = registry.GetOrLoadURI(ref->uri);
            if (!asset || asset->IsFailed())
            {
                error = AnsiString("cannot load material data asset at instance ")
                      + AnsiString::numberOf(i);
                return false;
            }
            if (ref->row >= asset->GetRowCount())
            {
                error = AnsiString("material data row out of range at instance ")
                      + AnsiString::numberOf(i);
                return false;
            }
            resolved.Add({asset, ref->row});
        }
        out = resolved;
        return true;
    }
}
