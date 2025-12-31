mat4 GetJointMatrix()
{
// Joint数据分Joint ID和Joint Weight两部分
// Joint ID是一个uvec4，在shader中为整数。在C++端可使用RGBA8UI或是RGBA16UI来传递。
// Joint Weight是权重，在shader中为浮点。在C++端使用RGBA8或RGBA4来传递。

    return  joint.mats[JointID.x]*JointWeight.x+
            joint.mats[JointID.y]*JointWeight.y+
            joint.mats[JointID.z]*JointWeight.z+
            joint.mats[JointID.w]*JointWeight.w;
}
