#pragma once

#include <hgl/type/EnumUtil.h>
#include <compare>
#include <cstring>

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
    };

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

        bool operator==(const VertexAttribType &vat)const{return vat_code==vat.vat_code;}

        const uint8 ToCode()const{return vat_code;}

        const bool FromCode(const uint8 code)
        {
            vat_code=code;

            return Check();
        }
    };
#pragma pack(pop)

    using VAType=VertexAttribType;

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

    enum class VertexAttrib:uint8
    {
        Position=0,

        Normal,
        Tangent,
        Bitangent,
        Color,
        Luminance,
        TexCoord,

        AO,

        Size,
        Rotation,

        TransformID,
        MaterialInstanceID,

        JointID,
        JointWeight,

        ENUM_CLASS_RANGE(Position,JointWeight)
    };

    constexpr const char *VertexAttribNames[]=
    {
        "Position",

        "Normal",
        "Tangent",
        "Bitangent",
        "Color",
        "Luminance",
        "TexCoord",

        "AO",

        "Size",
        "Rotation",

        "TransformID",
        "MaterialInstanceID",

        "JointID",
        "JointWeight"
    };

    inline const char *GetVertexAttribName(const VertexAttrib va)
    {
        RANGE_CHECK_RETURN_NULLPTR(va);

        return VertexAttribNames[(int)va];
    }

    // VAN is a convenience alias for VertexAttrib
#define VAN VertexAttrib

    /**
     * 根据名称查找VertexAttrib枚举值
     * @return VertexAttrib枚举值，如果找不到返回RANGE_SIZE (无效值)
     */
    inline VertexAttrib GetVertexAttribByName(const char *name)
    {
        if(!name)return VAN::RANGE_SIZE;

        for(int i=0;i<(int)VAN::RANGE_SIZE;i++)
            if(strcmp(VertexAttribNames[i],name)==0)
                return VertexAttrib(i);

        return VAN::RANGE_SIZE;
    }
}
