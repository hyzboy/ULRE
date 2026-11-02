#pragma once

#include <vulkan/vulkan.h>
#include <hgl/type/StrChar.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    /// 注：Set 0（Scene 集）与其绑定枚举 `SceneBinding` 已**整体退场**——相机 / 调色板 /
    /// 全局地址表 / sky / viewport / shadow 全部改走 BDA（地址进 global_addresses 表，
    /// 表基址经 pc_root.addr_global_addresses 下发）⇒ 本引擎不再有任何 Scene 集绑定，
    /// 描述符集只剩 Bindless 一个（集号随之收敛为 0）。

    enum class DescriptorSetType:int
    {
        Unknown=-1,        ///<Phase 7 拼写修正：Unknown（枚举值不变，序列化契约不受影响）

        Bindless=0,     ///< 全局 Bindless 纹理数组集合（Set 0），一帧绑一次
                        ///< （Scene/PerObject/Vertex/Material 集已随 BDA 化全部退场——行表/顶点流/
                        ///<  材质行经 pc_root 地址 + buffer_reference 寻址，viewport / sky / shadow
                        ///<  亦走 global_addresses ⇒ 唯一集合就是本集）

        ENUM_CLASS_RANGE(Bindless,Bindless)
    };

    constexpr const size_t DESCRIPTOR_SET_TYPE_COUNT=size_t(DescriptorSetType::RANGE_SIZE);

    /// 按索引调试名（Scene 集退场后唯一集合：Bindless=0）
    constexpr const char *DescriptSetTypeName[]=
    {
        "Bindless"
    };

    inline const char *GetDescriptorSetTypeName(const enum class DescriptorSetType &type)
    {
        if(type==DescriptorSetType::Unknown)return "Unknown";

        RANGE_CHECK_RETURN_NULLPTR(type);

        return DescriptSetTypeName[(size_t)type];
    }

    /// 注：`descriptor_macros.glsl` 已改由 ShaderConstGen 生成（真源
    /// `inc/hgl/shader/contract/PipelineContract.h` 的 `DescriptorSetId`），
    /// 原先为此服务的 `kDescriptorBindingMacros` 规范表与
    /// `EmitDescriptorBindingDefines()` 已删除 —— 一张只为"能生成"而存在的
    /// 常量表就是第二真源。集合号与 GLSL 宏的相等性由生成的
    /// `inc/hgl/shader/generated/ShaderConstantsParity.gen.h` 用 static_assert 守住。

}//namespace hgl::graph
