#pragma once
#include<hgl/vk/VKFormat.h>
#include<hgl/common/VertexAttribDef.h>
#include<ankerl/unordered_dense.h>

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

class VILConfig:public ankerl::unordered_dense::map<VertexAttrib,VAConfig>
{
public:

    using Base=ankerl::unordered_dense::map<VertexAttrib,VAConfig>;

    using Base::Base;

    int GetCount() const
    {
        return static_cast<int>(this->size());
    }

    bool ContainsKey(const VertexAttrib &attrib) const
    {
        return this->find(attrib) != this->end();
    }

    bool Get(const VertexAttrib &attrib,VAConfig &cfg) const
    {
        auto it=this->find(attrib);
        if(it==this->end())
            return false;

        cfg=it->second;
        return true;
    }

    bool Add(const VertexAttrib &attrib,const VAConfig &cfg)
    {
        if(this->ContainsKey(attrib))
            return(false);

        return this->emplace(attrib,cfg).second;
    }

    auto operator <=> (const VILConfig &vc)const
    {
        int off=this->GetCount()-vc.GetCount();
        if(off)return(off);

        for(const auto &[key,value]:*this)
        {
            VAConfig vac;
            if(!vc.Get(key,vac))
                return(1);

            auto cmp=value<=>vac;
            if(cmp!=0)return(cmp<0?-1:1);
        }

        return 0;
    }
};//class VILConfig:public Map<AnsiString,VAConfig>
}//namespace hgl::graph
