#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/vk/VKVertexInputFormat.h>

namespace hgl::graph
{
    enum class AttributePrecisionGrade : uint8
    {
        Unknown = 0,
        High,
        Medium,
        Low
    };

    inline const char *GetAttributePrecisionGradeName(const AttributePrecisionGrade grade)
    {
        switch (grade)
        {
        case AttributePrecisionGrade::High: return "High";
        case AttributePrecisionGrade::Medium: return "Medium";
        case AttributePrecisionGrade::Low: return "Low";
        default: return "Unknown";
        }
    }

    enum class AttributeRuntimeCost : uint8
    {
        None = 0,
        Low,
        Medium,
        High
    };

    inline const char *GetAttributeRuntimeCostName(const AttributeRuntimeCost cost)
    {
        switch (cost)
        {
        case AttributeRuntimeCost::None: return "None";
        case AttributeRuntimeCost::Low: return "Low";
        case AttributeRuntimeCost::Medium: return "Medium";
        default: return "High";
        }
    }

    struct AttributeCompatibilityRule
    {
        VertexSemantic semantic = VertexSemantic::Unknown;
        VkFormat source_format = VK_FORMAT_UNDEFINED;
        uint8 source_vec_size = 0;
        // 0 means "don't care": the rule matches any source stride as long as the semantic/format/vector width matches.
        uint32 source_stride = 0;
        VkFormat target_format = VK_FORMAT_UNDEFINED;
        uint8 target_vec_size = 0;
        // 0 means "don't care" for the required material-side stride as well.
        uint32 target_stride = 0;
        // Whether the conversion preserves source information exactly.
        bool lossless = false;
        // Expected quality after conversion, even when auto-apply is disabled for now.
        AttributePrecisionGrade precision = AttributePrecisionGrade::Unknown;
        // Relative cost for a future runtime or offline conversion path.
        AttributeRuntimeCost runtime_cost = AttributeRuntimeCost::High;
        // R09 only registers capability/diagnostics first; execution stays disabled unless this is explicitly true.
        bool allow_auto_apply = false;
    };

    inline bool MatchAttributeCompatibilityRule(
        const AttributeCompatibilityRule &rule,
        const VertexSemantic semantic,
        const VkFormat source_format,
        const uint8 source_vec_size,
        const uint32 source_stride,
        const VkFormat target_format,
        const uint8 target_vec_size,
        const uint32 target_stride)
    {
        if (rule.semantic != semantic)
            return false;

        if (rule.source_format != source_format || rule.target_format != target_format)
            return false;

        if (rule.source_vec_size != source_vec_size || rule.target_vec_size != target_vec_size)
            return false;

        if (rule.source_stride != 0 && source_stride != rule.source_stride)
            return false;

        if (rule.target_stride != 0 && target_stride != rule.target_stride)
            return false;

        return true;
    }

    // Register a custom compatibility rule into the global registry.
    // Default rules (normal-direction candidates) are pre-registered at startup.
    // Registered rules are diagnostics-only: none are auto-applied unless allow_auto_apply is set.
    void RegisterAttributeCompatibilityRule(const AttributeCompatibilityRule &rule);

    // Look up the first registered rule that matches the given source->target pair.
    // Returns nullptr when no rule covers this combination (i.e., the mismatch is hard).
    const AttributeCompatibilityRule *FindAttributeCompatibilityRule(
        const VertexSemantic semantic,
        const VkFormat source_format,
        const uint8 source_vec_size,
        const uint32 source_stride,
        const VkFormat target_format,
        const uint8 target_vec_size,
        const uint32 target_stride);
}
