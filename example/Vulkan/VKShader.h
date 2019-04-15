#pragma once
#include"VK.h"

VK_NAMESPACE_BEGIN
class Shader
{
    
public:


};//class Shader

VkShaderModule CreateShaderModule(VkDevice device,const uint32_t *spv_data,const uint32_t spv_size,const VkShaderStageFlagBits shader_stage_bit);

inline VkShaderModule CreateVertexShader    (VkDevice device,const uint32_t *spv_data,const uint32_t spv_size) { return CreateShaderModule(device,spv_data,spv_size,VK_SHADER_STAGE_VERTEX_BIT); }
inline VkShaderModule CreateFragmentShader  (VkDevice device,const uint32_t *spv_data,const uint32_t spv_size) { return CreateShaderModule(device,spv_data,spv_size,VK_SHADER_STAGE_FRAGMENT_BIT); }
inline VkShaderModule CreateGeometryShader  (VkDevice device,const uint32_t *spv_data,const uint32_t spv_size) { return CreateShaderModule(device,spv_data,spv_size,VK_SHADER_STAGE_GEOMETRY_BIT); }
inline VkShaderModule CreateTessCtrlShader  (VkDevice device,const uint32_t *spv_data,const uint32_t spv_size) { return CreateShaderModule(device,spv_data,spv_size,VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT); }
inline VkShaderModule CreateTessEvalShader  (VkDevice device,const uint32_t *spv_data,const uint32_t spv_size) { return CreateShaderModule(device,spv_data,spv_size,VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT); }
inline VkShaderModule CreateComputeShader   (VkDevice device,const uint32_t *spv_data,const uint32_t spv_size) { return CreateShaderModule(device,spv_data,spv_size,VK_SHADER_STAGE_COMPUTE_BIT); }
VK_NAMESPACE_END
