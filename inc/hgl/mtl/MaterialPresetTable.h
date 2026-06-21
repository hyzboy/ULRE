#pragma once

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/RenderPhase.h>
#include <hgl/mtl/PassType.h>

#include <cstddef>
#include <vector>

namespace hgl::graph::mtl
{
    struct MaterialPresetCandidate
    {
        const char  *surface_path = nullptr;
        MaterialLOD  quality_level = MaterialLOD::Base;
        RenderPhase  render_phase = RenderPhase::Forward;
        PassType     pass_override = PassType::ForwardOpaque;
        bool         has_pass_override = false;
    };

    class MaterialPresetTable
    {
    public:
        void Clear() noexcept;

        bool AddCandidate(MaterialPreset preset, const MaterialPresetCandidate &candidate);

        std::vector<MaterialPresetCandidate> Query(MaterialPreset preset,
                                                   MaterialLOD requested_quality,
                                                   RenderPhase phase) const;

        bool Empty() const noexcept;
        size_t Size() const noexcept;

    private:
        struct Row
        {
            MaterialPreset preset;
            MaterialPresetCandidate candidate;
        };

        std::vector<Row> rows_;
    };
}
