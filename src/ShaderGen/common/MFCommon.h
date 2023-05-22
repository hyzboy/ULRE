#pragma once

#include<hgl/graph/mtl/StdMaterial.h>

STD_MTL_NAMESPACE_BEGIN
namespace func
{
    constexpr const char GetLocalToWorld[]=R"(
mat4 GetLocalToWorld()
{
    return mat4(LocalToWorld_0,
                LocalToWorld_1,
                LocalToWorld_2,
                LocalToWorld_3);
}
)";

    constexpr const char GetJointMatrix[]=R"(
mat4 GetJointMatrix()
{
    return  joint.mats[JointID.x]*JointWeight.x+
            joint.mats[JointID.y]*JointWeight.y+
            joint.mats[JointID.z]*JointWeight.z+
            joint.mats[JointID.w]*JointWeight.w;
}
)";

    constexpr const char HandoverMI[]=R"(
void HandoverMI()
{
    Output.MaterialInstanceID=MaterialInstanceID;
})";

    constexpr const char GetMI[]=R"(
MaterialInstance GetMI()
{
    return mtl.mi[Input.MaterialInstanceID];
})";
}//namespace func
STD_MTL_NAMESPACE_END
