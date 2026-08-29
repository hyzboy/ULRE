#pragma once

#include <hgl/type/EnumUtil.h>
#include <compare>
#include <cstring>

namespace hgl::graph
{
#pragma pack(push,1)
    enum class VertexSemantic:uint8
    {
        Unknown=0,
        Position,
        Normal,
        Tangent,
        Bitangent,
        Color,
        Luminance,
        TexCoord,
        AO,
        Size,
        Rotation,
        Assign,
        TransformID,
        DataIndexID,

        ENUM_CLASS_RANGE(Unknown,DataIndexID)
    };

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

    inline const char *GetVertexSemanticName(const VertexSemantic semantic)
    {
        switch(semantic)
        {
            case VertexSemantic::Position:         return "Position";
            case VertexSemantic::Normal:           return "Normal";
            case VertexSemantic::Tangent:          return "Tangent";
            case VertexSemantic::Bitangent:        return "Bitangent";
            case VertexSemantic::Color:            return "Color";
            case VertexSemantic::Luminance:        return "Luminance";
            case VertexSemantic::TexCoord:         return "TexCoord";
            case VertexSemantic::AO:               return "AO";
            case VertexSemantic::Size:             return "Size";
            case VertexSemantic::Rotation:         return "Rotation";
            case VertexSemantic::Assign:           return "Assign";
            case VertexSemantic::TransformID:      return "TransformID";
            case VertexSemantic::DataIndexID:      return "DataIndexID";
            default:                               return "Unknown";
        }
    }

    inline VertexSemantic GetVertexSemanticByName(const char *name)
    {
        if(!name||!*name)return VertexSemantic::Unknown;

        if(std::strcmp(name,"Position")==0)       return VertexSemantic::Position;
        if(std::strcmp(name,"Normal")==0)         return VertexSemantic::Normal;
        if(std::strcmp(name,"Tangent")==0)        return VertexSemantic::Tangent;
        if(std::strcmp(name,"Bitangent")==0)      return VertexSemantic::Bitangent;
        if(std::strcmp(name,"Color")==0)          return VertexSemantic::Color;
        if(std::strcmp(name,"Luminance")==0)      return VertexSemantic::Luminance;
        if(std::strcmp(name,"TexCoord")==0)       return VertexSemantic::TexCoord;
        if(std::strcmp(name,"AO")==0)             return VertexSemantic::AO;
        if(std::strcmp(name,"Size")==0)           return VertexSemantic::Size;
        if(std::strcmp(name,"Rotation")==0)       return VertexSemantic::Rotation;
        if(std::strcmp(name,"Assign")==0)         return VertexSemantic::Assign;
        if(std::strcmp(name,"TransformID")==0)    return VertexSemantic::TransformID;
        if(std::strcmp(name,"DataIndexID")==0)    return VertexSemantic::DataIndexID;

        return VertexSemantic::Unknown;
    }

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

    namespace VAN
    {
        constexpr VertexSemantic Position       = VertexSemantic::Position;
        constexpr VertexSemantic Normal         = VertexSemantic::Normal;
        constexpr VertexSemantic Tangent        = VertexSemantic::Tangent;
        constexpr VertexSemantic Bitangent      = VertexSemantic::Bitangent;
        constexpr VertexSemantic Color          = VertexSemantic::Color;
        constexpr VertexSemantic Luminance      = VertexSemantic::Luminance;
        constexpr VertexSemantic TexCoord       = VertexSemantic::TexCoord;
        constexpr VertexSemantic AO             = VertexSemantic::AO;
        constexpr VertexSemantic Size           = VertexSemantic::Size;
        constexpr VertexSemantic Rotation       = VertexSemantic::Rotation;
        constexpr VertexSemantic Assign         = VertexSemantic::Assign;
        constexpr VertexSemantic TransformID    = VertexSemantic::TransformID;
        constexpr VertexSemantic DataIndexID    = VertexSemantic::DataIndexID;
    }
}
