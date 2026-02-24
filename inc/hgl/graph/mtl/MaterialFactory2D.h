#pragma once

/// 包含所有内置 2D 材质的工厂注册。
/// 只在需要通过 CreateMaterialCreateInfo("MaterialName", cfg) 方式按名称创建 2D 材质的
/// 编译单元（如 MaterialManager.cpp）中包含此头文件。
/// 此头文件会在每个包含它的编译单元中自动注册工厂，重复注册由注册系统负责去重。

#include<hgl/graph/mtl/Material2DCreateConfig.h>

namespace hgl::graph::mtl{

IMPL_MATERIAL_FACTORY(VertexColor2D,        const Material2DCreateConfig)
IMPL_MATERIAL_FACTORY(PureColor2D,          Material2DCreateConfig)

IMPL_MATERIAL_FACTORY(PureTexture2D,        const Material2DCreateConfig)
IMPL_MATERIAL_FACTORY(RectTexture2D,        Material2DCreateConfig)
IMPL_MATERIAL_FACTORY(RectTexture2DArray,   Material2DCreateConfig)

IMPL_MATERIAL_FACTORY(Text2D,               const Text2DMaterialCreateConfig)

}//namespace hgl::graph::mtl
