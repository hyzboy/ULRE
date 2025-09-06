#include<hgl/component/Component.h>
#include<tsl/robin_map.h>
#include<hgl/type/String.h>
#include<hgl/log/LogInfo.h>

namespace hgl::graph
{
    namespace
    {
        using ComponentManagerMap=tsl::robin_map<size_t,ComponentManager *>;

        ComponentManagerMap *component_manager_map=nullptr;
    }//namespace

    void InitializeComponentManager()
    {
        if(component_manager_map)
            return;

        component_manager_map=new ComponentManagerMap;
    }

    void UninitializeComponentManager()
    {
        if(!component_manager_map)
            return;

        for(auto &cm : *component_manager_map)
            delete cm.second;

        delete component_manager_map;
        component_manager_map=nullptr;
    }

    bool RegisterComponentManager(ComponentManager *cm)
    {
        if(!cm)return(false);

        if(!component_manager_map)
            return(false);

        const size_t hash_code=cm->GetTypeHash();

        if(component_manager_map->contains(hash_code))
            return(false);

        component_manager_map->emplace(hash_code,cm);

        return(true);
    }

    ComponentManager *GetComponentManager(const size_t hash_code)
    {
        if(!component_manager_map)
            return(nullptr);

        if(!component_manager_map->contains(hash_code))
            return(nullptr);

        //tsl::robin_map的[]对于不存在的会自行插入一个，所以不要把下面的.at改成[]
        return component_manager_map->at(hash_code);
    }

    ComponentManager::~ComponentManager()
    {
        for(auto *c:component_set)
        {
            c->OnDetach(nullptr);

            //Component::~Component()函数会再次调用ComponentManager->DetachComponent，其中会执行component_set的删除，整个流程就会出现问题。
            //所以这里先OnDetachManager掉Component中的Manager属性，然后再执行删除
            c->OnDetachManager(this);

            LogInfo(AnsiString("~ComponentManager delete ")+AnsiString::numberOf(c->GetUniqueID()));
            delete c;
        }
    }

    int ComponentManager::GetComponents(ComponentList &comp_list,SceneNode *node)
    {
        if(!node)return(-1);
        if(comp_list.IsEmpty())return(-2);
        if(!component_manager_map)return(-3);

        int result=0;

        for(auto cc:component_set)
            if(cc->GetOwnerNode()==node)
            {
                comp_list.Add(cc);
                ++result;
            }

        return result;
    }

    void ComponentManager::UpdateComponents(const double delta_time)
    {
        //Component **cc=component_set.GetData();

        //for(int i=0;i<component_set.GetCount();i++)
        //    cc[i]->Update(delta_time);
    }
}//namespace hgl::graph
