#pragma once

/// DescriptorRequirement.h
///
/// 统一的 descriptor / push-constant 资源需求声明。
/// 所有 ColorSource（内置 sampler PCG、用户自定义 PCG、纯函数）都必须通过此接口
/// 向 BindingAllocator 显式声明自己需要哪些 binding，不允许任何"幕后塞 binding"。

#include <hgl/common/ShaderStageDef.h>
#include <cstdint>
#include <string>

namespace hgl::graph
{

/// descriptor 类型（值与 VkDescriptorType 对应，不直接引入 vulkan.h 以避免头文件污染）
enum class DescriptorType : uint32_t
{
    Sampler                  = 0,
    CombinedImageSampler     = 1,
    SampledImage             = 2,
    StorageImage             = 3,
    UniformTexelBuffer       = 4,
    StorageTexelBuffer       = 5,
    UniformBuffer            = 6,
    StorageBuffer            = 7,
    UniformBufferDynamic     = 8,
    StorageBufferDynamic     = 9,
    InputAttachment          = 10,
};

/// binding 号分配策略
enum class BindingPolicy : uint8_t
{
    Auto,               ///< 由 BindingAllocator 按 slot 字典序自动分配（推荐）
    FixedSet,           ///< set 号固定，binding 号在该 set 内自动分配
    FixedSetAndBinding, ///< set 和 binding 均固定；冲突则报 fatal
};

/// 单个 descriptor 资源的需求声明
struct DescriptorRequirement
{
    DescriptorType   type            = DescriptorType::CombinedImageSampler;
    uint32_t         count           = 1;       ///< 数组大小；0 = unbounded（bindless）
    ShaderStage      stages          = ShaderStage::Fragment;
    BindingPolicy    binding_policy  = BindingPolicy::Auto;
    uint32_t         fixed_set       = 0;       ///< 仅 FixedSet / FixedSetAndBinding 有效
    uint32_t         fixed_binding   = 0;       ///< 仅 FixedSetAndBinding 有效
    std::string      debug_name;                ///< 调试名，如 "Sampler_BaseColor"
};

/// push constant 需求声明
struct PushConstantRequirement
{
    ShaderStage  stages      = ShaderStage::Fragment;
    uint32_t     offset      = 0;
    uint32_t     size        = 0;
    std::string  debug_name;
};

/// BindingAllocator 分配完成后，每个 DescriptorRequirement 的解析结果
struct ResolvedBinding
{
    uint32_t  set     = 0;
    uint32_t  binding = 0;
    std::string debug_name; ///< 透传自 DescriptorRequirement::debug_name
};

} // namespace hgl::graph
