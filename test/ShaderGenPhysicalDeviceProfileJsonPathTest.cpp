#include <ShaderGen/GLSLCompiler.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstdio>

using namespace hgl::graph;

namespace
{
    constexpr uint32_t SPV_VERSION_1_5 = (1u << 16) | (5u << 8);
    constexpr uint32_t SPV_VERSION_1_6 = (1u << 16) | (6u << 8);
}

int main()
{
    int failed = 0;

    const char *json_v12 = R"JSON(
{
  "status":"PASS",
  "device_count":1,
  "devices":[
    {
      "name":"P0-Test-V12",
      "device_type":"discrete",
      "capability_tier":"high",
      "vendor_id":4318,
      "device_id":1234,
      "api_version":4202496,
      "limits":{
        "maxImageDimension2D":8192,
        "maxPushConstantsSize":256,
        "maxVertexInputAttributes":16,
        "maxBoundDescriptorSets":8,
        "maxUniformBufferRange":65536,
        "maxStorageBufferRange":134217728
      },
      "features":{
        "geometryShader":true,
        "tessellationShader":true,
        "descriptorIndexing":true
      },
      "queue_families":[{"queueFlags":"GRAPHICS|COMPUTE|TRANSFER"}]
    }
  ]
}
)JSON";

    if (!SetShaderCompilerPhysicalDeviceProfileFromJson(json_v12))
    {
        std::fprintf(stderr, "[FAIL] JSON v1.2 profile parse/apply failed\n");
        ++failed;
    }
    else
    {
        uint32_t vk_version = 0;
        uint32_t spv_version = 0;
        GetShaderCompilerTargetVersions(vk_version, spv_version);

        if (vk_version != VK_API_VERSION_1_2 || spv_version != SPV_VERSION_1_5)
        {
            std::fprintf(stderr, "[FAIL] v1.2 target mismatch, vk=%u.%u spv=%u.%u\n",
                         VK_VERSION_MAJOR(vk_version),
                         VK_VERSION_MINOR(vk_version),
                         (spv_version >> 16) & 0xff,
                         (spv_version >> 8) & 0xff);
            ++failed;
        }
    }

    const char *json_v13 = R"JSON(
{
  "status":"PASS",
  "device_count":1,
  "devices":[
    {
      "name":"P0-Test-V13",
      "device_type":"discrete",
      "capability_tier":"high",
      "vendor_id":4318,
      "device_id":5678,
      "api_version":4206592,
      "limits":{
        "maxImageDimension2D":8192,
        "maxPushConstantsSize":256,
        "maxVertexInputAttributes":16,
        "maxBoundDescriptorSets":8,
        "maxUniformBufferRange":65536,
        "maxStorageBufferRange":134217728
      },
      "features":{
        "geometryShader":true,
        "tessellationShader":true,
        "descriptorIndexing":true
      },
      "queue_families":[{"queueFlags":"GRAPHICS|COMPUTE|TRANSFER"}]
    }
  ]
}
)JSON";

    if (!SetShaderCompilerPhysicalDeviceProfileFromJson(json_v13))
    {
        std::fprintf(stderr, "[FAIL] JSON v1.3 profile parse/apply failed\n");
        ++failed;
    }
    else
    {
        uint32_t vk_version = 0;
        uint32_t spv_version = 0;
        GetShaderCompilerTargetVersions(vk_version, spv_version);

        if (vk_version != VK_API_VERSION_1_3 || spv_version != SPV_VERSION_1_6)
        {
            std::fprintf(stderr, "[FAIL] v1.3 target mismatch, vk=%u.%u spv=%u.%u\n",
                         VK_VERSION_MAJOR(vk_version),
                         VK_VERSION_MINOR(vk_version),
                         (spv_version >> 16) & 0xff,
                         (spv_version >> 8) & 0xff);
            ++failed;
        }
    }

    if (SetShaderCompilerPhysicalDeviceProfileFromJson("{\"device_count\":0}"))
    {
        std::fprintf(stderr, "[FAIL] invalid JSON should be rejected\n");
        ++failed;
    }

    if (failed != 0)
    {
        std::fprintf(stderr, "ShaderGenPhysicalDeviceProfileJsonPathTest FAILED (%d)\n", failed);
        return 1;
    }

    std::fprintf(stdout, "ShaderGenPhysicalDeviceProfileJsonPathTest PASSED\n");
    return 0;
}
