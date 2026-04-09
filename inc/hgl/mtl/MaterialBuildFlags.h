#pragma once

/// StdMaterial.h — 材质创建辅助枚举
///
/// 原 StdMaterial 虚函数链已被 CompileCompositorMaterial + StaticMaterialDef 取代。
/// 本文件仅保留 IncludeSky/IncludeCamera/IncludeL2W 枚举供
/// Material2DCreateConfig / Material3DCreateConfig 使用。

namespace hgl::graph
{
    namespace mtl
    {
        enum class IncludeSky:uint8
        {
            Without=0,
            With
        };

        enum class IncludeCamera:uint8
        {
            Without=0,
            With
        };

        enum class IncludeL2W:uint8
        {
            Without=0,
            With
        };
    }//namespace mtl
}//namespace hgl::graph
