#include<hgl/graph/World.h>
#include<hgl/type/Map.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKRenderTargetSwapchain.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/geo/line/LineRenderManager.h>

namespace hgl::graph
{
    LineRenderManager *CreateLineRenderManager(RenderFramework *rf,IRenderTarget *);

    namespace
    {
        Map<U8String,World *> registered_world_map;  ///<世界列表
    }//namespace

    bool RegisterWorld(World *sw)
    {
        if(!sw)return(false);

        const U8String &world_name=sw->GetWorldName();

        if(registered_world_map.Find(world_name))
            return false;///<已经注册过了

        registered_world_map.Add(world_name,sw);
        return true;
    }

    World *GetWorld(const U8String &world_name)
    {
        if(world_name.IsEmpty())
            return(nullptr);

        return GetObjectFromMap(registered_world_map,world_name);
    }

    bool UnregisterWorld(const U8String &world_name)
    {
        if(world_name.IsEmpty())
            return(false);

        return registered_world_map.DeleteByKey(world_name);
    }
}//namespace hgl::graph

namespace hgl::graph
{
    World::World(RenderFramework *rf)
    {
        render_framework=rf;

        scene_desc_binding=new DescriptorBinding(DescriptorSetType::World);

        {
            ubo_sky_info=rf->CreateUBO<UBOSkyInfo>(&mtl::SBS_SkyInfo);
            ubo_sky_info->data()->SetTime(10,0,0);  //早上10点
            scene_desc_binding->AddUBO(ubo_sky_info);
        }

        root_node=new SceneNode(this);
    }

    World::~World()
    {
        SAFE_CLEAR(root_node);
        SAFE_CLEAR(scene_desc_binding);
    }
}//namespace hgl::graph
