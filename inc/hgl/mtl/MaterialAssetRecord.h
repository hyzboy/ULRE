#pragma once

/// MaterialAssetRecord.h — 可序列化材质资产描述符
///
/// 这是一个纯数据结构（允许 std::string），用于描述一个完整的材质创建参数。
/// 目前作为"硬编码常量表"在示例程序中使用，模拟从外部文件（JSON/BIN）加载材质配置。
/// 后续只需替换填充该结构体的数据来源，即可无缝切换到真正的外部序列化方案。
///
/// 结构关系：
///   MaterialCreateConfig           通用底层配置（着色器标记位、图元类型等）
///   ├─ Material2DCreateConfig      2D 材质（坐标系、position 格式）
///   └─ Material3DCreateConfig      3D 材质（摄像机/天空/光照模型）
///      ├─ SkyMinimalCreateConfig   仅预设构造参数，无额外字段
///      ├─ TerrainGridCreateConfig  仅预设构造参数，无额外字段
///      └─ BillboardMaterialCreateConfig  固定/动态大小 + 混合模式等
///
/// MaterialAssetRecord 将上述所有字段展平到单个结构体中，
/// 通过 dim 字段区分 2D / 3D，通过 preset 字段决定子类型处理逻辑。

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/LightingModel.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/mtl/RenderAlphaMode.h>
#include <hgl/common/CoordinateSystem.h>
#include <hgl/common/PrimitiveTypeDef.h>
#include <hgl/common/TextureSamplerTypeDef.h>
#include <hgl/vk/VertexAttrib.h>
#include <hgl/vk/VKFormat.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>

#include <vector>
#include <string>

namespace hgl::graph::mtl
{

/// 材质资产描述符 —— 可序列化的材质创建参数平铺结构体
///
/// 设计原则：
/// - 与序列化层解耦：字段全部是基础类型或 std::string，无运行时指针
/// - 2D/3D 共用一个结构体，通过 dim 字段区分；未使用的字段在对应分支被忽略
/// - 纹理路径为空时跳过加载；pos_format 为零初始化时使用维度默认值
/// - billboard 子结构仅在 preset == Billboard2DFixed / Billboard2DDynamic 时生效
struct MaterialAssetRecord
{
    // ── 标识 ──────────────────────────────────────────────────────────────────
    std::string     id;                                     ///< 逻辑资产名（缓存键 / 文件名干）
    std::string     domain_id;                              ///< 资源域标识（同 ShaderMaterialProgram 不同域 = 不同渲染批次）
    MaterialPreset  preset  = MaterialPreset::Standard;     ///< 材质预设

    // ── 维度选择 ──────────────────────────────────────────────────────────────
    enum class Dim : uint8 { D2, D3 } dim = Dim::D3;

    // ── 通用字段（2D & 3D 均有效）────────────────────────────────────────────
    PrimitiveType   prim    = PrimitiveType::Triangles;     ///< 图元类型
    bool            l2w     = true;                         ///< 是否包含 LocalToWorld 变换

    /// position 顶点属性格式；零初始化（vat_code==0）= 使用维度默认值
    /// （D2 默认 VAT_VEC2，D3 默认 VAT_VEC3）
    VAType          pos_format = {};

    // ── 2D 专用（dim == D2 时有效）────────────────────────────────────────────
    CoordinateSystem2D coord_2d = CoordinateSystem2D::NDC;  ///< 2D 坐标系

    // ── 3D 专用（dim == D3 时有效）────────────────────────────────────────────
    bool                camera      = true;                 ///< 包含摄像机矩阵 (IncludeCamera)
    bool                sky         = false;                ///< 包含天空信息   (IncludeSky)
    SkyLightAmbientModel sky_ambient = SkyLightAmbientModel::Simple;    ///< 天光模型
    LightingModel        lighting    = LightingModel::Lambert;          ///< 光照模型

    // ── MaterialInstance 管线预设 ──────────────────────────────────────────────
    GraphicsPipelinePreset pipeline = GraphicsPipelinePreset::Solid3D;

    // ── MaterialInstance 顶点输入覆写（可选）─────────────────────────────────────
    // 空列表 = 使用 ShaderMaterialProgram 默认 VIL。
    // 非空 = 在 CreateMI 时构建 VILConfig 覆写指定 attrib 的格式。
    struct VILOverride
    {
        VertexAttrib attrib = VAN::Position;
        VkFormat     format = PF_UNDEFINED;
    };

    [[deprecated("use GeometryVertexFormat-based auto derivation via AcquireMI(rec, gvf, ...)")]]
    std::vector<VILOverride> mi_vil_overrides;

    // ── 纹理配置列表（以 SamplerSlot 为显式 key）────────────────────────────────
    struct TextureSlotConfig
    {
        SamplerSlot       slot        = SamplerSlot::BaseColor;          ///< 目标插槽
        TextureSourceMode source_mode = TextureSourceMode::None;         ///< None = 不覆写来源模式
        std::string       path;                                          ///< 空字符串 = 不加载贴图
    };

    std::vector<TextureSlotConfig> textures;

    // ── Billboard 专用配置（仅对 Billboard2DFixed / Billboard2DDynamic 有效）──
    struct BillboardConfig
    {
        bool               fixed_size          = false;                     ///< 像素固定大小
        uint32             pixel_w             = 64;                        ///< 像素宽度
        uint32             pixel_h             = 64;                        ///< 像素高度
        RenderAlphaMode    blend_mode          = RenderAlphaMode::Transparent;    ///< 混合模式
        TextureChannelHint base_color_channel  = TextureChannelHint::RGBA;  ///< 通道提示
        bool               front_face_ccw      = false;                     ///< false = Clockwise
        std::string        texture_id;                                      ///< 纹理唯一标识
    } billboard;
};

} // namespace hgl::graph::mtl
