#pragma once
#include<hgl/graph/mtl/StdMaterial.h>

STD_MTL_NAMESPACE_BEGIN

// 注意：走的几何Shader，输出是三角形条

constexpr const char shader_billboard_vertex_ccw[]=R"(
const vec2 BillboardVertex[4]=vec2[]
(
    vec2(-0.5,-0.5),
    vec2(-0.5, 0.5),
    vec2( 0.5,-0.5),
    vec2( 0.5, 0.5)
);
)";

constexpr const char shader_billboard_vertex_cw[]=R"(
const vec2 BillboardVertex[4]=vec2[]
(
    vec2(-0.5,-0.5),
    vec2( 0.5,-0.5),
    vec2(-0.5, 0.5),
    vec2( 0.5, 0.5)
);
)";

STD_MTL_NAMESPACE_END
