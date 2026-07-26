#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/graph/geo/AttributeCompatibility.h>
#include <hgl/vk/VKFormat.h>
#include <hgl/vk/VKVertexInputFormat.h>
#include <initializer_list>
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

    namespace detail
    {
        inline uint8_t InferVecSizeFromFormat(const VkFormat format)
        {
            switch (format)
            {
            case VF_V2F:
            case VF_V2HF:
            case VF_V2I:
            case VF_V2I16:
            case VF_V2I8:
            case VF_V2U:
            case VF_V2U8:
            case VF_V2U16:
            case VF_V2UN8:
            case VF_V2UN16:
            case VF_V2SN8:
            case VF_V2SN16:
                return 2;
            case VF_V3F:
            case VF_V3HF:
            case VF_V3I:
            case VF_V3I16:
            case VF_V3I8:
            case VF_V3U:
            case VF_V3U8:
            case VF_V3U16:
            case VF_V3UN8:
            case VF_V3UN16:
            case VF_V3SN8:
            case VF_V3SN16:
                return 3;
            case VF_V4F:
            case VF_V4HF:
            case VF_V4I:
            case VF_V4I16:
            case VF_V4I8:
            case VF_V4U:
            case VF_V4U8:
            case VF_V4U16:
            case VF_V4UN8:
            case VF_V4UN16:
            case VF_V4SN8:
            case VF_V4SN16:
                return 4;
            case VF_V1F:
            case VF_V1HF:
            case VF_V1I:
            case VF_V1I16:
            case VF_V1I8:
            case VF_V1U:
            case VF_V1U8:
            case VF_V1U16:
            case VF_V1UN8:
            case VF_V1UN16:
            case VF_V1SN8:
            case VF_V1SN16:
                return 1;
            default:
                break;
            }

            return 0;
        }
    }

    class GeometryVertexFormat
    {
    private:

        std::vector<GeometryVertexAttributeFormat> attributes;

    public:

        GeometryVertexFormat() = default;

        GeometryVertexFormat(std::initializer_list<GeometryVertexAttributeFormat> init)
        {
            for (const auto &attribute : init)
                Add(attribute.semantic, attribute.format, attribute.vec_size, attribute.stride);
        }

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

            const uint8_t final_vec_size = vec_size ? vec_size : detail::InferVecSizeFromFormat(format);
            const uint32_t final_stride = stride ? stride : GetStrideByFormat(format);
            if (final_vec_size == 0 || final_stride == 0)
                return false;

            GeometryVertexAttributeFormat attribute;
            attribute.semantic = semantic;
            attribute.format = format;
            attribute.vec_size = final_vec_size;
            attribute.stride = final_stride;

            attributes.push_back(attribute);
            return true;
        }

        bool Add(const VertexSemantic semantic, const VkFormat format)
        {
            return Add(semantic, format, 0, 0);
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

    enum class GeometryVertexHandlingPath : uint8
    {
        DirectBind = 0,
        ExplicitHandling,
        Reject
    };

    inline const char *GetGeometryVertexHandlingPathName(const GeometryVertexHandlingPath path)
    {
        switch (path)
        {
        case GeometryVertexHandlingPath::DirectBind: return "DirectBind";
        case GeometryVertexHandlingPath::ExplicitHandling: return "ExplicitHandling";
        default: return "Reject";
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

    struct GeometryVertexFailureSummary
    {
        GeometryVertexMatchKind failure_kind = GeometryVertexMatchKind::Exact;
        GeometryVertexHandlingPath handling_path = GeometryVertexHandlingPath::DirectBind;
        bool requires_explicit_handling = false;
        bool has_mismatch = false;
        bool has_unsupported = false;
        bool has_compatible = false;
        bool has_only_registered_compatibility_differences = false;

        const GeometryVertexAttributeMatch *first_failure = nullptr;
        const GeometryVertexAttributeMatch *first_mismatch = nullptr;
        const GeometryVertexAttributeMatch *first_unsupported = nullptr;
        const GeometryVertexAttributeMatch *first_compatible = nullptr;
        const GeometryVertexAttributeMatch *first_registered_compatibility = nullptr;

        const char *GetFailureKindName() const
        {
            return GetGeometryVertexMatchKindName(failure_kind);
        }

        const char *GetHandlingPathName() const
        {
            return GetGeometryVertexHandlingPathName(handling_path);
        }
    };

    struct GeometryVertexFormatMatch
    {
        GeometryVertexMatchKind overall = GeometryVertexMatchKind::Exact;
        std::vector<GeometryVertexAttributeMatch> attributes;

        GeometryVertexMatchKind GetFailureKind() const
        {
            if (HasMismatch()) return GeometryVertexMatchKind::Mismatch;
            if (HasUnsupported()) return GeometryVertexMatchKind::Unsupported;
            if (HasCompatible()) return GeometryVertexMatchKind::Compatible;
            return GeometryVertexMatchKind::Exact;
        }

        const char *GetFailureKindName() const
        {
            return GetGeometryVertexMatchKindName(GetFailureKind());
        }

        const GeometryVertexAttributeMatch *FirstByKind(const GeometryVertexMatchKind expected_kind) const
        {
            for (const auto &attribute : attributes)
            {
                if (attribute.kind == expected_kind)
                    return &attribute;
            }

            return nullptr;
        }

        bool HasMismatch() const
        {
            return FirstByKind(GeometryVertexMatchKind::Mismatch) != nullptr;
        }

        bool HasUnsupported() const
        {
            return FirstByKind(GeometryVertexMatchKind::Unsupported) != nullptr;
        }

        bool HasCompatible() const
        {
            return FirstByKind(GeometryVertexMatchKind::Compatible) != nullptr;
        }

        const GeometryVertexAttributeMatch *FirstFailureAttribute() const
        {
            const GeometryVertexMatchKind failure_kind = GetFailureKind();
            return (failure_kind == GeometryVertexMatchKind::Exact) ? nullptr : FirstByKind(failure_kind);
        }

        bool HasOnlyRegisteredCompatibilityDifferences() const
        {
            return !HasMismatch() && (HasUnsupported() || HasCompatible());
        }

        GeometryVertexHandlingPath GetHandlingPath() const
        {
            if (IsDirectBindSatisfied())
                return GeometryVertexHandlingPath::DirectBind;

            if (HasMismatch())
                return GeometryVertexHandlingPath::Reject;

            return GeometryVertexHandlingPath::ExplicitHandling;
        }

        const char *GetHandlingPathName() const
        {
            return GetGeometryVertexHandlingPathName(GetHandlingPath());
        }

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

        GeometryVertexFailureSummary BuildFailureSummary() const
        {
            GeometryVertexFailureSummary summary;

            summary.failure_kind = GetFailureKind();
            summary.handling_path = GetHandlingPath();
            summary.requires_explicit_handling = RequiresExplicitHandling();
            summary.has_mismatch = HasMismatch();
            summary.has_unsupported = HasUnsupported();
            summary.has_compatible = HasCompatible();
            summary.has_only_registered_compatibility_differences = HasOnlyRegisteredCompatibilityDifferences();

            summary.first_failure = FirstFailureAttribute();
            summary.first_mismatch = FirstByKind(GeometryVertexMatchKind::Mismatch);
            summary.first_unsupported = FirstByKind(GeometryVertexMatchKind::Unsupported);
            summary.first_compatible = FirstByKind(GeometryVertexMatchKind::Compatible);
            summary.first_registered_compatibility = FirstRegisteredCompatibilityRule();

            return summary;
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
