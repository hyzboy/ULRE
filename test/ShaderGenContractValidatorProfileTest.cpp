#include <hgl/shadergen/contract/ShaderGenContractValidator.h>

#include <cstdio>
#include <string>

using namespace hgl::graph::mtl::contract;

static bool ContainsMessage(const ShaderGenContractValidationResult &result, const char *keyword)
{
    if (!keyword || !keyword[0])
        return false;

    for (const auto &msg : result.errors)
    {
        if (msg.find(keyword) != std::string::npos)
            return true;
    }

    return false;
}

int main()
{
    int failed = 0;

    ShaderGenRequest request;
    request.contract_version = kShaderGenContractVersion;
    request.has_physical_device_profile = true;
    request.physical_device_profile.name = "P1-Test-Profile";
    request.physical_device_profile.limits.max_vertex_input_attributes = 1;
    request.physical_device_profile.limits.max_bound_descriptor_sets = 1;
    request.physical_device_profile.limits.max_uniform_buffer_range = 64;
    request.physical_device_profile.limits.max_storage_buffer_range = 128;
    request.physical_device_profile.features.geometry_shader = false;

    ShaderGenResult result;
    result.contract_version = kShaderGenContractVersion;

    result.vertex_layout.attributes.push_back({0, "POSITION", "vec3", 0});
    result.vertex_layout.attributes.push_back({1, "NORMAL", "vec3", 0});

    result.layout.bindings.push_back({0, 0, ResourceClass::UniformBuffer, uint32_t(ShaderStageMask::Vertex), "ubo0", "UBO0"});
    result.layout.bindings.push_back({1, 0, ResourceClass::UniformBuffer, uint32_t(ShaderStageMask::Vertex), "ubo1", "UBO1"});

    BufferStructDesc ubo;
    ubo.struct_name = "PerMaterial";
    ubo.resource_class = ResourceClass::UniformBuffer;
    ubo.byte_size = 256;
    result.buffer_structs.push_back(ubo);

    BufferStructDesc ssbo;
    ssbo.struct_name = "StorageData";
    ssbo.resource_class = ResourceClass::StorageBuffer;
    ssbo.byte_size = 512;
    result.buffer_structs.push_back(ssbo);

    StageSpvBlob gs_blob;
    gs_blob.stage_mask = uint32_t(ShaderStageMask::Geometry);
    gs_blob.words.push_back(0x07230203u);
    result.spv_per_stage.push_back(gs_blob);

    const auto with_profile = ValidateShaderGenRequestResult(request, result, "ProfileLimitedMat");

    if (with_profile.valid)
    {
        std::fprintf(stderr, "[FAIL] expected profile-limited validation to fail\n");
        ++failed;
    }

    if (!ContainsMessage(with_profile, "vertex attributes"))
    {
        std::fprintf(stderr, "[FAIL] missing vertex attribute profile-limit error\n");
        ++failed;
    }

    if (!ContainsMessage(with_profile, "descriptor set count"))
    {
        std::fprintf(stderr, "[FAIL] missing descriptor-set profile-limit error\n");
        ++failed;
    }

    if (!ContainsMessage(with_profile, "UBO byte_size"))
    {
        std::fprintf(stderr, "[FAIL] missing UBO profile-limit error\n");
        ++failed;
    }

    if (!ContainsMessage(with_profile, "SSBO byte_size"))
    {
        std::fprintf(stderr, "[FAIL] missing SSBO profile-limit error\n");
        ++failed;
    }

    if (!ContainsMessage(with_profile, "geometry shader stage present"))
    {
        std::fprintf(stderr, "[FAIL] missing geometry feature mismatch error\n");
        ++failed;
    }

    ShaderGenRequest no_profile_request = request;
    no_profile_request.has_physical_device_profile = false;

    const auto without_profile = ValidateShaderGenRequestResult(no_profile_request, result, "ProfileLimitedMat");
    if (!without_profile.valid)
    {
        std::fprintf(stderr, "[FAIL] validation should pass when profile gating is disabled\n");
        ++failed;
    }

    if (failed != 0)
    {
        std::fprintf(stderr, "ShaderGenContractValidatorProfileTest FAILED (%d)\n", failed);
        return 1;
    }

    std::fprintf(stdout, "ShaderGenContractValidatorProfileTest PASSED\n");
    return 0;
}
