#pragma once

#include<hgl/type/EnumUtil.h>
#include<compare>

namespace hgl::graph
{
    #pragma pack(push,1)
    enum class VertexInputGroup:uint8
    {
        Basic,

        TransformID,
        MaterialInstanceID,

        JointID,
        JointWeight,

        ENUM_CLASS_RANGE(Basic,JointWeight)
    };

    constexpr const char *VertexInputGroupName[]=
    {
        "Basic",

        "TransformID",
        "MaterialInstanceID",

        "JointID",
        "JointWeight"
    };

    inline const char *GetVertexInputGroupName(const VertexInputGroup vig)
    {
        RANGE_CHECK_RETURN_NULLPTR(vig);

        return VertexInputGroupName[(int)vig];
    }

    enum class VertexAttribBaseType:uint8
    {
        Bool=0,
        Int,
        UInt,
        Float,
        Double,

        ENUM_CLASS_RANGE(Bool,Double)
    };//enum class VertexAttribBaseType

    using VABaseType=VertexAttribBaseType;

    struct VertexAttribType
    {
        union
        {
            struct
            {
                VertexAttribBaseType basetype:4;
                uint8 vec_size:4;
            };

            uint8 vat_code;
        };

    public:

        const bool Check()const
        {
            if(basetype<VertexAttribBaseType::Bool
                ||basetype>VertexAttribBaseType::Double)return(false);

            if(vec_size<=0||vec_size>4)return(false);

            return(true);
        }

        std::strong_ordering operator<=>(const VertexAttribType &vat)const
        {
            if(auto cmp = basetype <=> vat.basetype; cmp != 0)
                return cmp;
            
            return vec_size <=> vat.vec_size;
        }

        bool operator==(const VertexAttribType &vat)const = default;

        const uint8 ToCode()const{return vat_code;}

        const bool FromCode(const uint8 code)
        {
            vat_code=code;

            return Check();
        }
    };//struct VertexAttribType
    #pragma pack(pop)

    using VAType=VertexAttribType;

    /**
        * 根据字符串解晰顶点属性类型
        */
    bool ParseVertexAttribType(VAType *,const char *);

    const char *GetVertexAttribName(const VABaseType &base_type,const uint vec_size);
    const char *GetVertexAttribName(const VAType *type);

    constexpr const VAType VAT_BOOL ={VABaseType::Bool,1};
    constexpr const VAType VAT_BVEC2={VABaseType::Bool,2};
    constexpr const VAType VAT_BVEC3={VABaseType::Bool,3};
    constexpr const VAType VAT_BVEC4={VABaseType::Bool,4};

    constexpr const VAType VAT_INT  ={VABaseType::Int,1};
    constexpr const VAType VAT_IVEC2={VABaseType::Int,2};
    constexpr const VAType VAT_IVEC3={VABaseType::Int,3};
    constexpr const VAType VAT_IVEC4={VABaseType::Int,4};

    constexpr const VAType VAT_UINT ={VABaseType::UInt,1};
    constexpr const VAType VAT_UVEC2={VABaseType::UInt,2};
    constexpr const VAType VAT_UVEC3={VABaseType::UInt,3};
    constexpr const VAType VAT_UVEC4={VABaseType::UInt,4};

    constexpr const VAType VAT_FLOAT={VABaseType::Float,1};
    constexpr const VAType VAT_VEC2 ={VABaseType::Float,2};
    constexpr const VAType VAT_VEC3 ={VABaseType::Float,3};
    constexpr const VAType VAT_VEC4 ={VABaseType::Float,4};

    constexpr const VAType VAT_DOUBLE={VABaseType::Double,1};
    constexpr const VAType VAT_DVEC2 ={VABaseType::Double,2};
    constexpr const VAType VAT_DVEC3 ={VABaseType::Double,3};
    constexpr const VAType VAT_DVEC4 ={VABaseType::Double,4};

    constexpr const size_t VERTEX_ATTRIB_NAME_MAX_LENGTH=32;

    /**
        * 预定义一些顶点属性名称，可用可不用。但一般默认shader会使用这些名称
        */
    namespace VertexAttribName
    {
    #define VAN_DEFINE(name)    constexpr const char name[]=#name;
        VAN_DEFINE(Position)

        VAN_DEFINE(Normal)
        VAN_DEFINE(Tangent)
        VAN_DEFINE(Bitangent)
        VAN_DEFINE(Color)
        VAN_DEFINE(Luminance)
        VAN_DEFINE(TexCoord)

        VAN_DEFINE(AO)

        VAN_DEFINE(Size)
        VAN_DEFINE(Rotation)

        VAN_DEFINE(Assign)

        VAN_DEFINE(JointID)
        VAN_DEFINE(JointWeight)
    #undef VAN_DEFINE
    }//namespace VertexAttribName

    #define VAN VertexAttribName
}//namespace hgl::graph
