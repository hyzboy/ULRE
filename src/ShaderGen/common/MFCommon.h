#pragma once

#include<hgl/graph/mtl/StdMaterial.h>

STD_MTL_NAMESPACE_BEGIN
namespace func
{
//C++端使用一个RG8UI或RGB16UI格式的顶点输入流来传递Assign数据，其中x为LocalToWorld ID，y为MaterialInstance ID

    constexpr const char MaterialInstanceID[]="MaterialInstanceID";

    constexpr const char GetLocalToWorld[]=R"(
mat4 GetLocalToWorld()
{
    return mat4(LocalToWorld_0,
                LocalToWorld_1,
                LocalToWorld_2,
                LocalToWorld_3);
}
)";
}//namespace func
STD_MTL_NAMESPACE_END
