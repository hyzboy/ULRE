#include <hgl/graph/geo/AttributeCompatibility.h>
#include <vector>

namespace hgl::graph
{
namespace
{
    // Thread-safety note: rules are expected to be registered once at startup.
    // No locking is applied; do not register rules concurrently.
    std::vector<AttributeCompatibilityRule> &GetRegistry()
    {
        static std::vector<AttributeCompatibilityRule> registry = []
        {
            std::vector<AttributeCompatibilityRule> rules;

            // normal direction: source and target are both full-precision float —
            // stride/format is already equal so this acts as an exact-match alias.
            rules.push_back({VertexSemantic::Normal,
                             VK_FORMAT_R32G32B32_SFLOAT, 3, 0,
                             VK_FORMAT_R32G32B32_SFLOAT, 3, 0,
                             true, AttributePrecisionGrade::High, AttributeRuntimeCost::Low, false});

            // normal direction: half-float source -> float target (medium precision loss).
            rules.push_back({VertexSemantic::Normal,
                             VK_FORMAT_R16G16B16_SFLOAT, 3, 0,
                             VK_FORMAT_R32G32B32_SFLOAT, 3, 0,
                             false, AttributePrecisionGrade::Medium, AttributeRuntimeCost::Medium, false});

            // normal direction: 8-bit unorm source -> float target (low precision, diagnostics only).
            rules.push_back({VertexSemantic::Normal,
                             VK_FORMAT_R8G8B8_UNORM, 3, 0,
                             VK_FORMAT_R32G32B32_SFLOAT, 3, 0,
                             false, AttributePrecisionGrade::Low, AttributeRuntimeCost::Medium, false});

            // RG8 -> normal: structure is registered so diagnostics can identify it,
            // but allow_auto_apply stays false — do NOT enable without explicit sign-off.
            rules.push_back({VertexSemantic::Normal,
                             VK_FORMAT_R8G8_UNORM, 2, 0,
                             VK_FORMAT_R32G32B32_SFLOAT, 3, 0,
                             false, AttributePrecisionGrade::Low, AttributeRuntimeCost::High, false});

            return rules;
        }();

        return registry;
    }
}

void RegisterAttributeCompatibilityRule(const AttributeCompatibilityRule &rule)
{
    GetRegistry().push_back(rule);
}

const AttributeCompatibilityRule *FindAttributeCompatibilityRule(
    const VertexSemantic semantic,
    const VkFormat source_format,
    const uint8 source_vec_size,
    const uint32 source_stride,
    const VkFormat target_format,
    const uint8 target_vec_size,
    const uint32 target_stride)
{
    for (const AttributeCompatibilityRule &rule : GetRegistry())
    {
        if (MatchAttributeCompatibilityRule(rule, semantic,
                                            source_format, source_vec_size, source_stride,
                                            target_format, target_vec_size, target_stride))
            return &rule;
    }

    return nullptr;
}

}//namespace hgl::graph
