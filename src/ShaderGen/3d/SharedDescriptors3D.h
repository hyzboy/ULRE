#pragma once

// SharedDescriptors3D — 3D 材质公共 FixedDescriptorEntry 集合。
//
// 用途：消除 M_*.cpp 文件间重复的 constexpr FixedDescriptorEntry[] 数组。
// 凡与本文件中的公共集合完全一致的材质，直接引用此处的常量，不再各自维护副本。
//
// 覆盖材质（当前阶段）：
//   kBase3DDescriptors         → VertexColor3D
//   kBase3DWithMIDescriptors   → PureColor3D、Gizmo3D、VertexLuminance3D
//
// 暂未覆盖的材质（独特 entry 或 stage flags 存在差异，留待后续步骤处理）：
//   PBRColor3D（with_sky，mtl_texture_layer_rows stage flags 不同）
//   SkyMinimal（sky entry 使用 VK_SHADER_STAGE_FRAGMENT_BIT）
//   VertexPattleColor3D（color_pattle UBO 代替 MI SSBOs）
//   Standard / StandardTextureArray（已用 StandardSharedSpec.h）
//
// 关于顺序：CompileCompositorMaterial 按语义 (DescriptorSemantic) 处理 entry，
// 顺序不影响正确性；此处保持 Scene → Transform → Material 的逻辑分组。

#include <hgl/mtl/FixedDescriptorEntry.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/common/RenderAssignDef.h>
#include <vulkan/vulkan.h>

namespace hgl::graph::mtl
{

// ── 基础集合：Scene(viewport+camera) + Transform(l2w+l2w_index_rows) ─────────
// 所有 3D 材质均需要这 4 个 descriptor。
constexpr FixedDescriptorEntry kBase3DDescriptors[] = {
    { DescriptorSetType::Scene,     DescriptorKind::UBO,              uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport",        "ViewportInfo",           nullptr, DescriptorSemantic::ViewportInfo,          TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO  },
    { DescriptorSetType::Scene,     DescriptorKind::UBO,              uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",          "CameraInfo",             nullptr, DescriptorSemantic::CameraInfo,            TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO  },
    { DescriptorSetType::Transform, TransformDescriptorKind,          uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w",             "LocalToWorldData",        nullptr, DescriptorSemantic::LocalToWorld,          TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(TransformDescriptorKind)          },
    { DescriptorSetType::Transform, DescriptorKind::SSBO,             uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows",  "LocalToWorldIndexRows",   nullptr, DescriptorSemantic::LocalToWorldIndexTable,TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO },
};

constexpr uint32_t kBase3DDescriptorCount =
    uint32_t(sizeof(kBase3DDescriptors) / sizeof(kBase3DDescriptors[0]));

// ── 基础集合 + 材质实例 SSBOs（mtl + mtl_data_index_rows + mtl_texture_layer_rows）────
// 适用于有 MaterialInstance 但无 Sky 的 3D 材质（PureColor3D、Gizmo3D、VertexLuminance3D 等）。
constexpr FixedDescriptorEntry kBase3DWithMIDescriptors[] = {
    { DescriptorSetType::Scene,     DescriptorKind::UBO,              uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport",              "ViewportInfo",           nullptr, DescriptorSemantic::ViewportInfo,           TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO  },
    { DescriptorSetType::Scene,     DescriptorKind::UBO,              uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",                "CameraInfo",             nullptr, DescriptorSemantic::CameraInfo,             TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO  },
    { DescriptorSetType::Transform, TransformDescriptorKind,          uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w",                   "LocalToWorldData",        nullptr, DescriptorSemantic::LocalToWorld,           TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(TransformDescriptorKind)          },
    { DescriptorSetType::Transform, DescriptorKind::SSBO,             uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows",         "LocalToWorldIndexRows",   nullptr, DescriptorSemantic::LocalToWorldIndexTable, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO },
    { DescriptorSetType::Material,  MaterialInstanceDescriptorKind,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl",                   "MaterialInstanceData",    nullptr, DescriptorSemantic::MaterialSSBOSlotData,   TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::PBRSurface,  GetDescriptorSemanticLayerByKind(MaterialInstanceDescriptorKind)   },
    { DescriptorSetType::Material,  DescriptorKind::SSBO,             uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows",    "DataIndexRows",           nullptr, DescriptorSemantic::MaterialSSBOIndexTable,  TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO },
    { DescriptorSetType::Material,  DescriptorKind::SSBO,             uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_texture_layer_rows", "TextureLayerRows",        nullptr, DescriptorSemantic::MaterialTextureLayerTable,TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO },
};

constexpr uint32_t kBase3DWithMIDescriptorCount =
    uint32_t(sizeof(kBase3DWithMIDescriptors) / sizeof(kBase3DWithMIDescriptors[0]));

} // namespace hgl::graph::mtl
