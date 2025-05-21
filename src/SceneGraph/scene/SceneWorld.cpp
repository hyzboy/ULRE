#include<hgl/graph/SceneWorld.h>
#include<hgl/type/Map.h>

namespace hgl::graph
{
    namespace
    {
        Map<U8String,SceneWorld *> scene_world_map;///<场景世界列表
    }//namespace

    bool RegistrySceneWorld(SceneWorld *sw)
    {
        if(!sw)return(false);

        const U8String &world_name=sw->GetWorldName();

        if(scene_world_map.Find(world_name))
            return false;///<已经注册过了

        scene_world_map.Add(world_name,sw);
        return true;
    }

    SceneWorld *GetSceneWorld(const U8String &world_name)
    {
        if(world_name.IsEmpty())
            return(nullptr);

        return GetObjectFromMap(scene_world_map,world_name);
    }

    bool UnregistrySceneWorld(const U8String &world_name)
    {
        if(world_name.IsEmpty())
            return(false);

        return scene_world_map.DeleteByKey(world_name);
    }
}//namespace hgl::graph
