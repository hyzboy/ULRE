#pragma once

#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VKInterpolation.h>
#include<hgl/type/ArrayList.h>
#include<hgl/type/String.h>

namespace hgl::graph
{
    const uint GetShaderCountByBits(const uint32_t bits);                   ///<根据ShaderStage位数据统计有多少个shader
    const uint GetMaxShaderStage(const uint32_t bits);                      ///<根据ShaderStage位数据获取最大的ShaderStage位
    const char *GetShaderStageName(const VkShaderStageFlagBits &);          ///<获取指定ShaderStage位的名称
    const uint GetShaderStageFlagBits(const char *,int len=0);              ///<根据名称获取ShaderStage位数据

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

    using VIAList=ArrayList<VIA>;

    inline const AnsiString GetShaderAttributeTypename(const VertexInputAttribute *ss)
    {
        return AnsiString(GetVertexAttribName((VABaseType)ss->basetype,ss->vec_size));
    }

    const VkFormat GetVulkanFormat(const VertexInputAttribute *sa);

    struct VertexInputAttributeArray:public Comparator<VertexInputAttributeArray>
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

        int Comp(const VertexInputAttributeArray *saa)const
        {
            if(!saa)
                return 1;

            int off=count-saa->count;
            if(off)return off;

            for(uint i=0;i<count;i++)
            {
            #define SAA_COMP_ITEM(name) off=items[i].name-saa->items[i].name;if(off)return off;
                SAA_COMP_ITEM(location)
                SAA_COMP_ITEM(basetype)
                SAA_COMP_ITEM(vec_size)
                SAA_COMP_ITEM(input_rate)
            #undef SAA_COMP_ITEM
                        
                off=hgl::strcmp(items[i].name,saa->items[i].name);
                if(off)return off;
            }

            return 0;
        }

        const int compare(const VertexInputAttributeArray &saa)const override
        {
            return Comp(&saa);
        }

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
                if(hgl::strcmp(items[i].name,name)==0)
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
