// ShaderLibrary/module/stage/stage_common.glsl
//
// 网格阶段固定主着色器前导的下半段：descriptor 宏、scene UBO、l2w SSBO、
// MeshDrawParams / MeshDrawCommands 结构、全部顶点属性 buffer_reference 声明。
//
// 为什么无条件包含 l2w_ssbo.glsl：它只声明 buffer_reference 类型与 l2w 垫片宏，
// 未被引用的形状不会在 SPIR-V 里留下痕迹（与生成器的 needs_l2w 判定等价）。
//
// 顶点 buffer_reference 全部声明：scalar 布局下未使用的类型不产生任何 SPIR-V，
// 这样各形状无需按属性格式裁剪本文件。
//
// 本文件不得含结构性 #if 分支。


#include "contract/descriptor_macros.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/descriptor/l2w_ssbo.glsl"

// mesh shader：无 gl_VertexIndex；VertexIndexID = MeshVertexIndex（main 解析）
uint MeshVertexIndex;
#define VertexIndexID (MeshVertexIndex)

struct MeshDrawParams
{
    uint index_base;
    uint vertex_base;
    uint is_indexed;
    uint index_count;
    uint vertex_count;
    uint data_index_count;
    uint data_vertex_count;
    float char_height;
    uint first_instance;
    uint64_t addr_position;
    uint64_t addr_uv;
    uint64_t addr_ntb;
    uint64_t addr_color;
    uint64_t addr_luminance;
    uint64_t addr_transform_id;
    uint64_t addr_size;
    uint64_t addr_index;
};
layout(buffer_reference, scalar, buffer_reference_align=16) buffer MeshDrawParamsRef { MeshDrawParams rows[]; };

struct MeshDrawCommand
{
    uint geometry_id;
    uint first_instance;
};
layout(buffer_reference, scalar, buffer_reference_align=8) buffer MeshDrawCommandsRef { MeshDrawCommand cmds[]; };

MeshDrawParams draw_params;

layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexIndexRef       { uint data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexPositionRef    { vec3 data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexPositionV2Ref  { vec2 data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexPositionPackedRef { uint data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexUVRef          { vec2 data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexUVPackedRef    { uint data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexNTBRef         { vec3 data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexNTBPackedRef   { uint data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexColorRef       { vec4 data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexColorPackedRef { uint data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexLuminanceRef   { uint data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexTransformIDRef { uint data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexSizeRef        { vec2 data[]; };

#define sbo_vertex_index VertexIndexRef(draw_params.addr_index)
