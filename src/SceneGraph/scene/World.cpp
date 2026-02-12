#include<hgl/graph/World.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKRenderTargetSwapchain.h>
#include<hgl/graph/mtl/UBOCommon.h>

namespace hgl::graph
{
    namespace
    {
        UnorderedMap<IDString,World *> registered_world_map;  ///<世界列表
    }//namespace

    bool RegisterWorld(World *sw)
    {
        if(!sw)return(false);

        const IDString &world_name=sw->GetWorldName();

        if(registered_world_map.ContainsKey(world_name))
            return false;///<已经注册过了

        registered_world_map.Add(world_name,sw);
        return true;
    }

    World *GetWorld(const IDString &world_name)
    {
        if(world_name.IsEmpty())
            return(nullptr);

        World *world = nullptr;
        registered_world_map.Get(world_name, world);
        return world;
    }

    bool UnregisterWorld(const IDString &world_name)
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

        world_desc_binding=new DescriptorBinding(DescriptorSetType::World);

        {
            ubo_sky_info=rf->CreateUBO<UBOSkyInfo>(&mtl::SBS_SkyInfo,BufferUpdateClass::Deferred);
            if(ubo_sky_info)
            {
                ubo_sky_info->Data()->SetTime(10,0,0);  //早上10点

                world_desc_binding->AddUBO(ubo_sky_info);
            }
        }

        root_node=new SceneNode(this);
    }

    World::~World()
    {
        for(SceneNode *sn : all_nodes)
        {
            delete sn;
        }

        SAFE_CLEAR(root_node);
        SAFE_CLEAR(world_desc_binding);
    }
}//namespace hgl::graph
