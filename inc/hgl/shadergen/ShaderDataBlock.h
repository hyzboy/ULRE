#pragma once

#include<hgl/math/Math.h>

namespace hgl::graph
{
    // 此处类型重定义与GLSL中的共用，参考ShaderCreateInfo.cpp中的ShaderCreateInfo::CreateShader函数

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

    class ShaderDataBlock
    {
    public:

        ShaderDataBlock()=default;
        virtual ~ShaderDataBlock()=default;

        virtual const char *    GetSource()const=0;
        virtual const size_t    GetSourceLength()const=0;

        virtual const DescriptorSetType GetDescSetType()const=0;
    };

    #define SDB_FIELD_Cpp_Source(type,name) type name;
    #define SDB_FIELD_GLSL_Source(type,name) "\t" #type " " #name ";\n"

    #define SDB_DEFINE(bo,name,vn,dst) class ##bo##name:public ShaderDataBlock   \
    { \
        SDB_FIELD_##name(SDB_FIELD_Cpp_Source); \
    \
        static constexpr const char struct_name[]=#name; \
        static constexpr const char glsl_source_code[]=UBO_FIELD_##name(SDB_FIELD_GLSL_Source); \
    \
    public: \
    \
        virtual const char *    GetName()const{return struct_name;}    \
        virtual const char *    GetSource()const{return glsl_source_code;}    \
        virtual const size_t    GetSourceLength()const{return sizeof(glsl_source_code);} \
    \
        virtual const DescriptorSetType GetDescSetType()const{return DescriptorSetType::dst};\
    };

    #define UBO_DEFINE(name,vn,dst)     SDB_DEFINE(UBO,name,vn,dst)
    #define SSBO_DEFINE(name,vn,dst)    SDB_DEFINE(SSBO,name,vn,dst)

    /*//////////////////////////////////////////////////////////////////////////////////////////////////

    C++中定义UBO方法：

    #define SDB_FIELD_ViewportInfo(F) \
            F(Matrix4,ortho_matrix)            \
            F(Float2,canvas_resolution)        \
            F(Float2,viewport_resolution)      \
            F(Float2,inv_viewport_resolution)

    UBO_DEFINE(ViewportInfo,viewport,RenderTarget)    //

    该结构定义后，在C++中展开为：

    class UBOViewportInfo :public ShaderDataBlock
    {
        Matrix4 ortho_matrix;
        Float2  canvas_resolution;
        Float2  viewport_resolution;
        Float2  inv_viewport_resolution;

        static constexpr const char struct_name[]="Viewport";
        static constexpr const char glsl_source_code[] = "  Matrix4 ortho_matrix;\n"
                                                        "  Float2 canvas_resolution;\n"
                                                        "  Float2 viewport_resolution;\n"
                                                        "  Float2 inv_viewport_resolution;\n";

    public:

        virtual const char* GetName()const {return struct_name;}
        virtual const char* GetSource()const {return glsl_source_code;}
        virtual const size_t GetSourceLength()const {return sizeof(glsl_source_code);}

        virtual const DescriptorSetType GetDescSetType()const{return DescriptorSetType::RenderTarget};
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////*/
}//namespace hgl::graph
