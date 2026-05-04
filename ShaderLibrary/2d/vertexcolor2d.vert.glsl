#if defined(FETCH_COLOR_SSBO_BINDING)
#ifndef FETCH_COLOR_PROVIDER_ID
#define FETCH_COLOR_PROVIDER_ID 4
#endif

#define ATTRIB_SET     VERTEXSTREAMS_SET
#define ATTRIB_BINDING FETCH_COLOR_SSBO_BINDING
#define ATTRIB_TAG     Color

#if FETCH_COLOR_PROVIDER_ID == 4
#include "attribute_provider/ssbo_packed_rgba8.glsl"
#elif FETCH_COLOR_PROVIDER_ID == 3
#include "attribute_provider/ssbo_vec4.glsl"
#else
vec4 ReadAttrib_Color(uint)
{
    return vec4(1.0);
}
#endif

#undef ATTRIB_TAG
#undef ATTRIB_BINDING
#undef ATTRIB_SET
#else
layout(location=COLOR_LOCATION) in vec4 Color;
#endif

layout(location=0) out vec4 fragColor;

void main()
{
#if defined(FETCH_COLOR_SSBO_BINDING)
    fragColor = ReadAttrib_Color(gl_VertexIndex);
#else
    fragColor = Color;
#endif
    gl_Position = GetPosition2D();
}
