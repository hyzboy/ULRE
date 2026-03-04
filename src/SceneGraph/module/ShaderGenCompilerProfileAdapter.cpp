#include <hgl/graph/module/ShaderGenCompilerProfileAdapter.h>
#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <hgl/shadergen/ShaderCompilerProfileAPI.h>

namespace hgl::graph
{
    void ApplyShaderCompilerProfile(const mtl::contract::PhysicalDeviceProfileLite *profile)
    {
        if (!profile)
            return;

        SetShaderCompilerPhysicalDeviceProfile(*profile);
    }

    void ApplyShaderCompilerProfileFromRequest(const mtl::contract::ShaderGenRequest *request)
    {
        if (!request || !request->has_physical_device_profile)
            return;

        ApplyShaderCompilerProfile(&request->physical_device_profile);
    }
}
