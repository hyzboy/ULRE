#include<hgl/graph/mtl/ShaderVariableType.h>

VK_NAMESPACE_BEGIN

namespace
{
    constexpr const char *scalar_typename[]=
    {
        "bool",
        "int",
        "uint",
        "float",
        "double"
    };

    constexpr const char *vector_typename[][3]=
    {
        {"bvec2","bvec3","bvec4"},
        {"ivec2","ivec3","ivec4"},
        {"uvec2","uvec3","uvec4"},
        { "vec2", "vec3", "vec4"},
        {"dvec2","dvec3","dvec4"}
    };

    constexpr const char *mat_1_typename[][3]=
    {
        {"mat2","mat3","mat4"},
        {"dmat2","dmat3","dmat4"}
    };

    constexpr const char *mat_2_typename[][3][3]=
    {
        {
            {"mat2x2","mat2x3","mat2x4"},
            {"mat3x2","mat3x3","mat3x4"},
            {"mat4x2","mat4x3","mat4x4"}
        },
        {
            {"dmat2x2","dmat2x3","dmat2x4"},
            {"dmat3x2","dmat3x3","dmat3x4"},
            {"dmat4x2","dmat4x3","dmat4x4"}
        }
    };

    constexpr const char AtomicCounterTypename[]="atomic_uint";
}//namespace

const bool ShaderVariableType::Check()const
{
    if(!RangeCheck(base_type))return(false);

    RANGE_CHECK_RETURN_FALSE(base_type)

    if(base_type==SVBaseType::Scalar)
        return(true);

    if(base_type==SVBaseType::Vector)
    {
        if(vector.vec_size<2||vector.vec_size>4)return(false);

        return(true);
    }

    if(base_type==SVBaseType::Matrix)
    {
        if(matrix.m==0)
        {
            if(matrix.n<2||matrix.n>4)return(false);
        }
        else
        {
            if(matrix.n<2||matrix.n>4)return(false);
            if(matrix.m<2||matrix.m>4)return(false);
        }

        return(true);
    }

    if(base_type==SVBaseType::Sampler)
        return RangeCheck(sampler.type);

    if(base_type==SVBaseType::Image)
        return RangeCheck(image.type);

    return(true);
}

std::strong_ordering ShaderVariableType::operator<=>(const ShaderVariableType &svt)const
{
    int off=(int)base_type-(int)svt.base_type;

    if(off)return std::strong_ordering(off<=>0);

    if(base_type==SVBaseType::Scalar)
        return int(scalar.type)<=>int(svt.scalar.type);

    if(base_type==SVBaseType::Vector)
    {
        if(auto cmp=int(vector.type)<=>int(svt.vector.type); cmp!=0)
            return cmp;

        return vector.vec_size<=>svt.vector.vec_size;
    }

    if(base_type==SVBaseType::Matrix)
    {
        if(auto cmp=int(matrix.type)<=>int(svt.matrix.type); cmp!=0)
            return cmp;

        if(auto cmp=matrix.n<=>svt.matrix.n; cmp!=0)
            return cmp;

        return matrix.m<=>svt.matrix.m;
    }

    if(base_type==SVBaseType::Sampler)
        return int(sampler.type)<=>int(svt.sampler.type);

    if(base_type==SVBaseType::Image)
        return int(image.type)<=>int(svt.image.type);

    return std::strong_ordering::equal;
}

const char *ShaderVariableType::GetTypename()const
{
    if(base_type==SVBaseType::Scalar)
    {
        RANGE_CHECK_RETURN_NULLPTR(scalar.type)

        return scalar_typename[int(scalar.type)];
    }

    if(base_type==SVBaseType::Vector)
    {
        RANGE_CHECK_RETURN_NULLPTR(vector.type)

        if(vector.vec_size<2||vector.vec_size>4)
            return(nullptr);

        return vector_typename[int(vector.type)][vector.vec_size-2];
    }

    if(base_type==SVBaseType::Matrix)
    {
        if(matrix.type!=VABaseType::Float&&matrix.type==VABaseType::Double)
            return(nullptr);

        if(matrix.n<2||matrix.n>4)
            return(nullptr);

        if(matrix.m==0)
            return mat_1_typename[int(matrix.type)-int(VABaseType::Float)][matrix.n-2];

        if(matrix.m<2||matrix.m>4)
            return(nullptr);

        return mat_2_typename[int(matrix.type)-int(VABaseType::Float)][matrix.n-2][matrix.m-2];
    }

    if(base_type==SVBaseType::Sampler)
    {
        return GetSamplerTypeName(sampler.type);
    }

    if(base_type==SVBaseType::Image)
    {
        return GetShaderImageTypeName(image.type);
    }

    if(base_type==SVBaseType::AtomicCounter)
    {
        return AtomicCounterTypename;
    }

    return nullptr;
}

bool ShaderVariableType::ParseTypeString(const char *str)
{
    if(!str||!*str)
        return(false);

    if(str[0]=='b')
    {
        if(str[1]=='o'&&str[2]=='o'&&str[3]=='l')
        {
            base_type=SVBaseType::Scalar;
            scalar.type=VABaseType::Bool;
            return(true);
        }

        if(str[1]=='v'&&str[2]=='e'&&str[3]=='c')
        {
            base_type=SVBaseType::Vector;
            vector.type=VABaseType::Bool;
            vector.vec_size=str[4]-'0';
            return(vector.vec_size>=2&&vector.vec_size<=4);
        }

        return(false);
    }
    
    if(str[1]=='i')
    {
        if(str[2]=='n'&&str[3]=='t')
        {
            base_type=SVBaseType::Scalar;
            scalar.type=VABaseType::Int;
            return(true);
        }

        if(str[2]=='v'&&str[3]=='e'&&str[4]=='c')
        {
            base_type=SVBaseType::Vector;
            vector.type=VABaseType::Int;
            vector.vec_size=str[5]-'0';
            return(vector.vec_size>=2&&vector.vec_size<=4);
        }

        if(str[1]=='m'&&str[2]=='a'&&str[3]=='g'&&str[4]=='e')
        {
            int name_len=5;
            while(is_identifier_char(str[name_len]))
                ++name_len;

            base_type=SVBaseType::Image;
            image.type=ParseShaderImageType(str,name_len);
            return(image.type!=ShaderImageType::Error);
        }

        return(false);
    }

    if(str[1]=='u')
    {
        if(str[2]=='i'&&str[3]=='n'&&str[4]=='t')
        {
            base_type=SVBaseType::Scalar;
            scalar.type=VABaseType::UInt;
            return(true);
        }

        if(str[2]=='v'&&str[3]=='e'&&str[4]=='c')
        {
            base_type=SVBaseType::Vector;
            vector.type=VABaseType::UInt;
            vector.vec_size=str[5]-'0';
            return(vector.vec_size>=2&&vector.vec_size<=4);
        }

        return(false);
    }

    if(str[1]=='f')
    {
        if(str[2]=='l'&&str[3]=='o'&&str[4]=='a'&&str[5]=='t')
        {
            base_type=SVBaseType::Scalar;
            scalar.type=VABaseType::Float;
            return(true);
        }

        return(false);
    }

    if(str[0]=='v')
    {
        if(str[1]=='e'&&str[2]=='c')
        {
            base_type=SVBaseType::Vector;
            vector.type=VABaseType::Float;
            vector.vec_size=str[3]-'0';
            return(vector.vec_size>=2&&vector.vec_size<=4);
        }

        return(false);
    }

    if(str[1]=='d')
    {
        if(str[2]=='o'&&str[3]=='u'&&str[4]=='b'&&str[5]=='l'&&str[6]=='e')
        {
            base_type=SVBaseType::Scalar;
            scalar.type=VABaseType::Double;
            return(true);
        }

        if(str[2]=='m'&&str[3]=='a'&&str[4]=='t')
        {
            base_type=SVBaseType::Matrix;
            matrix.type=VABaseType::Double;
            matrix.n=str[5]-'0';

            if(str[6]=='x')
            {
                matrix.m=str[7]-'0';
                return(matrix.m>=2&&matrix.m<=4);
            }

            matrix.m=0;
            return(matrix.n>=2&&matrix.n<=4);
        }

        return(false);
    }

    if(str[0]=='m')
    {
        if(str[1]=='a'&&str[2]=='t')
        {
            base_type=SVBaseType::Matrix;
            matrix.type=VABaseType::Float;
            matrix.n=str[3]-'0';

            if(str[4]=='x')
            {
                matrix.m=str[5]-'0';
                return(matrix.m>=2&&matrix.m<=4);
            }

            matrix.m=0;
            return(matrix.n>=2&&matrix.n<=4);
        }

        return(false);
    }

    if(hgl::strcmp(str,"sampler",7)==0)
    {
        int name_len=7;
        while(is_identifier_char(str[name_len]))
            ++name_len;

        base_type=SVBaseType::Sampler;
        sampler.type=ParseSamplerType(str,name_len);
        return(sampler.type!=SamplerType::Error);
    }

    if(hgl::strcmp(str,"atomic_uint",11)==0)
    {
        base_type=SVBaseType::AtomicCounter;
        return(true);
    }

    return(false);
}

const bool ShaderVariableType::From(const VAType &vat,const uint16 count)
{
    array_size=count;

    if(vat.vec_size==1)
    {
        base_type=SVBaseType::Scalar;
        scalar.type=vat.basetype;

        return(true);
    }
    else if(vat.vec_size<=4)
    {
        base_type=SVBaseType::Vector;

        vector.type=vat.basetype;
        vector.vec_size=vat.vec_size;

        return(true);
    }

    return(false);
}
VK_NAMESPACE_END
