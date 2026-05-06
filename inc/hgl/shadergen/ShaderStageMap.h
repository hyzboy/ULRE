#pragma once

#include<cstdint>
#include<memory>
#include<ankerl/unordered_dense.h>
#include<hgl/shadergen/ShaderCreateInfo.h>

namespace hgl{namespace graph{
enum class ShaderStage:uint32_t;

class ShaderStageMap
{
    using MapType=ankerl::unordered_dense::map<ShaderStage,std::unique_ptr<ShaderCreateInfo>>;
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
        std::unique_ptr<ShaderCreateInfo> owned(sc);

        if(!owned)
            return false;

        if(ContainsKey(flag))
            return false;

        map.emplace(flag,std::move(owned));
        return true;
    }

    bool Add(ShaderCreateInfo *sc);

    /// 命中返回 ShaderCreateInfo*，未命中返回 nullptr。
    /// 与 std::unordered_map::operator[] 不同，不会插入新元素。
    ShaderCreateInfo *Find(const ShaderStage key)
    {
        auto it = map.find(key);
        return it != map.end() ? it->second.get() : nullptr;
    }
    const ShaderCreateInfo *Find(const ShaderStage key) const
    {
        auto it = map.find(key);
        return it != map.end() ? it->second.get() : nullptr;
    }

    /// 带 out 参数的版本，命中返回 true。
    bool TryGet(const ShaderStage key, ShaderCreateInfo *&out)
    {
        auto it = map.find(key);
        if (it == map.end()) return false;
        out = it->second.get();
        return true;
    }

    [[deprecated("Use Find(key); operator[] does NOT match std::map semantics (returns nullptr, not default-insert)")]]
    // operator[] for accessing elements by key
    ShaderCreateInfo* operator[](const ShaderStage& key)
    {
        auto iter=map.find(key);
        return iter!=map.end()?iter->second.get():nullptr;
    }

    [[deprecated("Use Find(key); operator[] does NOT match std::map semantics (returns nullptr, not default-insert)")]]
    const ShaderCreateInfo* operator[](const ShaderStage& key) const
    {
        auto iter=map.find(key);
        return iter!=map.end()?iter->second.get():nullptr;
    }
};
}}//namespace hgl::graph
