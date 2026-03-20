#ifndef L2W_SSBO_GLSL
#define L2W_SSBO_GLSL

#include "common/descriptor_macros.glsl"

layout(set=TID_SET, binding=TID_BINDING) readonly buffer TransformIDData {
    uint ids[];
} tid;

layout(set=L2W_SET, binding=L2W_BINDING) readonly buffer LocalToWorldData {
    mat4 mats[];
} l2w;

uint GetTransformID()
{
    return tid.ids[gl_InstanceIndex];
}

mat4 GetTransform()
{
    return l2w.mats[GetTransformID()];
}

#endif 