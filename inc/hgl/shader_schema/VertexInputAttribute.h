#pragma once

#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VKInterpolation.h>
#include<hgl/type/ValueArray.h>
#include<hgl/type/String.h>
#include<compare>

namespace hgl::graph
{
#pragma pack(push,1)
    struct VertexInputAttribute
    {
        //注：这个类要从GLSLCompiler动态链接库中直接传递，所以不可以使用AnsiString

        char    name[VERTEX_ATTRIB_NAME_MAX_LENGTH];
        uint8   location;

        //对应hgl/graph/VertexAttrib.h中的enum class VABaseType
        uint8   basetype;
        uint8   vec_size;

        uint8               input_rate;     //输入频率
        VertexInputGroup    group;          //分组

        Interpolation       interpolation;  //插值方式
    };//struct VertexInputAttribute
#pragma pack(pop)

    using VIA=VertexInputAttribute;

    using VIAList=ValueArray<VIA>;

    // Comparison operator for VertexInputAttribute
    inline bool operator==(const VertexInputAttribute& lhs, const VertexInputAttribute& rhs) {
        return std::strcmp(lhs.name, rhs.name) == 0 &&
               lhs.location == rhs.location &&
               lhs.basetype == rhs.basetype &&
               lhs.vec_size == rhs.vec_size &&
               lhs.input_rate == rhs.input_rate &&
               lhs.group == rhs.group &&
               lhs.interpolation == rhs.interpolation;
    }

    inline const AnsiString GetShaderAttributeTypename(const VertexInputAttribute *ss)
    {
        return AnsiString(GetVertexAttribName((VABaseType)ss->basetype,ss->vec_size));
    }

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

                if(auto cmp = items[i].input_rate <=> saa.items[i].input_rate; cmp != 0)
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

        bool Contains(const char *name)const
        {
            if(count<=0)
                return(false);

            for(uint i=0;i<count;i++)
                if(::hgl::strcmp(items[i].name,name)==0)
                    return(true);

            return(false);
        }

        bool Add(VIA &via)
        {
            if(Contains(via.name))
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
    };//struct VertexInputAttributeArray

    using VIAArray=VertexInputAttributeArray;
}//namespace hgl::graph
