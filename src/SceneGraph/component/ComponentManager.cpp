#include<hgl/component/Component.h>
#include<tsl/robin_map.h>

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

    bool RegistryComponentManager(ComponentManager *cm)
    {
        if(!cm)return(false);

        if(!component_manager_map)
            return(false);

        const size_t hash_code=cm->GetHashCode();

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

        //[]对于不存的会自行插入一个，所以不要把下面的.at改成[]
        return component_manager_map->at(hash_code);
    }

    int ComponentManager::GetComponents(ArrayList<Component *> &comp_list,SceneNode *node)
    {
        if(!node)return(-1);
        if(comp_list.IsEmpty())return(-2);
        if(!component_manager_map)return(-3);

        Component **cc=component_set.GetData();

        int result=0;

        for(int i=0;i<component_set.GetCount();i++)
            if(cc[i]->GetOwnerNode()==node)
            {
                comp_list.Add(cc[i]);
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
