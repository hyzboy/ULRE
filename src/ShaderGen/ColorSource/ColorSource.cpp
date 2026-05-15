#include <hgl/shadergen/ColorSource.h>
#include <hgl/mtl/SamplerSlot.h>

namespace hgl::graph
{

ColorSource ColorSource::MakeSampler2D(mtl::SamplerSlot slot,
                                       ColorSourceOutputFormat fmt,
                                       const std::string &debug_name)
{
    ColorSource cs;
    cs.slot      = slot;
    cs.kind      = ColorSourceKind::BuiltinSampler2D;
    cs.signature = ColorSourceSignature::UV2D;
    cs.builtin.output_format = fmt;
    cs.bindings.push_back(DescriptorRequirement{
        .type           = DescriptorType::CombinedImageSampler,
        .count          = 1,
        .stages         = ShaderStage::Fragment,
        .binding_policy = BindingPolicy::Auto,
        .debug_name     = debug_name.empty()
                            ? std::string("Sampler_") + mtl::SamplerSlotNameList[size_t(slot)]
                            : debug_name,
    });
    return cs;
}

ColorSource ColorSource::MakeSampler2DArray(mtl::SamplerSlot slot,
                                            ColorSourceOutputFormat fmt,
                                            const std::string &debug_name)
{
    ColorSource cs;
    cs.slot      = slot;
    cs.kind      = ColorSourceKind::BuiltinSampler2DArray;
    cs.signature = ColorSourceSignature::UV2DPerInstance;
    cs.builtin.output_format = fmt;
    cs.bindings.push_back(DescriptorRequirement{
        .type           = DescriptorType::CombinedImageSampler,
        .count          = 1,
        .stages         = ShaderStage::Fragment,
        .binding_policy = BindingPolicy::Auto,
        .debug_name     = debug_name.empty()
                            ? std::string("Sampler_") + mtl::SamplerSlotNameList[size_t(slot)]
                            : debug_name,
    });
    return cs;
}

} // namespace hgl::graph
