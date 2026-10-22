#pragma once

namespace hgl::graph::mtl {}

#include <hgl/type/DataType.h>
#include <hgl/mtl/contract/ShaderGenContract.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl::contract
{
    using namespace hgl::graph::mtl;

    // 硬性 Vulkan 1.4（vulkan1.4.md 第 1 项）：版本/SPIR-V 阶梯整体塌缩为常量。
    // 1.3 起 SPIR-V 1.6 即核心，1.4 下无条件成立——不再按 api_version 探测。
    constexpr uint32_t MakeVkVersion(const uint32_t major, const uint32_t minor, const uint32_t patch = 0)
    {
        return (major << 22) | (minor << 12) | patch;
    }

    constexpr uint32_t VkVersionMajor(const uint32_t version)
    {
        return version >> 22;
    }

    constexpr uint32_t VkVersionMinor(const uint32_t version)
    {
        return (version >> 12) & 0x3ffu;
    }

    constexpr uint32_t SPV_VERSION_1_6 = (1u << 16) | (6u << 8);
    constexpr uint32_t TARGET_VULKAN_VERSION = MakeVkVersion(1, 4);
    constexpr uint32_t TARGET_SPV_VERSION = SPV_VERSION_1_6;

    // 代码生成代数：GLSL 发射逻辑语义变化（如材质数据 Arena+BDA 改造）时 +1，
    // 一次性整体更换全部 SPV artifact 缓存键（防新旧生成器产物混用）
    //
    // 5：stage key 的 definition_hash / interface_hash 从「已生成的 GLSL 文本」
    //    换成「emit 之前的声明源」。同一份输入算出的键值必然与旧版不同，
    //    旧 SPV 缓存全部作废——正是这里换代的目的。
    // 6：interface_hash 折叠 inter-stage 条目时不再吃结构体 padding（改走
    //    InterStageSemanticContractEntry 的显式逐字段 operator<<），键值随之变化。
    // 7：Sky 模板整删纯校验用的 OutputPolicy slot（sky.glsl.tmpl 没有对应占位符，
    //    该模块从不发射）。resolved_template_hash 随 module_root_count 变化，
    //    sky 程序缓存自然失效；这里一并换代，避免新旧声明闭包混用。
    // 8：编译目标哈希不再折叠物理设备身份（vendor_id / device_id / api_version /
    //    limits / features）。SPIR-V 是平台中立字节码，这些字段只影响 glslang 的
    //    校验门槛，不改变发射出的指令；唯一真正进 GLSL 文本的设备量是 mesh
    //    invocation 上限，它早已由 ClampMeshInvocationsByDevice 折进
    //    definition_hash / interface_hash。此前把它们塞进 compiler_hash，等于同一份
    //    SPV 按显卡型号各存一份 —— 发行版无法「只打包一套 SPV」。键值整体变化，
    //    这里换代作废旧缓存。
    constexpr uint32_t SHADERGEN_CODEGEN_GENERATION = 8;

    // 物理设备量（vendor/device/api_version/limits/features）**不得**折进任何
    // SPV 缓存键：SPIR-V 是平台中立字节码，设备量只影响 glslang 的校验门槛，
    // 不改变发射出的指令；唯一真正进 GLSL 文本的设备量是 mesh invocation 上限，
    // 它由 ClampMeshInvocationsByDevice 折进 definition_hash / interface_hash。
    // 设备量一旦进键，一个 SPV 就得按显卡型号各存一份，发行版无法「只打包一套」。
    //
    // 缓存键统一用 GetShaderCompilerProfileHash（见下）。

    // 编译目标哈希 —— **全部** SPV 缓存键（stage key / program key / artifact
    // metadata）唯一允许使用的编译器身份维度。
    //
    // 只吃三样常量：Vulkan 目标版本、SPIR-V 目标版本、代码生成代数。这三者才是
    // 真正决定 SPV 字节内容的因素；物理设备量一个都不进（理由见上一段）。
    //
    // `profile` 参数保留，一是守住调用点形状，二是未来若真出现「某设备必须换
    // 产物」的需求时有落点；当前实现刻意不读它。
    inline uint64 GetShaderCompilerProfileHash(
        const PhysicalDeviceProfileLite *profile) noexcept
    {
        (void)profile;

        uint32 vulkan_version = TARGET_VULKAN_VERSION;
        uint32 spv_version = TARGET_SPV_VERSION;

        hgl::hash::FNV1aHasher64 h;

        h << vulkan_version
          << spv_version
          << SHADERGEN_CODEGEN_GENERATION;

        return h;
    }
}
