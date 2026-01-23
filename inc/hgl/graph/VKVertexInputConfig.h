#pragma once
#include<hgl/graph/VKFormat.h>
#include<hgl/type/String.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN
struct VAConfig
{
    VkFormat format;
    VkVertexInputRate input_rate;

public:

    VAConfig()
    {
        format=PF_UNDEFINED;
        input_rate=VK_VERTEX_INPUT_RATE_VERTEX;
    }

    VAConfig(const VkFormat fmt,const VkVertexInputRate ir=VK_VERTEX_INPUT_RATE_VERTEX)
    {
        format=fmt;
        input_rate=ir;
    }

    auto operator <=> (const VAConfig &vc)const=default;
};//struct VAConfig

class VILConfig:public Map<AnsiString,VAConfig>
{
public:

    using Map<AnsiString,VAConfig>::Map;

    bool Add(const AnsiString &name,const VkFormat fmt,const VkVertexInputRate ir=VK_VERTEX_INPUT_RATE_VERTEX)
    {
        if(ContainsKey(name))
            return(false);

        return Map<AnsiString,VAConfig>::Add(name,VAConfig(fmt,ir));
    }

    auto operator <=> (const VILConfig &vc)const
    {
        int off;

        off=GetCount()-vc.GetCount();
        if(off)return(off);

        for(const auto &pair:*this)
        {
            VAConfig vac;
            if(!vc.Get(pair->key,vac))
                return(1);

            auto cmp=pair->value<=>vac;
            if(cmp!=0)return(cmp<0?-1:1);
        }

        return 0;
    }
};//class VILConfig:public Map<AnsiString,VAConfig>
VK_NAMESPACE_END
