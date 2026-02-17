#pragma once
#include<hgl/vk/VKFormat.h>
#include<hgl/type/String.h>
#include<hgl/type/UnorderedMap.h>
#include<vector>

namespace hgl::graph{
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

class VILConfig:public hgl::UnorderedMap<AnsiString,VAConfig>
{
public:

    using Base=hgl::UnorderedMap<AnsiString,VAConfig>;

    using Base::Base;

    bool Add(const AnsiString &name,const VAConfig &cfg)
    {
        if(this->ContainsKey(name))
            return(false);

        return static_cast<Base *>(this)->Add(name,cfg);
    }

    auto operator <=> (const VILConfig &vc)const
    {
        int off=this->GetCount()-vc.GetCount();
        if(off)return(off);

        std::vector<AnsiString> keys;
        std::vector<VAConfig> values;
        this->GetKeyValueArrays(keys, values);

        for(size_t i=0;i<keys.size();++i)
        {
            VAConfig vac;
            if(!vc.Get(keys[i],vac))
                return(1);

            auto cmp=values[i]<=>vac;
            if(cmp!=0)return(cmp<0?-1:1);
        }

        return 0;
    }
};//class VILConfig:public Map<AnsiString,VAConfig>
}//namespace hgl::graph
