#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(local_size_x = 64) in;

layout(push_constant) uniform LevelPushConstant
{
    uint level_start_offset;
    uint level_node_count;
    uint64_t pos_addr;
    uint64_t rot_addr;
    uint64_t scale_addr;
    uint64_t parent_addr;
    uint64_t eval_order_addr;
    uint64_t world_addr;
} pc;

// 全部走 BDA：地址经 push constant 下发，无 set 无 binding
layout(buffer_reference, std430, buffer_reference_align=16) buffer Mat4ArrayRef { mat4 values[]; };
layout(buffer_reference, std430, buffer_reference_align=16) buffer Vec4ArrayRef { vec4 values[]; };
layout(buffer_reference, std430, buffer_reference_align=16) buffer UintArrayRef { uint values[]; };

// 句柄哨兵。⚠ 本文件由示例直接按路径加载、不走 `@ulre` 模块系统，故此处**保留声明式
// 常量**而非 #include（值由 ShaderConstGen 的 [[verify_glsl]] const_decl 通道对拍）。
const uint INVALID_HANDLE = 0xFFFFFFFFu;

// 由原生 TRS 合成局部矩阵（与 CPU 端 ComposeLocalMatrix 同式：M = T * R * S）
mat4 ComposeLocalMatrix(vec3 p,vec4 q,vec3 s)
{
    const float x = q.x, y = q.y, z = q.z, w = q.w;

    // GLSL mat3 构造按**列**填（col0 / col1 / col2），与 glm::mat4_cast 一致
    mat3 r = mat3(1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y + z * w), 2.0 * (x * z - y * w),
                  2.0 * (x * y - z * w), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z + x * w),
                  2.0 * (x * z + y * w), 2.0 * (y * z - x * w), 1.0 - 2.0 * (x * x + y * y));

    mat4 local = mat4(r);

    local[0] *= s.x;
    local[1] *= s.y;
    local[2] *= s.z;
    local[3]  = vec4(p,1.0);

    return local;
}

void main()
{
    uint local_idx = gl_GlobalInvocationID.x;
    if (local_idx >= pc.level_node_count)
        return;

    Vec4ArrayRef positions = Vec4ArrayRef(pc.pos_addr);
    Vec4ArrayRef rotations = Vec4ArrayRef(pc.rot_addr);
    Vec4ArrayRef scales    = Vec4ArrayRef(pc.scale_addr);
    UintArrayRef parent_indices = UintArrayRef(pc.parent_addr);
    UintArrayRef eval_order     = UintArrayRef(pc.eval_order_addr);
    Mat4ArrayRef world_matrices = Mat4ArrayRef(pc.world_addr);

    uint node_idx = eval_order.values[pc.level_start_offset + local_idx];
    uint parent_idx = parent_indices.values[node_idx];

    mat4 local_mat = ComposeLocalMatrix(positions.values[node_idx].xyz,
                                        rotations.values[node_idx],
                                        scales.values[node_idx].xyz);

    if (parent_idx != INVALID_HANDLE)
    {
        world_matrices.values[node_idx] = world_matrices.values[parent_idx] * local_mat;
    }
    else
    {
        world_matrices.values[node_idx] = local_mat;
    }
}
