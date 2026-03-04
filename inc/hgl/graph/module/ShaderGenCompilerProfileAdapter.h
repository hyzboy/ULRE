#pragma once

namespace hgl::graph
{
    namespace mtl::contract
    {
        struct PhysicalDeviceProfileLite;
        struct ShaderGenRequest;
    }

    void ApplyShaderCompilerProfile(const mtl::contract::PhysicalDeviceProfileLite *profile);
    void ApplyShaderCompilerProfileFromRequest(const mtl::contract::ShaderGenRequest *request);
}
