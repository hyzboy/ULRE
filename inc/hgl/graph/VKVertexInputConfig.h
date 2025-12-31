#pragma once
#include<hgl/graph/VKFormat.h>
#include<hgl/type/String.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN
struct VAConfig:public ComparatorData<VAConfig>
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
};//struct VAConfig

class VILConfig:public Map<AnsiString,VAConfig>,public Comparator<VILConfig>
{
public:

    using Map<AnsiString,VAConfig>::Map;

    bool Add(const AnsiString &name,const VkFormat fmt,const VkVertexInputRate ir=VK_VERTEX_INPUT_RATE_VERTEX)
    {
        if(ContainsKey(name))
            return(false);

        return Map<AnsiString,VAConfig>::Add(name,VAConfig(fmt,ir));
    }

    const int compare(const VILConfig &vc)const override
    {
        int off;

        off=GetCount()-vc.GetCount();
        if(off)return(off);

        auto **sp=GetDataList();
        VAConfig vac;

        for(int i=0;i<GetCount();i++)
        {
            if(!vc.Get((*sp)->key,vac))
                return(1);

            off=(*sp)->value.compare(vac);
            if(off)return(off);

            ++sp;
        }

        return 0;
    }
};//class VILConfig:public Map<AnsiString,VAConfig>
VK_NAMESPACE_END
