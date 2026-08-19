#pragma once

#include <vulkan/vulkan.h>
#include <hgl/type/StrChar.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    ///< Scene 集（Set 0）全局 UBO 硬编码绑定号。四个 UBO 全局共用、一帧写/绑一次。
    constexpr const int kSceneBindingCamera   = 0;    ///< 相机 UBO
    constexpr const int kSceneBindingSky      = 1;    ///< 天空/太阳光 UBO
    constexpr const int kSceneBindingViewport = 2;    ///< 视口 UBO
    constexpr const int kSceneBindingColorPalette = 3; ///< 顶点调色板 UBO

    ///< PerObject 集（Set 1）编译期固定 SSBO 硬编码绑定号。成员为引擎内部 per-object 结构，非 TOML 动态列表。
    constexpr const int kPerObjectBindingL2W            = 0;    ///< per-draw 变换数据
    constexpr const int kPerObjectBindingL2WIndexRows   = 1;    ///< 实例 → l2w 行索引表
    constexpr const int kPerObjectBindingJoint          = 2;    ///< 骨骼 joint 数据（预留）
    constexpr const int kPerObjectBindingDataIndexRows  = 3;    ///< 实例 → 材质数据槽行索引表
    // 顶点数据 SSBO（MeshShader 方向：顶点输入统一为 SSBO）——每对象大 buffer
    constexpr const int kPerObjectBindingVertexPosition = 4;    ///< 顶点位置 SSBO
    constexpr const int kPerObjectBindingVertexUV       = 5;    ///< 顶点 UV SSBO
    constexpr const int kPerObjectBindingVertexNTB      = 6;    ///< 顶点 NTB SSBO
    constexpr const int kPerObjectBindingVertexJoint    = 7;    ///< 顶点 Joint SSBO
    constexpr const int kPerObjectBindingVertexIndex    = 8;    ///< 顶点索引 SSBO（非索引绘制——索引数据统一 SSBO）

    enum class DescriptorSetType:int
    {
        Unknow=-1,

        Scene=0,        ///< 全局 UBO 集（camera/sky/viewport/color_palette），所有材质共用，一帧写/绑一次
        PerObject,      ///< per-object/per-draw SSBO 集（l2w/l2w_index_rows/joint/mtl_data_index_rows）
        Material,       ///< per-material 描述符集（mtl 数据槽/索引表）
        Bindless,       ///< 全局 Bindless 纹理数组集合（Set 3），一帧绑一次

        ENUM_CLASS_RANGE(Scene,Bindless)
    };

    constexpr const size_t DESCRIPTOR_SET_TYPE_COUNT=size_t(DescriptorSetType::RANGE_SIZE);

    constexpr const char *DescriptSetTypeName[]=
    {
        "Scene",
        "PerObject",
        "Material",
        "Bindless"
    };

    inline const char *GetDescriptorSetTypeName(const enum class DescriptorSetType &type)
    {
        if(type==DescriptorSetType::Unknow)return "Unknow";

        RANGE_CHECK_RETURN_NULLPTR(type);

        return DescriptSetTypeName[(size_t)type];
    }
}
