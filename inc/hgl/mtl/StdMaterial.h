#pragma once

/// StdMaterial.h — 材质创建辅助枚举
///
/// 原 StdMaterial 虚函数链已被 CompileCompositorMaterial + FixedMaterialDef 取代。
/// 本文件仅保留 WithSky/WithCamera/WithLocalToWorld 枚举供
/// request 到构建参数转换阶段复用。

#include<hgl/type/String.h>

namespace hgl::graph
{
    namespace mtl
    {
        enum class WithSky:uint8
        {
            Without=0,
            With
        };

        enum class WithCamera:uint8
        {
            Without=0,
            With
        };

        enum class WithLocalToWorld:uint8
        {
            Without=0,
            With
        };
    }//namespace mtl
}//namespace hgl::graph
