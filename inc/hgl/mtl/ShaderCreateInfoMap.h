#pragma once

namespace hgl::graph::mtl {}

#include<cstdint>
#include <hgl/common/ShaderStageDef.h>
#include<ankerl/unordered_dense.h>

namespace hgl{namespace graph{namespace mtl{
    using namespace hgl::graph::mtl;

class ShaderCreateInfo;

class ShaderCreateInfoMap
{
    using MapType=ankerl::unordered_dense::map<ShaderStage,ShaderCreateInfo *>;
    MapType map;

public:

    auto begin(){return map.begin();}
    auto end(){return map.end();}
    auto begin()const{return map.begin();}
    auto end()const{return map.end();}

    bool IsEmpty()const{return map.empty();}
    int GetCount()const{return static_cast<int>(map.size());}
    void Clear(){map.clear();}

    bool ContainsKey(const ShaderStage flag)const
    {
        return map.contains(flag);
    }

    bool Add(const ShaderStage flag,ShaderCreateInfo *sc)
    {
        if(!sc)
            return false;

        if(ContainsKey(flag))
            return false;

        map.emplace(flag,sc);
        return true;
    }

    bool Add(ShaderCreateInfo *sc);

    // operator[] for accessing elements by key
    ShaderCreateInfo* operator[](const ShaderStage& key)
    {
        auto iter=map.find(key);
        return iter!=map.end()?iter->second:nullptr;
    }

    const ShaderCreateInfo* operator[](const ShaderStage& key) const
    {
        auto iter=map.find(key);
        return iter!=map.end()?iter->second:nullptr;
    }
};
}}}//namespace hgl::graph::mtl
