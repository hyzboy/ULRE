#pragma once

namespace hgl::graph::mtl {}

#include<cstdint>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/type/Smart.h>
#include<ankerl/unordered_dense.h>

namespace hgl{namespace graph{namespace mtl{
class ShaderCreateInfoMap
{
    /// 值用 `SharedPtr` 承载所有权：map 是 `ShaderCreateInfo` 的唯一持有者，
    /// 表项销毁即归还对象 —— 上层不再需要手写 delete 循环。
    using MapType=ankerl::unordered_dense::map<ShaderStage,hgl::SharedPtr<ShaderCreateInfo>>;
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
        return iter!=map.end()?iter->second.get():nullptr;
    }

    const ShaderCreateInfo* operator[](const ShaderStage& key) const
    {
        auto iter=map.find(key);
        return iter!=map.end()?iter->second.get():nullptr;
    }
};
}}}//namespace hgl::graph::mtl
