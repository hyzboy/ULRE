#ifdef THIS_IS_VERTEX_SHADER
    #define VS_OUTPUT_LAYOUT out
#else
    #define VS_OUTPUT_LAYOUT in
#endif//THIS_IS_VERTEX_SHADER

#ifdef FS_USE_POSITION
layout(location = 0) VS_OUTPUT_LAYOUT vec3 v2fPosition;
#endif//FS_USE_POSITION

#ifdef FS_USE_COLOR
layout(location = 1) VS_OUTPUT_LAYOUT vec3 v2fColor;
#endif//FS_USE_COLOR

#ifdef FS_USE_NORMAL
layout(location = 2) VS_OUTPUT_LAYOUT vec3 v2fNormal;
#endif//FS_USE_NORMAL

#ifdef FS_USE_TANGENT
layout(location = 3) VS_OUTPUT_LAYOUT vec3 v2fTangent;
#endif//FS_USE_TANGENT

#ifdef FS_USE_TEX_COORD
layout(location = 4) VS_OUTPUT_LAYOUT vec2 v2fTexCoord;
#endif//FS_USE_TEX_COORD
