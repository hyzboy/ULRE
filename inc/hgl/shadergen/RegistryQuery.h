#pragma once

#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/MaterialVariantRow.h>
#include <hgl/mtl/PipelineStateRow.h>
#include <hgl/mtl/SurfaceFragmentTemplate.h>
#include <hgl/mtl/VertexProgramTemplate.h>
#include <string>

namespace hgl::graph::mtl
{
    struct RegistryQueryResult
    {
        const VertexProgramTemplate *vertex = nullptr;
        const SurfaceFragmentTemplate *fragment = nullptr;
        const PipelineStateRow *pipeline = nullptr;
        std::string miss_reason;
    };

    const VertexProgramTemplate *FindVertexProgramTemplate(const MaterialVariantKey &key,
                                                           std::string *miss_reason = nullptr);

    const SurfaceFragmentTemplate *FindSurfaceFragmentTemplate(const MaterialVariantKey &key,
                                                               std::string *miss_reason = nullptr);

    const PipelineStateRow *FindPipelineStateRow(const MaterialVariantKey &key,
                                                 std::string *miss_reason = nullptr);

    RegistryQueryResult QueryPhase3Registry(const MaterialVariantKey &key);

    MaterialVariantRow ComposeLegacyRow(const VertexProgramTemplate *vertex,
                                        const SurfaceFragmentTemplate *fragment,
                                        const PipelineStateRow *pipeline,
                                        const MaterialVariantKey &key,
                                        const char *debug_name = "Phase3ComposedRow");
}
