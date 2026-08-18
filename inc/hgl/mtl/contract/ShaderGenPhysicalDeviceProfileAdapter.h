#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/contract/ShaderGenContract.h>

namespace hgl::graph
{
    class VulkanPhyDevice;
}

namespace hgl::graph::mtl::contract
{
    using namespace hgl::graph::mtl;
    const char *ResolveCapabilityTier(const ::hgl::graph::VulkanPhyDevice &pd);
    const char *ResolveDeviceTypeName(uint32_t physical_device_type);

    PhysicalDeviceProfileLite BuildPhysicalDeviceProfileFromVulkanPhyDevice(const ::hgl::graph::VulkanPhyDevice &pd);
}
