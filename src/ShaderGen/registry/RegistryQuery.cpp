#include <hgl/shadergen/RegistryQuery.h>

#include <cstdio>

namespace hgl::graph::mtl
{
    namespace
    {
        bool SupportsVertexBits(const VertexProgramTemplate &tpl, const MaterialVariantKey &key) noexcept
        {
            return tpl.supported_va_bits_mask == 0 || (key.vertex_attribute_feature_bits & ~tpl.supported_va_bits_mask) == 0;
        }

        uint32 BuildRequestedTextureMask(const MaterialVariantKey &key) noexcept
        {
            uint32 mask = 0;
            for (uint32 i = 0; i < uint32(SamplerSlot::RANGE_SIZE); ++i)
            {
                const auto slot = static_cast<SamplerSlot>(i);
                if (key.GetTextureSourceMode(slot) != TextureSourceMode::None)
                    mask |= (1u << i);
            }
            return mask;
        }

        bool MatchesSurface(const SurfaceFragmentTemplate &tpl, const MaterialVariantKey &key) noexcept
        {
            if (tpl.surface_type != key.surface_type)
                return false;

            if (tpl.lighting_model != key.lighting_model)
                return false;

            if (tpl.billboard_geometry_only)
            {
                if (key.geometry_mode != GeometryMode::BillboardCameraFacing
                 && key.geometry_mode != GeometryMode::BillboardAxisLocked)
                    return false;
            }

            const uint32 requested_mask = BuildRequestedTextureMask(key);
            if ((requested_mask & tpl.required_tex_slots_mask) != tpl.required_tex_slots_mask)
                return false;

            const uint32 declared_mask = tpl.required_tex_slots_mask | tpl.optional_tex_slots_mask;
            if ((requested_mask & ~declared_mask) != 0)
                return false;

            if ((key.sampler_feature_bits & tpl.required_sampler_feature_bits) != tpl.required_sampler_feature_bits)
                return false;

            return true;
        }
    }

    const VertexProgramTemplate *FindVertexProgramTemplate(const MaterialVariantKey &key,
                                                           std::string *miss_reason)
    {
        for (size_t i = 0; i < kVertexProgramTemplatesCount; ++i)
        {
            const auto &tpl = kVertexProgramTemplates[i];
            if (tpl.geometry_mode != key.geometry_mode)
                continue;
            if (tpl.position_provider != key.position_provider)
                continue;
            if (!SupportsVertexBits(tpl, key))
                continue;
            return &tpl;
        }

        if (miss_reason)
            *miss_reason = "no vertex template matched geometry_mode/position_provider/vertex_attrib_bits";
        return nullptr;
    }

    const SurfaceFragmentTemplate *FindSurfaceFragmentTemplate(const MaterialVariantKey &key,
                                                               std::string *miss_reason)
    {
        for (size_t i = 0; i < kSurfaceFragmentTemplatesCount; ++i)
        {
            const auto &tpl = kSurfaceFragmentTemplates[i];
            if (MatchesSurface(tpl, key))
                return &tpl;
        }

        if (miss_reason)
            *miss_reason = "no surface fragment template matched surface_type/lighting/texture slots";
        return nullptr;
    }

    const PipelineStateRow *FindPipelineStateRow(const MaterialVariantKey &key,
                                                 std::string *miss_reason)
    {
        for (size_t i = 0; i < kPipelineStateRowsCount; ++i)
        {
            const auto &row = kPipelineStateRows[i];
            if (row.blend != key.blend_mode)
                continue;
            if (row.pass != key.pass_hint)
                continue;
            return &row;
        }

        if (miss_reason)
            *miss_reason = "no pipeline state row matched blend/pass";
        return nullptr;
    }

    RegistryQueryResult QueryPhase3Registry(const MaterialVariantKey &key)
    {
        RegistryQueryResult result;

        result.vertex = FindVertexProgramTemplate(key, &result.miss_reason);
        if (!result.vertex)
            return result;

        result.fragment = FindSurfaceFragmentTemplate(key, &result.miss_reason);
        if (!result.fragment)
            return result;

        result.pipeline = FindPipelineStateRow(key, &result.miss_reason);
        if (!result.pipeline)
            return result;

        result.miss_reason.clear();
        return result;
    }
}
