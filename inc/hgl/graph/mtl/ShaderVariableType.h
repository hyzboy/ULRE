#pragma once

#include<hgl/type/String.h>
#include<hgl/type/ArrayList.h>
#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VKInterpolation.h>
#include<hgl/graph/VKSamplerType.h>
#include<hgl/graph/VKImageType.h>

VK_NAMESPACE_BEGIN
enum class ShaderVariableBaseType:uint8
{
    Scalar=0,
    Vector,
    Matrix,
    Sampler,
    Image,
    AtomicCounter,

    ENUM_CLASS_RANGE(Scalar,AtomicCounter)
};//enum class ShaderVariableBaseType

using SVBaseType=ShaderVariableBaseType;
        
#pragma pack(push,1)

struct ShaderVariableType
{
    union
    {
        struct
        {
            union
            {
                struct
                {
                    SVBaseType base_type;

                    union
                    {
                        struct Scalar
                        {
                            VABaseType type;
                        }scalar;

                        struct Vector
                        {
                            VABaseType type;
                            uint8 vec_size;
                        }vector;

                        struct Matrix
                        {
                            VABaseType type;
                            uint8 n;
                            uint8 m;
                        }matrix;

                        struct Sampler
                        {
                            SamplerType type;
                        }sampler;

                        struct Image
                        {
                            ShaderImageType type;
                        }image;
                    };
                };

                uint32 type_code;
            };

            uint32 array_size;
        };

        uint64 svt_code;
    };

public:

    // ✅ 使用默认构造函数，使类型成为 trivial
    constexpr ShaderVariableType() : svt_code(0) {}
    
    // ✅ 使用默认拷贝构造和赋值
    constexpr ShaderVariableType(const ShaderVariableType &) = default;
    constexpr ShaderVariableType& operator=(const ShaderVariableType &) = default;

    const bool Check() const;

    std::strong_ordering operator<=>(const ShaderVariableType &svt) const;

    const char *GetTypename() const;

    bool ParseTypeString(const char *str);

    constexpr uint64 ToCode() const { return svt_code; }
    bool FromCode(const uint64 code)
    {
        svt_code = code;
        return Check();
    }

    const bool From(const VAType &vat, const uint16 count = 1);
};//struct ShaderVariableType
#pragma pack(pop)

using SVType=ShaderVariableType;

// ✅ 工厂函数命名空间
namespace SVTypeFactory
{
    constexpr ShaderVariableType Scalar(VABaseType type, uint32 count = 1)
    {
        ShaderVariableType svt{};
        svt.base_type = SVBaseType::Scalar;
        svt.scalar.type = type;
        svt.array_size = count;
        return svt;
    }
    
    constexpr ShaderVariableType Vector(VABaseType type, uint8 size, uint32 count = 1)
    {
        ShaderVariableType svt{};
        svt.base_type = SVBaseType::Vector;
        svt.vector.type = type;
        svt.vector.vec_size = size;
        svt.array_size = count;
        return svt;
    }
    
    constexpr ShaderVariableType Matrix(VABaseType type, uint8 rows, uint8 cols, uint32 count = 1)
    {
        ShaderVariableType svt{};
        svt.base_type = SVBaseType::Matrix;
        svt.matrix.type = type;
        svt.matrix.m = rows;
        svt.matrix.n = cols;
        svt.array_size = count;
        return svt;
    }
    
    constexpr ShaderVariableType Sampler(SamplerType type, uint32 count = 1)
    {
        ShaderVariableType svt{};
        svt.base_type = SVBaseType::Sampler;
        svt.sampler.type = type;
        svt.array_size = count;
        return svt;
    }
    
    constexpr ShaderVariableType Image(ShaderImageType type, uint32 count = 1)
    {
        ShaderVariableType svt{};
        svt.base_type = SVBaseType::Image;
        svt.image.type = type;
        svt.array_size = count;
        return svt;
    }
}

// ✅ 使用 inline constexpr 定义常量（C++17 特性）
inline constexpr SVType SVT_BOOL   = SVTypeFactory::Scalar(VABaseType::Bool,   1);
inline constexpr SVType SVT_INT    = SVTypeFactory::Scalar(VABaseType::Int,    1);
inline constexpr SVType SVT_UINT   = SVTypeFactory::Scalar(VABaseType::UInt,   1);
inline constexpr SVType SVT_FLOAT  = SVTypeFactory::Scalar(VABaseType::Float,  1);
inline constexpr SVType SVT_DOUBLE = SVTypeFactory::Scalar(VABaseType::Double, 1);

inline constexpr SVType SVT_BVEC2  = SVTypeFactory::Vector(VABaseType::Bool,   2, 1);
inline constexpr SVType SVT_BVEC3  = SVTypeFactory::Vector(VABaseType::Bool,   3, 1);
inline constexpr SVType SVT_BVEC4  = SVTypeFactory::Vector(VABaseType::Bool,   4, 1);

inline constexpr SVType SVT_IVEC2  = SVTypeFactory::Vector(VABaseType::Int,    2, 1);
inline constexpr SVType SVT_IVEC3  = SVTypeFactory::Vector(VABaseType::Int,    3, 1);
inline constexpr SVType SVT_IVEC4  = SVTypeFactory::Vector(VABaseType::Int,    4, 1);

inline constexpr SVType SVT_UVEC2  = SVTypeFactory::Vector(VABaseType::UInt,   2, 1);
inline constexpr SVType SVT_UVEC3  = SVTypeFactory::Vector(VABaseType::UInt,   3, 1);
inline constexpr SVType SVT_UVEC4  = SVTypeFactory::Vector(VABaseType::UInt,   4, 1);

inline constexpr SVType SVT_VEC2   = SVTypeFactory::Vector(VABaseType::Float,  2, 1);
inline constexpr SVType SVT_VEC3   = SVTypeFactory::Vector(VABaseType::Float,  3, 1);
inline constexpr SVType SVT_VEC4   = SVTypeFactory::Vector(VABaseType::Float,  4, 1);

inline constexpr SVType SVT_DVEC2  = SVTypeFactory::Vector(VABaseType::Double, 2, 1);
inline constexpr SVType SVT_DVEC3  = SVTypeFactory::Vector(VABaseType::Double, 3, 1);
inline constexpr SVType SVT_DVEC4  = SVTypeFactory::Vector(VABaseType::Double, 4, 1);

inline constexpr SVType SVT_MAT2   = SVTypeFactory::Matrix(VABaseType::Float,  2, 2, 1);
inline constexpr SVType SVT_MAT3   = SVTypeFactory::Matrix(VABaseType::Float,  3, 3, 1);
inline constexpr SVType SVT_MAT4   = SVTypeFactory::Matrix(VABaseType::Float,  4, 4, 1);
inline constexpr SVType SVT_MAT2x3 = SVTypeFactory::Matrix(VABaseType::Float,  2, 3, 1);
inline constexpr SVType SVT_MAT2x4 = SVTypeFactory::Matrix(VABaseType::Float,  2, 4, 1);
inline constexpr SVType SVT_MAT3x2 = SVTypeFactory::Matrix(VABaseType::Float,  3, 2, 1);
inline constexpr SVType SVT_MAT3x4 = SVTypeFactory::Matrix(VABaseType::Float,  3, 4, 1);
inline constexpr SVType SVT_MAT4x2 = SVTypeFactory::Matrix(VABaseType::Float,  4, 2, 1);
inline constexpr SVType SVT_MAT4x3 = SVTypeFactory::Matrix(VABaseType::Float,  4, 3, 1);

inline constexpr SVType SVT_Sampler1D          = SVTypeFactory::Sampler(SamplerType::Sampler1D,          1);
inline constexpr SVType SVT_Sampler2D          = SVTypeFactory::Sampler(SamplerType::Sampler2D,          1);
inline constexpr SVType SVT_Sampler3D          = SVTypeFactory::Sampler(SamplerType::Sampler3D,          1);

inline constexpr SVType SVT_SamplerCube        = SVTypeFactory::Sampler(SamplerType::SamplerCube,        1);
inline constexpr SVType SVT_Sampler2DRect      = SVTypeFactory::Sampler(SamplerType::Sampler2DRect,      1);
        
inline constexpr SVType SVT_Sampler1DArray     = SVTypeFactory::Sampler(SamplerType::Sampler1DArray,     1);
inline constexpr SVType SVT_Sampler2DArray     = SVTypeFactory::Sampler(SamplerType::Sampler2DArray,     1);

inline constexpr SVType SVT_SamplerCubeArray   = SVTypeFactory::Sampler(SamplerType::SamplerCubeArray,   1);

inline constexpr SVType SVT_SamplerBuffer      = SVTypeFactory::Sampler(SamplerType::SamplerBuffer,      1);

inline constexpr SVType SVT_Sampler2DMS        = SVTypeFactory::Sampler(SamplerType::Sampler2DMS,        1);
inline constexpr SVType SVT_Sampler2DMSArray   = SVTypeFactory::Sampler(SamplerType::Sampler2DMSArray,   1);

inline constexpr SVType SVT_Sampler1DShadow         = SVTypeFactory::Sampler(SamplerType::Sampler1DShadow,         1);
inline constexpr SVType SVT_Sampler2DShadow         = SVTypeFactory::Sampler(SamplerType::Sampler2DShadow,         1);

inline constexpr SVType SVT_SamplerCubeShadow       = SVTypeFactory::Sampler(SamplerType::SamplerCubeShadow,       1);
inline constexpr SVType SVT_Sampler2DRectShadow     = SVTypeFactory::Sampler(SamplerType::Sampler2DRectShadow,     1);

inline constexpr SVType SVT_Sampler1DArrayShadow    = SVTypeFactory::Sampler(SamplerType::Sampler1DArrayShadow,    1);
inline constexpr SVType SVT_Sampler2DArrayShadow    = SVTypeFactory::Sampler(SamplerType::Sampler2DArrayShadow,    1);
inline constexpr SVType SVT_SamplerCubeArrayShadow  = SVTypeFactory::Sampler(SamplerType::SamplerCubeArrayShadow,  1);

inline constexpr SVType SVT_Image1D            = SVTypeFactory::Image(ShaderImageType::Image1D,            1);
inline constexpr SVType SVT_Image2D            = SVTypeFactory::Image(ShaderImageType::Image2D,            1);
inline constexpr SVType SVT_Image3D            = SVTypeFactory::Image(ShaderImageType::Image3D,            1);

inline constexpr SVType SVT_ImageCube          = SVTypeFactory::Image(ShaderImageType::ImageCube,          1);
inline constexpr SVType SVT_Image2DRect        = SVTypeFactory::Image(ShaderImageType::Image2DRect,        1);

inline constexpr SVType SVT_Image1DArray       = SVTypeFactory::Image(ShaderImageType::Image1DArray,       1);
inline constexpr SVType SVT_Image2DArray       = SVTypeFactory::Image(ShaderImageType::Image2DArray,       1);

inline constexpr SVType SVT_ImageCubeArray     = SVTypeFactory::Image(ShaderImageType::ImageCubeArray,     1);

inline constexpr SVType SVT_ImageBuffer        = SVTypeFactory::Image(ShaderImageType::ImageBuffer,        1);

inline constexpr SVType SVT_Image2DMS          = SVTypeFactory::Image(ShaderImageType::Image2DMS,          1);
inline constexpr SVType SVT_Image2DMSArray     = SVTypeFactory::Image(ShaderImageType::Image2DMSArray,     1);

struct ShaderVariable
{
    char            name[VERTEX_ATTRIB_NAME_MAX_LENGTH];
    uint            location;
    SVType          type;
    Interpolation   interpolation;  //插值方式
};

using SVList=ArrayList<ShaderVariable>;

struct ShaderVariableArray
{
    uint count;

    ShaderVariable *items;

public:

    const bool IsEmpty()const{return !items||count<=0;}

public:

    ShaderVariableArray()
    {
        count=0;
        items=nullptr;
    }

    ~ShaderVariableArray()
    {
        Clear();
    }

    std::strong_ordering operator<=>(const ShaderVariableArray &sva)const
    {
        if(auto cmp = count <=> sva.count; cmp != 0)
            return cmp;

        for(uint i=0;i<count;i++)
        {
            if(auto cmp = items[i].location <=> sva.items[i].location; cmp != 0)
                return cmp;

            const uint64 lhs_code = items[i].type.ToCode();
            const uint64 rhs_code = sva.items[i].type.ToCode();

            if(auto cmp = lhs_code <=> rhs_code; cmp != 0)
                return cmp;

            if(auto cmp = items[i].interpolation <=> sva.items[i].interpolation; cmp != 0)
                return cmp;

            if(auto cmp = hgl::strcmp_ordering(items[i].name, sva.items[i].name); cmp != 0)
                return cmp;
        }

        return std::strong_ordering::equal;
    }
                
    bool Init(const uint c=0)
    {
        if(items)
            return(false);

        if(c>0)
        {
            count=c;
            items=array_alloc<ShaderVariable>(count);
        }
        else
        {
            count=0;
            items=nullptr;
        }

        return(true);
    }

    bool Contains(const char *name)const
    {                
        if(count<=0)
            return(false);

        for(uint i=0;i<count;i++)
            if(hgl::strcmp(items[i].name,name)==0)
                return(true);

        return(false);
    }

    bool Add(ShaderVariable &sv)
    {
        if(Contains(sv.name))
            return(false);

        sv.location=count;

        if(!items)
        {
            items=array_alloc<ShaderVariable>(1);
            count=1;
        }
        else
        {
            ++count;
            items=array_realloc(items,count);
        }
                
        mem_copy(items[count-1],sv);
        return(true);
    }

    void Clear()
    {
        if(items)
        {
            array_free(items);
            items=nullptr;
        }

        count=0;
    }

    bool Clone(const ShaderVariableArray *src)
    {
        if(!src)
            return(false);

        if(!Init(src->count))
            return(false);

        mem_copy(items,src->items,src->count);
        return(true);
    }
            
    void ToString(AnsiString &output_string)
    {
        if(IsEmpty())
            return;

        const ShaderVariable *sv=items;

        for(uint i=0;i<count;i++)
        {
            output_string+="    ";

            if(sv->interpolation!=Interpolation::Smooth)
            {
                if(RangeCheck(sv->interpolation))
                {
                    output_string+=InterpolationName[size_t(sv->interpolation)];
                    output_string+=" ";
                }
            }

            output_string+=sv->type.GetTypename();
            output_string+=" ";
            output_string+=sv->name;
            output_string+=";\n";

            ++sv;
        }
    }
};//struct ShaderVariableArray

using SVArray=ShaderVariableArray;

VK_NAMESPACE_END
