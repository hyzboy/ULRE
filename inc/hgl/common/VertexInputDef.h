#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/common/InterpolationDef.h>
#include <hgl/type/ValueArray.h>
#include <hgl/type/String.h>
#include <compare>

namespace hgl::graph
{
    const uint GetShaderCountByBits(const uint32_t bits);
    const uint GetMaxShaderStage(const uint32_t bits);
    const char *GetShaderStageName(const VkShaderStageFlagBits &);
    const uint GetShaderStageFlagBits(const char *,int len=0);

#pragma pack(push,1)
    struct VertexInputAttribute
    {
        VertexSemantic semantic;
        char    name[VERTEX_ATTRIB_NAME_MAX_LENGTH];
        uint8   location;

        uint8   basetype;
        uint8   vec_size;

        Interpolation       interpolation;
    };
#pragma pack(pop)

    using VIA=VertexInputAttribute;

    using VIAList=ValueArray<VIA>;

    inline bool operator==(const VertexInputAttribute& lhs, const VertexInputAttribute& rhs) {
        return lhs.semantic == rhs.semantic &&
               lhs.location == rhs.location &&
               lhs.basetype == rhs.basetype &&
               lhs.vec_size == rhs.vec_size &&
               lhs.interpolation == rhs.interpolation;
    }

    inline const AnsiString GetShaderAttributeTypename(const VertexInputAttribute *ss)
    {
        return AnsiString(GetVertexAttribName((VABaseType)ss->basetype,ss->vec_size));
    }

    const VkFormat GetVulkanFormat(const VertexInputAttribute *sa);

    struct VertexInputAttributeArray
    {
        uint count;
        VIA *items;

    public:

        VertexInputAttributeArray()
        {
            count=0;
            items=nullptr;
        }

        VertexInputAttributeArray(const VertexInputAttributeArray &viaa)
        {
            count=0;
            items=nullptr;

            Clone(&viaa);
        }

        ~VertexInputAttributeArray()
        {
            Clear();
        }

        std::strong_ordering operator<=>(const VertexInputAttributeArray &saa) const
        {
            if(auto cmp = count <=> saa.count; cmp != 0)
                return cmp;

            for(uint i = 0; i < count; i++)
            {
                if(auto cmp = items[i].location <=> saa.items[i].location; cmp != 0)
                    return cmp;

                if(auto cmp = items[i].basetype <=> saa.items[i].basetype; cmp != 0)
                    return cmp;

                if(auto cmp = items[i].vec_size <=> saa.items[i].vec_size; cmp != 0)
                    return cmp;

                if(auto cmp = ::hgl::strcmp_ordering(items[i].name, saa.items[i].name); cmp != 0)
                    return cmp;
            }

            return std::strong_ordering::equal;
        }

        bool operator==(const VertexInputAttributeArray &saa) const = default;

        bool Init(const uint c=0)
        {
            if(items)
                return(false);

            if(c>0)
            {
                count=c;
                items=array_alloc<VertexInputAttribute>(count);
            }
            else
            {
                count=0;
                items=nullptr;
            }

            return(true);
        }

        bool Contains(const VertexSemantic semantic)const
        {
            if(count<=0)
                return(false);

            for(uint i=0;i<count;i++)
                if(items[i].semantic==semantic)
                    return(true);

            return(false);
        }

        bool Contains(const char *name)const
        {
            return Contains(GetVertexSemanticByName(name));
        }

        bool Add(VIA &via)
        {
            if(via.semantic==VertexSemantic::Unknown && *via.name)
                via.semantic=GetVertexSemanticByName(via.name);

            if(via.semantic!=VertexSemantic::Unknown)
                hgl::strcpy(via.name,sizeof(via.name),GetVertexSemanticName(via.semantic));

            if(Contains(via.semantic))
                return(false);

            via.location=count;

            if(!items)
            {
                items=array_alloc<VertexInputAttribute>(1);
                count=1;
            }
            else
            {
                ++count;
                items=array_realloc(items,count);
            }

            mem_copy(items[count-1],via);
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

        bool Clone(const VertexInputAttributeArray *src)
        {
            if(!src)
                return(false);

            if(!Init(src->count))
                return(false);

            mem_copy(items,src->items,src->count);
            return(true);
        }
    };

    using VIAArray=VertexInputAttributeArray;
}

