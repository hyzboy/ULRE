#include <iostream>
#include <glm/glm.hpp>

using Float     =float;
using Float2    =glm::vec2;
using Float3    =glm::vec3;
using Float4    =glm::vec4;

using Integer   =int;
using Int2      =glm::ivec2;
using Int3      =glm::ivec3;
using Int4      =glm::ivec4;

using UInteger  =unsigned int;
using Uint2     =glm::uvec2;
using Uint3     =glm::uvec3;
using Uint4     =glm::uvec4;

using Boolean   =bool;
using Bool2     =glm::bvec2;
using Bool3     =glm::bvec3;
using Bool4     =glm::bvec4;

using Double    =double;
using Double2   =glm::dvec2;
using Double3   =glm::dvec3;
using Double4   =glm::dvec4;

using Matrix2   =glm::mat2;
using Matrix3   =glm::mat3;
using Matrix4   =glm::mat4;

using Matrix2x3 =glm::mat2x3;
using Matrix2x4 =glm::mat2x4;

using Matrix3x2 =glm::mat3x2;
using Matrix3x4 =glm::mat3x4;

using Matrix4x2 =glm::mat4x2;
using Matrix4x3 =glm::mat4x3;

class UBOBase
{
public:

    UBOBase()=default;
    virtual ~UBOBase()=default;

    virtual const char *    GetSource()const=0;
    virtual const size_t    GetSourceLength()const=0;
};

#define UBO_FIELD_Cpp_Source(type,name) type name;
#define UBO_FIELD_GLSL_Source(type,name) "\t" #type " " #name ";\n"

#define UBO_DEFINE(name) class UBO##name:public UBOBase   \
{ \
    UBO_FIELD_##name(UBO_FIELD_Cpp_Source) \
\
    static constexpr const char glsl_source_code[]=UBO_FIELD_##name(UBO_FIELD_GLSL_Source); \
\
public: \
\
    static const char *    StaticGetSource(){return glsl_source_code;}    \
    static const size_t    StaticSourceLength(){return sizeof(glsl_source_code);} \
\
    virtual const char *    GetSource()const{return glsl_source_code;}    \
    virtual const size_t    GetSourceLength()const{return sizeof(glsl_source_code);} \
};

#define UBO_FIELD_ViewportInfo(F) \
    F(Matrix4,ortho_matrix)            \
    F(Float2,canvas_resolution)        \
    F(Float2,viewport_resolution)      \
    F(Float2,inv_viewport_resolution)

UBO_DEFINE(ViewportInfo)        //注意这里的名称必须与上方的UBO_FILED_??????一致

int main()
{
    UBOViewportInfo vp_info;

    const char *glsl_string=UBOViewportInfo::StaticGetSource();
    const size_t len=UBOViewportInfo::StaticSourceLength();

    std::cout << "sizeof(ViewportInfo) = " << len << "\n\n";
    std::cout << "GLSL UBO definition:\n";
    std::cout << "layout(std140) uniform ViewportInfoUBO\n";
    std::cout << "{\n";
    std::cout << glsl_string;
    std::cout << "};\n";
    return 0;
}