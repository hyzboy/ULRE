#pragma once

#include <cstdio>
#include <string>

namespace hgl::graph
{
    class MaterialTemplate;
    class Geometry;
    class VertexInputLayout;
    using VIL = VertexInputLayout;
    class VILConfig;

    namespace mtl
    {
        struct MaterialAssetRecord;
    }

    void DumpMaterialVertexInput(FILE *out,
                                 const char *prefix,
                                 const char *label,
                                 const MaterialTemplate *material);

    void DumpVIL(FILE *out,
                 const char *prefix,
                 const char *label,
                 const VIL *vil);

    void DumpGeometryVertexFormats(FILE *out,
                                   const char *prefix,
                                   const char *label,
                                   const Geometry *geometry,
                                   bool include_vk_buffer = false);

    void DumpVILConfig(FILE *out,
                       const char *prefix,
                       const char *label,
                       const VILConfig &cfg);

    void DumpResolveVILIncompatibleDiagnostics(FILE *out,
                                               const char *prefix,
                                               MaterialTemplate *material,
                                               const Geometry *geometry,
                                               const mtl::MaterialAssetRecord &fallback_rec,
                                               const std::string &build_reason,
                                               const VILConfig &runtime_vil_cfg);

    void DumpPrimitiveBindingDiagnostics(FILE *out,
                                         const char *prefix,
                                         const Geometry *geometry,
                                         const VIL *vil,
                                         const std::string &material_name,
                                         const char *reason);
}