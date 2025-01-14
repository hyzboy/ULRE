#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/IDName.h>

VK_NAMESPACE_BEGIN

class GraphModule;

class GraphModulesMap
{
    SortedSet<GraphModule *> gm_set;

    Map<AIDName,GraphModule *> gm_map_by_name;
    Map<size_t,GraphModule *> gm_map_by_hash;

    List<GraphModule *> gm_list;        //按创建顺序记录，用于倒序释放

public:

    bool Add(GraphModule *gm);

    const bool IsEmpty()const
    {
        return gm_set.IsEmpty();
    }

    GraphModule *Get(const AIDName &name)
    {
        GraphModule *gm;

        if(gm_map_by_name.Get(name,gm))
            return gm;

        return nullptr;
    }

    template<typename T>
    T *Get()
    {
        GraphModule *gm;

        return gm_map_by_hash.Get(GetTypeHash<T>(),gm)?(T *)gm:nullptr;
    }

    template<typename T>
    const bool IsLoaded()const{return gm_map_by_hash.ContainsKey(T::GetTypeHash());}

    const bool IsLoaded(const AIDName &name)const{return gm_map_by_name.ContainsKey(name);}

    bool Release(GraphModule *gm);          ///<释放一个模块

    void Destory();                         ///<销毁所有模块
};//class GraphModulesMap

VK_NAMESPACE_END
