#pragma once

/// 包含所有内置 3D 材质的工厂注册。
/// 只在需要通过 CreateMaterialCreateInfo("MaterialName", cfg) 方式按名称创建 3D 材质的
/// 编译单元（如 MaterialManager.cpp）中包含此头文件。
/// 此头文件会在每个包含它的编译单元中自动注册工厂，重复注册由注册系统负责去重。

#include<hgl/graph/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{

IMPL_MATERIAL_FACTORY(PureColor3D,          Material3DCreateConfig)
IMPL_MATERIAL_FACTORY(VertexColor3D,        const Material3DCreateConfig)
IMPL_MATERIAL_FACTORY(VertexLuminance3D,    Material3DCreateConfig)
IMPL_MATERIAL_FACTORY(VertexPattleColor3D,  const Material3DCreateConfig)
IMPL_MATERIAL_FACTORY(Gizmo3D,              Material3DCreateConfig)
IMPL_MATERIAL_FACTORY(TextureBlinnPhong,    const Material3DCreateConfig)

IMPL_MATERIAL_FACTORY(TerrainGrid,          const TerrainGridCreateConfig)
IMPL_MATERIAL_FACTORY(SkyMinimal,           const SkyMinimalCreateConfig)
IMPL_MATERIAL_FACTORY(Billboard2D,          BillboardMaterialCreateConfig)
IMPL_MATERIAL_FACTORY(BasicLit,             BasicLitMaterialCreateConfig)

}//namespace hgl::graph::mtl
