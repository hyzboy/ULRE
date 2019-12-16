#include<hgl/graph/shader/param/type.h>

BEGIN_SHADER_PARAM_NAMESPACE
const char *GetTypename(const ParamType pt)
{
    constexpr char *name[]=
    {
        "?boolean?",
        "?float?",
        "?double?",
        "?int?",
        "?uint?",
        "?matrix?",

        "boolean",
        "bvec2",
        "bvec3",
        "bvec4",

        "float",
        "vec2",
        "vec3",
        "vec4",

        "int",
        "ivec2",
        "ivec3",
        "ivec4",

        "uint",
        "uvec2",
        "uvec3",
        "uvec4",

        "double",
        "dvec2",
        "dvec3",
        "dvec4",

        "mat2",
        "mat3",
        "mat4",
        "mat3x2",
        "mat2x3",
        "mat4x2",
        "mat2x4",
        "mat4x3",
        "mat3x4",

        "sampler1D",
        "sampler2D",
        "sampler3D",
        "samplerCube",
        "sampler2DRect",

        "sampler1DArray",
        "sampler2DArray",
        "samplerCubeArray",

        "sampler1DShadow",
        "sampler2DShadow",
        "samplerCubeShadow",
        "sampler2DRectShadow",

        "sampler1DArrayShadow",
        "sampler2DArrayShadow",
        "samplerCubeArrayShadow",

        "sampler2DMS",
        "sampler2DMSArray",

        "samplerBuffer",

        "?ARRAY_1D?",
        "?ARRAY_2D?",

        "?UBO?",
        "?NODE?"
    };

    if(pt<ParamType::BEGIN_RANGE
     ||pt>ParamType::END_RANGE)return(nullptr);

    return name[size_t(pt)-size_t(ParamType::BEGIN_RANGE)];
}
END_SHADER_PARAM_NAMESPACE