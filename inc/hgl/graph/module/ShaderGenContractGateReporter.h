#pragma once

namespace hgl::graph
{
    void ReportMirrorPreferredStrictAbort(const char *material_name,
                                          const char *category,
                                          const char *reason);

    void ReportMirrorSPVFallback(const char *material_name,
                                 const char *reason);

    void ReportMirrorVertexFallback(const char *material_name,
                                    const char *reason);

    void ReportMirrorDescriptorFallback(const char *material_name,
                                        const char *phase,
                                        const char *reason);
}
