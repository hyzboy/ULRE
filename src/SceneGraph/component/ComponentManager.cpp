#include<hgl/component/Component.h>

namespace hgl::graph
{
    int ComponentManager::GetComponents(List<Component *> &comp_list,SceneNode *node)
    {
        if(!node)return(-1);

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
        Component **cc=component_set.GetData();

        for(int i=0;i<component_set.GetCount();i++)
            cc[i]->Update(delta_time);
    }
}//namespace hgl::graph
