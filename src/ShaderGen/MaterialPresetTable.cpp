#include <hgl/mtl/MaterialPresetTable.h>

namespace hgl::graph::mtl
{
    void MaterialPresetTable::Clear() noexcept
    {
        rows_.clear();
    }

    bool MaterialPresetTable::AddCandidate(MaterialPreset preset, const MaterialPresetCandidate &candidate)
    {
        if (!candidate.surface_path || !candidate.surface_path[0])
            return false;

        rows_.push_back(Row{preset, candidate});
        return true;
    }

    std::vector<MaterialPresetCandidate> MaterialPresetTable::Query(MaterialPreset preset,
                                                                    MaterialLOD requested_quality,
                                                                    RenderPhase phase) const
    {
        std::vector<MaterialPresetCandidate> out;

        const int requested = static_cast<int>(requested_quality);
        for (int q = requested; q >= 0; --q)
        {
            const auto quality = static_cast<MaterialLOD>(q);

            for (const Row &row : rows_)
            {
                if (row.preset != preset)
                    continue;

                if (row.candidate.render_phase != phase)
                    continue;

                if (row.candidate.quality_level != quality)
                    continue;

                out.push_back(row.candidate);
            }

            if (!out.empty())
                break;
        }

        return out;
    }

    bool MaterialPresetTable::Empty() const noexcept
    {
        return rows_.empty();
    }

    size_t MaterialPresetTable::Size() const noexcept
    {
        return rows_.size();
    }
}
