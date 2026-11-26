#include <hgl/graph/asset/MaterialDataReferenceSetResolve.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>

namespace hgl::graph::mtl
{
    bool ResolveMaterialDataReferenceSet(const MaterialDataReferenceSet &set,
                                         MaterialDataBlockRegistry &registry,
                                         ValueArray<ResolvedMaterialDataReference> &out,
                                         AnsiString &error)
    {
        error.Clear();
        if (set.schema != 1)
        {
            error = "unsupported material data reference set schema";
            return false;
        }

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
