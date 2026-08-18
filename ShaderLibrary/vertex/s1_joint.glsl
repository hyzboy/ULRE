// @ulre begin
// @ulre name s1_joint
// @ulre kind Utility
// @ulre priority 0
// @ulre ssbo VertexJoint VertexJoint 4 Vertex optional
// @ulre end
// Stage 1: Joint 数据（JointID+JointWeight 一个 SSBO）——蒙皮骨架占位
// 未来 MeshShader/蒙皮实现填充读取与变换
#ifndef S1_JOINT_GLSL
#define S1_JOINT_GLSL

layout(set=VERTEX_SET, binding=VERTEX_JOINT_BINDING) readonly buffer VertexJointData
{
    uvec4 ids;
    vec4 weights;
} sbo_vertex_joint;

#endif // S1_JOINT_GLSL
