#pragma once

#include<hgl/graph/mtl/StdMaterial.h>

STD_MTL_NAMESPACE_BEGIN
namespace func
{
//C++端使用一个RG8UI或RGB16UI格式的顶点输入流来传递Assign数据，其中x为LocalToWorld ID，y为MaterialInstance ID

    constexpr const char GetLocalToWorld[]=R"(
mat4 GetLocalToWorld()
{
    return l2w.mats[Assign.x];
}
)";

    constexpr const char MaterialInstanceID[]="MaterialInstanceID";

constexpr const char HandoverMI[]=R"(
void HandoverMI()
{
#if ShaderStage == VertexShader
    Output.MaterialInstanceID=Assign.y;
#elif ShaderStage == GeometryShader
    Output.MaterialInstanceID=Input[0].MaterialInstanceID;
#else
    Output.MaterialInstanceID=Input.MaterialInstanceID;
#endif
}
)";

    constexpr const char GetMI[]=R"(
MaterialInstance GetMI()
{
#if ShaderStage == VertexShader
    return mtl.mi[Assign.y];
#else
    return mtl.mi[Input.MaterialInstanceID];
#endif
}
)";

// Joint数据分Joint ID和Joint Weight两部分
// Joint ID是一个uvec4，在shader中为整数。在C++端可使用RGBA8UI或是RGBA16UI来传递。
// Joint Weight是权重，在shader中为浮点。在C++端使用RGBA8或RGBA4来传递。

    constexpr const char GetJointMatrix[]=R"(
mat4 GetJointMatrix()
{
    return  joint.mats[JointID.x]*JointWeight.x+
            joint.mats[JointID.y]*JointWeight.y+
            joint.mats[JointID.z]*JointWeight.z+
            joint.mats[JointID.w]*JointWeight.w;
}
)";
}//namespace func
STD_MTL_NAMESPACE_END
