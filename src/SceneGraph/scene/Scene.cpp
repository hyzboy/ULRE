#include<hgl/graph/Scene.h>
#include<hgl/type/Map.h>

namespace hgl::graph
{
    namespace
    {
        Map<U8String,Scene *> scene_world_map;///<场景列表
    }//namespace

    bool RegistryScene(Scene *sw)
    {
        if(!sw)return(false);

        const U8String &world_name=sw->GetSceneName();

        if(scene_world_map.Find(world_name))
            return false;///<已经注册过了

        scene_world_map.Add(world_name,sw);
        return true;
    }

    Scene *GetScene(const U8String &world_name)
    {
        if(world_name.IsEmpty())
            return(nullptr);

        return GetObjectFromMap(scene_world_map,world_name);
    }

    bool UnregistryScene(const U8String &world_name)
    {
        if(world_name.IsEmpty())
            return(false);

        return scene_world_map.DeleteByKey(world_name);
    }
}//namespace hgl::graph
