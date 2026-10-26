// ShaderLibrary/common/interstage_locations.glsl
//
// 双 Stage（Mesh Shader ↔ Fragment Shader）交接语义位置的唯一真源定义。
// 与 C++ src/ShaderProgramBuild/contract/ShaderSemanticRegistry.cpp 的
// InterStageSemanticRegistry 中的 stable_location 逐字段严格一一对应。
//
// 规则：严禁在 .mesh 或 .frag 中直接硬编码数字 location，一律使用本文件定义的宏。

#ifndef HGL_INTERSTAGE_LOCATIONS_GLSL
#define HGL_INTERSTAGE_LOCATIONS_GLSL

#define HGL_LOCATION_DATA_INDEX_ID   0
#define HGL_LOCATION_WORLD_POS       1
#define HGL_LOCATION_WORLD_NORMAL    2
#define HGL_LOCATION_UV0             3
#define HGL_LOCATION_STYLE_ID        4
#define HGL_LOCATION_VERTEX_COLOR    5
#define HGL_LOCATION_FRAG_DIRECTION  6
#define HGL_LOCATION_LUMINANCE       7
#define HGL_LOCATION_WORLD_TANGENT   8
#define HGL_LOCATION_WORLD_BINORMAL  9

#endif // HGL_INTERSTAGE_LOCATIONS_GLSL
