#pragma once

#include<hgl/type/String.h>
#include<hgl/type/ArrayList.h>
#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VKInterpolation.h>
#include<hgl/graph/VKSamplerType.h>
#include<hgl/graph/VKImageType.h>
#include<hgl/Comparator.h>

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

struct ShaderVariableType:public Comparator<ShaderVariableType>
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

    ShaderVariableType()
    {
        svt_code=0;
    }

    ShaderVariableType(const ShaderVariableType &svt)
    {
        svt_code=svt.svt_code;
    }

    ShaderVariableType(const VAType &vat,const uint count)
    {
        From(vat,count);
    }

    ShaderVariableType(const VABaseType &vabt,const uint32 count)
    {
        svt_code=0;

        base_type=SVBaseType::Scalar;

        scalar.type=vabt;

        array_size=count;
    }

    ShaderVariableType(const VABaseType &vabt,const uint8 size,const uint32 count)
    {
        svt_code=0;

        base_type=SVBaseType::Vector;

        vector.type=vabt;
        vector.vec_size=size;

        array_size=count;
    }

    ShaderVariableType(const VABaseType &vabt,const uint8 row,const uint8 col,const uint32 count)
    {
        svt_code=0;

        base_type=SVBaseType::Matrix;

        matrix.type=vabt;
        matrix.n=col;
        matrix.m=row;

        array_size=count;
    }

    ShaderVariableType(const SamplerType &st,const uint32 count)
    {
        svt_code=0;

        base_type=SVBaseType::Sampler;

        sampler.type=st;

        array_size=count;
    }

    ShaderVariableType(const ShaderImageType &sit,const uint32 count)
    {
        svt_code=0;

        base_type=SVBaseType::Image;

        image.type=sit;

        array_size=count;
    }

    const bool Check()const;

    const int compare(const ShaderVariableType &svt)const override;

    const char *GetTypename()const;

    bool ParseTypeString(const char *str);

    const uint64 ToCode()const{return svt_code;}
    const bool FromCode(const uint64 code)
    {
        svt_code=code;

        return Check();
    }

    const bool From(const VAType &vat,const uint16 count=1);

    static const ShaderVariableType Scalar(const VABaseType &vabt,const uint count=1)
    {
        ShaderVariableType svt;

        svt.base_type   =SVBaseType::Scalar;
        svt.scalar.type =vabt;
        svt.array_size  =count;

        return svt;
    }
};//struct ShaderVariableType
#pragma pack(pop)

using SVType=ShaderVariableType;

const SVType SVT_BOOL  (VABaseType::Bool,    1);
const SVType SVT_INT   (VABaseType::Int,     1);
const SVType SVT_UINT  (VABaseType::UInt,    1);
const SVType SVT_FLOAT (VABaseType::Float,   1);
const SVType SVT_DOUBLE(VABaseType::Double,  1);

const SVType SVT_BVEC2 (VABaseType::Bool,    2,1);
const SVType SVT_BVEC3 (VABaseType::Bool,    3,1);
const SVType SVT_BVEC4 (VABaseType::Bool,    4,1);

const SVType SVT_IVEC2 (VABaseType::Int,     2,1);
const SVType SVT_IVEC3 (VABaseType::Int,     3,1);
const SVType SVT_IVEC4 (VABaseType::Int,     4,1);

const SVType SVT_UVEC2 (VABaseType::UInt,    2,1);
const SVType SVT_UVEC3 (VABaseType::UInt,    3,1);
const SVType SVT_UVEC4 (VABaseType::UInt,    4,1);

const SVType SVT_VEC2  (VABaseType::Float,   2,1);
const SVType SVT_VEC3  (VABaseType::Float,   3,1);
const SVType SVT_VEC4  (VABaseType::Float,   4,1);

const SVType SVT_DVEC2 (VABaseType::Double,  2,1);
const SVType SVT_DVEC3 (VABaseType::Double,  3,1);
const SVType SVT_DVEC4 (VABaseType::Double,  4,1);

const SVType SVT_MAT2  (VABaseType::Float,   2,2,1);
const SVType SVT_MAT3  (VABaseType::Float,   3,3,1);
const SVType SVT_MAT4  (VABaseType::Float,   4,4,1);
const SVType SVT_MAT2x3(VABaseType::Float,   2,3,1);
const SVType SVT_MAT2x4(VABaseType::Float,   2,4,1);
const SVType SVT_MAT3x2(VABaseType::Float,   3,2,1);
const SVType SVT_MAT3x4(VABaseType::Float,   3,4,1);
const SVType SVT_MAT4x2(VABaseType::Float,   4,2,1);
const SVType SVT_MAT4x3(VABaseType::Float,   4,3,1);

const SVType SVT_Sampler1D(SamplerType::Sampler1D,          1);
const SVType SVT_Sampler2D(SamplerType::Sampler2D,          1);
const SVType SVT_Sampler3D(SamplerType::Sampler3D,          1);

const SVType SVT_SamplerCube(SamplerType::SamplerCube,      1);
const SVType SVT_Sampler2DRect(SamplerType::Sampler2DRect,  1);
        
const SVType SVT_Sampler1DArray(SamplerType::Sampler1DArray,1);
const SVType SVT_Sampler2DArray(SamplerType::Sampler2DArray,1);

const SVType SVT_SamplerCubeArray(SamplerType::SamplerCubeArray,1);

const SVType SVT_SamplerBuffer(SamplerType::SamplerBuffer,1);

const SVType SVT_Sampler2DMS(SamplerType::Sampler2DMS,1);
const SVType SVT_Sampler2DMSArray(SamplerType::Sampler2DMSArray,1);

const SVType SVT_Sampler1DShadow(SamplerType::Sampler1DShadow,1);
const SVType SVT_Sampler2DShadow(SamplerType::Sampler2DShadow,1);

const SVType SVT_SamplerCubeShadow(SamplerType::SamplerCubeShadow,1);
const SVType SVT_Sampler2DRectShadow(SamplerType::Sampler2DRectShadow,1);

const SVType SVT_Sampler1DArrayShadow(SamplerType::Sampler1DArrayShadow,1);
const SVType SVT_Sampler2DArrayShadow(SamplerType::Sampler2DArrayShadow,1);
const SVType SVT_SamplerCubeArrayShadow(SamplerType::SamplerCubeArrayShadow,1);

const SVType SVT_Image1D(ShaderImageType::Image1D,1);
const SVType SVT_Image2D(ShaderImageType::Image2D,1);
const SVType SVT_Image3D(ShaderImageType::Image3D,1);

const SVType SVT_ImageCube(ShaderImageType::ImageCube,1);
const SVType SVT_Image2DRect(ShaderImageType::Image2DRect,1);

const SVType SVT_Image1DArray(ShaderImageType::Image1DArray,1);
const SVType SVT_Image2DArray(ShaderImageType::Image2DArray,1);

const SVType SVT_ImageCubeArray(ShaderImageType::ImageCubeArray,1);

const SVType SVT_ImageBuffer(ShaderImageType::ImageBuffer,1);

const SVType SVT_Image2DMS(ShaderImageType::Image2DMS,1);
const SVType SVT_Image2DMSArray(ShaderImageType::Image2DMSArray,1);

struct ShaderVariable
{
    char            name[VERTEX_ATTRIB_NAME_MAX_LENGTH];
    uint            location;
    SVType          type;
    Interpolation   interpolation;  //插值方式
};

using SVList=ArrayList<ShaderVariable>;

struct ShaderVariableArray:public Comparator<ShaderVariableArray>
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

    const int compare(const ShaderVariableArray &sva)const override
    {
        int off=count-sva.count;
        if(off)return off;

        for(uint i=0;i<count;i++)
        {
            off=items[i].location-sva.items[i].location;
            if(off)
                return off;

            if(items[i].type.ToCode()>sva.items[i].type.ToCode())
                return 1;

            //ToCode返回的是uint64，可能差值超大，所以不能直接用-的结果

            if(items[i].type.ToCode()<sva.items[i].type.ToCode())
                return -1;

            off=int(items[i].interpolation)-int(sva.items[i].interpolation);
            if(off)
                return off;

            off=hgl::strcmp(items[i].name,sva.items[i].name);
            if(off)
                return off;
        }

        return 0;
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
