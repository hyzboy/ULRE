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
    constexpr const int kPerObjectBindingL2WIndex       = 1;    ///< 实例 → l2w 行索引表
    constexpr const int kPerObjectBindingPrivateDataIndex  = 3;    ///< 实例 → 材质私有数据行索引表（MaterialPrivateDataIndex）
    // 顶点数据 SSBO（MeshShader 方向：顶点输入统一为 SSBO）——每对象大 buffer
    constexpr const int kPerObjectBindingVertexPosition = 4;    ///< 顶点位置 SSBO
    constexpr const int kPerObjectBindingVertexUV       = 5;    ///< 顶点 UV SSBO
    constexpr const int kPerObjectBindingVertexNTB      = 6;    ///< 顶点 NTB SSBO
    constexpr const int kPerObjectBindingVertexIndex    = 8;    ///< 顶点索引 SSBO
    constexpr const int kPerObjectBindingVertexColor    = 9;    ///< 顶点颜色 SSBO
    constexpr const int kPerObjectBindingVertexLuminance = 10;   ///< 顶点亮度 SSBO
    constexpr const int kPerObjectBindingVertexTransformID = 11;  ///< 顶点 TransformID SSBO（调色板变换索引）
    constexpr const int kPerObjectBindingVertexSize      = 12;    ///< 顶点 Size/宽度 SSBO（Line width）
    constexpr const int kPerObjectBindingMeshDrawParams  = 13;    ///< mesh per-draw 参数表 SSBO（indirect 合批查表）
    // CharQuad 文本字符 SSBO（TextCharQuad mesh shader 模式）
    constexpr const int kPerObjectBindingTextCharInfo     = 14;    ///< 字符信息 SSBO
    constexpr const int kPerObjectBindingTextCharStyle    = 15;    ///< 字符样式 SSBO
    constexpr const int kPerObjectBindingTextCharInstance = 16;    ///< 字符实例 SSBO

    enum class DescriptorSetType:int
    {
        Unknow=-1,

        Scene=0,        ///< 全局 UBO 集（camera/sky/viewport/color_palette），所有材质共用，一帧写/绑一次
        PerObject,      ///< per-object/per-draw SSBO 集（l2w/l2w_index_rows/joint/material_private_data_index_rows）
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
