#include <hgl/graph/module/ShaderGenDescriptorLayoutAdapter.h>
#include <hgl/vk/VKDescriptorSetType.h>
#include <cstdio>

using namespace hgl::graph;

static mtl::contract::DescriptorBindingDesc MakeBinding(uint32_t set,
                                                         uint32_t binding,
                                                         mtl::contract::ResourceClass rc,
                                                         uint32_t stage,
                                                         const char *name)
{
    mtl::contract::DescriptorBindingDesc d;
    d.set = set;
    d.binding = binding;
    d.resource_class = rc;
    d.stage_mask = stage;
    d.name = name;
    return d;
}

int main()
{
    int failed = 0;

    std::vector<ShaderDescriptor> legacy;
    {
        ShaderDescriptor ubo;
        std::snprintf(ubo.name, sizeof(ubo.name), "%s", "camera");
        ubo.desc_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo.set_type = DescriptorSetType::Camera;
        ubo.set = 1;
        ubo.binding = 0;
        ubo.stage_flag = uint32_t(VK_SHADER_STAGE_VERTEX_BIT);
        legacy.push_back(ubo);

        ShaderDescriptor tex;
        std::snprintf(tex.name, sizeof(tex.name), "%s", "BaseColorMap");
        tex.desc_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        tex.set_type = DescriptorSetType::PerMaterial;
        tex.set = 3;
        tex.binding = 2;
        tex.stage_flag = uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT);
        legacy.push_back(tex);
    }

    mtl::contract::ShaderGenResult result;
    result.layout.bindings.push_back(MakeBinding(1, 0, mtl::contract::ResourceClass::UniformBuffer, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "camera"));
    result.layout.bindings.push_back(MakeBinding(3, 2, mtl::contract::ResourceClass::SampledImage, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "BaseColorMap"));

    {
        std::string reason;
        if (!ValidateContractDescriptorLayoutAgainstLegacy(legacy, result, reason))
        {
            std::fprintf(stderr, "[FAIL] ValidateContractDescriptorLayoutAgainstLegacy failed: %s\n", reason.c_str());
            ++failed;
        }
    }

    {
        std::vector<ShaderDescriptor> out;
        std::string reason;
        if (!BuildShaderDescriptorsFromContractLayout(result, legacy, out, reason))
        {
            std::fprintf(stderr, "[FAIL] BuildShaderDescriptorsFromContractLayout failed: %s\n", reason.c_str());
            ++failed;
        }
        else
        {
            if (out.size() != 2)
            {
                std::fprintf(stderr, "[FAIL] out size mismatch (%zu != 2)\n", out.size());
                ++failed;
            }
            else
            {
                if (out[0].set_type != DescriptorSetType::Camera)
                {
                    std::fprintf(stderr, "[FAIL] keep legacy set_type for camera\n");
                    ++failed;
                }

                if (out[1].set_type != DescriptorSetType::PerMaterial)
                {
                    std::fprintf(stderr, "[FAIL] keep legacy set_type for texture\n");
                    ++failed;
                }
            }
        }
    }

    {
        mtl::contract::ShaderGenResult bad = result;
        bad.layout.bindings[1].resource_class = mtl::contract::ResourceClass::StorageBuffer;

        std::string reason;
        if (ValidateContractDescriptorLayoutAgainstLegacy(legacy, bad, reason))
        {
            std::fprintf(stderr, "[FAIL] type mismatch should fail validation\n");
            ++failed;
        }
    }

    {
        mtl::contract::ShaderGenResult fallback_result;
        fallback_result.layout.bindings.push_back(MakeBinding(1, 7, mtl::contract::ResourceClass::UniformBuffer, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "new_ubo"));

        std::vector<ShaderDescriptor> out;
        std::string reason;
        if (!BuildShaderDescriptorsFromContractLayout(fallback_result, legacy, out, reason))
        {
            std::fprintf(stderr, "[FAIL] build fallback set_type failed: %s\n", reason.c_str());
            ++failed;
        }
        else if (out.size() != 1 || out[0].set_type != DescriptorSetType::RenderTarget)
        {
            std::fprintf(stderr, "[FAIL] fallback set_type by set index mismatch\n");
            ++failed;
        }
    }

    if (failed != 0)
    {
        std::fprintf(stderr, "ShaderGenDescriptorLayoutAdapterTest FAILED (%d)\n", failed);
        return 1;
    }

    std::fprintf(stdout, "ShaderGenDescriptorLayoutAdapterTest PASSED\n");
    return 0;
}
