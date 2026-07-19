#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/graph/geo/AttributeCompatibility.h>
#include <hgl/vk/VKVertexInputFormat.h>
#include <vector>
#include <cstring>

namespace hgl::graph
{
    struct GeometryVertexAttributeFormat
    {
        VertexSemantic semantic = VertexSemantic::Unknown;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint8_t vec_size = 0;
        uint32_t stride = 0;
    };

    class GeometryVertexFormat
    {
    private:

        std::vector<GeometryVertexAttributeFormat> attributes;

    public:

        void Clear()
        {
            attributes.clear();
        }

        uint32_t GetCount() const
        {
            return static_cast<uint32_t>(attributes.size());
        }

        const GeometryVertexAttributeFormat *Get(const uint32_t index) const
        {
            return index < attributes.size() ? &attributes[index] : nullptr;
        }

        const GeometryVertexAttributeFormat *Find(const VertexSemantic semantic) const
        {
            if (semantic == VertexSemantic::Unknown)
                return nullptr;

            for (const auto &attribute : attributes)
            {
                if (attribute.semantic == semantic)
                    return &attribute;
            }

            return nullptr;
        }

        bool Add(const VertexSemantic semantic, const VkFormat format, const uint8_t vec_size, const uint32_t stride)
        {
            if (semantic == VertexSemantic::Unknown || Find(semantic))
                return false;

            GeometryVertexAttributeFormat attribute;
            attribute.semantic = semantic;
            attribute.format = format;
            attribute.vec_size = vec_size;
            attribute.stride = stride;

            attributes.push_back(attribute);
            return true;
        }

    };

    enum class GeometryVertexMatchKind : uint8
    {
        Exact = 0,
        Compatible,
        Unsupported,
        Mismatch
    };

    inline const char *GetGeometryVertexMatchKindName(const GeometryVertexMatchKind kind)
    {
        switch (kind)
        {
        case GeometryVertexMatchKind::Exact: return "Exact";
        case GeometryVertexMatchKind::Compatible: return "Compatible";
        case GeometryVertexMatchKind::Unsupported: return "Unsupported";
        default: return "Mismatch";
        }
    }

    struct GeometryVertexCompatibilityDecision
    {
        GeometryVertexMatchKind kind = GeometryVertexMatchKind::Mismatch;
        const AttributeCompatibilityRule *compatibility_rule = nullptr;

        bool IsDirectBindSatisfied() const
        {
            return kind == GeometryVertexMatchKind::Exact;
        }

        bool HasRegisteredCompatibilityRule() const
        {
            return compatibility_rule != nullptr;
        }

        bool RequiresExplicitHandling() const
        {
            return kind == GeometryVertexMatchKind::Compatible
                || kind == GeometryVertexMatchKind::Unsupported;
        }
    };

    struct GeometryVertexAttributeMatch
    {
        GeometryVertexMatchKind kind = GeometryVertexMatchKind::Mismatch;
        VertexSemantic semantic = VertexSemantic::Unknown;
        VkFormat geometry_format = VK_FORMAT_UNDEFINED;
        VkFormat material_format = VK_FORMAT_UNDEFINED;
        uint32_t geometry_stride = 0;
        uint32_t material_stride = 0;
        uint8_t geometry_vec_size = 0;
        uint8_t material_vec_size = 0;
        // True means "there is a registered compatibility rule for this source->target pair".
        bool has_compatibility_rule = false;
        // Reserved for a future explicitly-enabled path; current R09 runtime does not auto-apply anything.
        bool compatibility_allow_auto_apply = false;
        bool compatibility_lossless = false;
        AttributePrecisionGrade compatibility_precision = AttributePrecisionGrade::Unknown;
        AttributeRuntimeCost compatibility_runtime_cost = AttributeRuntimeCost::High;
    };

    struct GeometryVertexFormatMatch
    {
        GeometryVertexMatchKind overall = GeometryVertexMatchKind::Exact;
        std::vector<GeometryVertexAttributeMatch> attributes;

        bool IsDirectBindSatisfied() const
        {
            return overall == GeometryVertexMatchKind::Exact;
        }

        bool RequiresExplicitHandling() const
        {
            return overall == GeometryVertexMatchKind::Compatible
                || overall == GeometryVertexMatchKind::Unsupported;
        }

        const GeometryVertexAttributeMatch *FirstRegisteredCompatibilityRule() const
        {
            for (const auto &attribute : attributes)
            {
                if (attribute.has_compatibility_rule)
                    return &attribute;
            }

            return nullptr;
        }

        const GeometryVertexAttributeMatch *FirstNonExact() const
        {
            for (const auto &attribute : attributes)
            {
                if (attribute.kind != GeometryVertexMatchKind::Exact)
                    return &attribute;
            }

            return nullptr;
        }
    };

    inline GeometryVertexCompatibilityDecision EvaluateGeometryVertexAttributeCompatibility(
        const GeometryVertexAttributeFormat &geometry_attribute,
        const VertexInputFormat &material_attribute)
    {
        GeometryVertexCompatibilityDecision decision;

        if (geometry_attribute.format == material_attribute.format
         && geometry_attribute.stride == material_attribute.stride
         && geometry_attribute.vec_size == material_attribute.vec_size)
        {
            decision.kind = GeometryVertexMatchKind::Exact;
            return decision;
        }

        // Any non-exact match must be backed by an explicit registered compatibility rule.
        decision.compatibility_rule = FindAttributeCompatibilityRule(
            geometry_attribute.semantic,
            geometry_attribute.format,
            geometry_attribute.vec_size,
            geometry_attribute.stride,
            material_attribute.format,
            uint8(material_attribute.vec_size),
            uint32(material_attribute.stride));

        if (decision.compatibility_rule)
        {
            decision.kind = decision.compatibility_rule->allow_auto_apply
                ? GeometryVertexMatchKind::Compatible
                : GeometryVertexMatchKind::Unsupported;
            return decision;
        }

        decision.kind = GeometryVertexMatchKind::Mismatch;
        return decision;
    }

    inline GeometryVertexMatchKind MatchGeometryVertexAttribute(
        const GeometryVertexAttributeFormat &geometry_attribute,
        const VertexInputFormat &material_attribute,
        const AttributeCompatibilityRule **out_compatibility_rule = nullptr)
    {
        if (out_compatibility_rule)
            *out_compatibility_rule = nullptr;

        const GeometryVertexCompatibilityDecision decision =
            EvaluateGeometryVertexAttributeCompatibility(geometry_attribute, material_attribute);

        if (out_compatibility_rule)
            *out_compatibility_rule = decision.compatibility_rule;

        return decision.kind;
    }

    inline GeometryVertexFormatMatch MatchGeometryVertexFormat(
        const GeometryVertexFormat &geometry_format,
        const VertexInputFormat *material_vif_list,
        const uint32_t material_vif_count)
    {
        GeometryVertexFormatMatch result;

        if(!material_vif_list||material_vif_count==0)
            return result;

        for (uint32_t i = 0; i < material_vif_count; ++i)
        {
            const VertexInputFormat *material_attribute=material_vif_list+i;

            GeometryVertexAttributeMatch match;
            match.semantic = material_attribute->semantic;
            match.material_format = material_attribute->format;
            match.material_stride = uint32_t(material_attribute->stride);
            match.material_vec_size = uint8_t(material_attribute->vec_size);

            const GeometryVertexAttributeFormat *geometry_attribute = geometry_format.Find(match.semantic);
            if (!geometry_attribute)
            {
                match.kind = GeometryVertexMatchKind::Mismatch;
            }
            else
            {
                match.geometry_format = geometry_attribute->format;
                match.geometry_stride = geometry_attribute->stride;
                match.geometry_vec_size = geometry_attribute->vec_size;

                const GeometryVertexCompatibilityDecision decision =
                    EvaluateGeometryVertexAttributeCompatibility(*geometry_attribute, *material_attribute);

                match.kind = decision.kind;
                if (decision.compatibility_rule)
                {
                    match.has_compatibility_rule = true;
                    match.compatibility_allow_auto_apply = decision.compatibility_rule->allow_auto_apply;
                    match.compatibility_lossless = decision.compatibility_rule->lossless;
                    match.compatibility_precision = decision.compatibility_rule->precision;
                    match.compatibility_runtime_cost = decision.compatibility_rule->runtime_cost;
                }
            }

            if (match.kind == GeometryVertexMatchKind::Mismatch)
                result.overall = GeometryVertexMatchKind::Mismatch;
            else if (match.kind == GeometryVertexMatchKind::Unsupported
                  && result.overall != GeometryVertexMatchKind::Mismatch)
                result.overall = GeometryVertexMatchKind::Unsupported;
            else if (match.kind == GeometryVertexMatchKind::Compatible
                  && result.overall == GeometryVertexMatchKind::Exact)
                result.overall = GeometryVertexMatchKind::Compatible;

            result.attributes.push_back(match);
        }

        return result;
    }
}
