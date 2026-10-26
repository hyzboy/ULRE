#include <hgl/mtl/MaterialStageInterface.h>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    namespace
    {
        bool SetFailure(
            MaterialStageInterfaceDiagnostic &diagnostic,
            const MaterialStageInterfaceError error,
            const InterStageSemantic semantic =
                InterStageSemantic::Unknown) noexcept
        {
            diagnostic.error = error;
            diagnostic.semantic = semantic;
            return false;
        }

        // ── inter-stage 条目校验（本地化）───────────────────────────────
        // 原经 ShaderInterfaceContract 聚合体校验的往返已删，规则逐条保留：
        // 逐条（语义/location/插值/形状）+ 两两（语义不重复、location 区间不冲突）。
        bool IsValidInterStageShape(
            const ShaderSemanticScalarType scalar_type,
            const uint8 component_count,
            const uint8 location_width) noexcept
        {
            return scalar_type != ShaderSemanticScalarType::Unknown
                && component_count > 0
                && component_count <= 4
                && location_width > 0;
        }

        bool HasInterStageLocationConflict(
            const uint32 first_location,
            const uint32 first_width,
            const uint32 second_location,
            const uint32 second_width) noexcept
        {
            return first_location < second_location + second_width
                && second_location < first_location + first_width;
        }

        bool ValidateInterStageEntries(
            const ValueArray<InterStageSemanticContractEntry> &entries) noexcept
        {
            for (int i = 0; i < entries.GetCount(); ++i)
            {
                const InterStageSemanticContractEntry &entry = entries[i];
                if (entry.semantic == InterStageSemantic::Unknown
                 || entry.location == InvalidShaderSemanticLocation
                 || entry.interpolation < InterStageInterpolation::Smooth
                 || entry.interpolation > InterStageInterpolation::NoPerspective
                 || !IsValidInterStageShape(
                        entry.scalar_type,
                        entry.component_count,
                        entry.location_width))
                    return false;

                for (int j = 0; j < i; ++j)
                {
                    const InterStageSemanticContractEntry &other = entries[j];
                    if (entry.semantic == other.semantic
                     || HasInterStageLocationConflict(
                            entry.location,
                            entry.location_width,
                            other.location,
                            other.location_width))
                        return false;
                }
            }
            return true;
        }
    }

    const char *GetMaterialStageInterfaceErrorName(
        const MaterialStageInterfaceError error) noexcept
    {
#define HGL_ERROR(name) case MaterialStageInterfaceError::name: return #name;
        switch (error)
        {
            HGL_MATERIAL_STAGE_INTERFACE_ERROR_LIST
        }
#undef HGL_ERROR
        return "Unknown";
    }

    InterStageSemanticMask GetMaterialInterStageSemanticMask(
        const MaterialVertexVaryingConfig &varying) noexcept
    {
        // 唯一实现见 MaterialStageInterface.h 的定义表（正反两向同源）。
        return BuildMaterialInterStageSemanticMaskFromVarying(varying);
    }

    bool BuildMaterialStageInterface(
        const MaterialVertexVaryingConfig &varying,
        ValueArray<InterStageSemanticContractEntry> &out_entries,
        MaterialStageInterfaceDiagnostic &out_diagnostic) noexcept
    {
        out_entries.Clear();
        out_diagnostic = {};

        const InterStageSemanticMask mask =
            GetMaterialInterStageSemanticMask(varying);
        for (uint32 value = 1;
             value < static_cast<uint32>(InterStageSemantic::RANGE_SIZE);
             ++value)
        {
            const InterStageSemantic semantic =
                static_cast<InterStageSemantic>(value);
            if (!(mask & GetInterStageSemanticMask(semantic)))
                continue;

            const InterStageSemanticInfo *info =
                GetInterStageSemanticInfo(semantic);
            if (!info
             || info->stable_location == InvalidShaderSemanticLocation
             || info->location_width == 0)
            {
                return SetFailure(
                    out_diagnostic,
                    MaterialStageInterfaceError::MissingSemanticMetadata,
                    semantic);
            }

            out_entries.Add(
                {
                    semantic,
                    info->value_shape.scalar_type,
                    info->interpolation,
                    info->value_shape.component_count,
                    info->location_width,
                    info->stable_location
                });
        }

        if (!ValidateInterStageEntries(out_entries))
            return SetFailure(
                out_diagnostic,
                MaterialStageInterfaceError::InvalidContract);
        return true;
    }

    const InterStageSemanticContractEntry *FindMaterialStageInterfaceEntry(
        const ValueArray<InterStageSemanticContractEntry> &entries,
        const InterStageSemantic semantic) noexcept
    {
        for (int i = 0; i < entries.GetCount(); ++i)
        {
            if (entries[i].semantic == semantic)
                return &entries[i];
        }
        return nullptr;
    }

}
