#include<hgl/graph/Scene.h>
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
        Map<U8String,Scene *> scene_world_map;///<场景列表
    }//namespace

    bool RegisterScene(Scene *sw)
    {
        if(!sw)return(false);

        const U8String &scene_name=sw->GetSceneName();

        if(scene_world_map.Find(scene_name))
            return false;///<已经注册过了

        scene_world_map.Add(scene_name,sw);
        return true;
    }

    Scene *GetScene(const U8String &scene_name)
    {
        if(scene_name.IsEmpty())
            return(nullptr);

        return GetObjectFromMap(scene_world_map,scene_name);
    }

    bool UnregisterScene(const U8String &scene_name)
    {
        if(scene_name.IsEmpty())
            return(false);

        return scene_world_map.DeleteByKey(scene_name);
    }
}//namespace hgl::graph

namespace hgl::graph
{
    Scene::Scene(RenderFramework *rf)
    {
        render_framework=rf;

        descriptor_binding=new DescriptorBinding(DescriptorSetType::Scene);

        {
            ubo_sky_info=rf->CreateUBO<UBOSkyInfo>(&mtl::SBS_SkyInfo);

            //float hour;
            //float minute;
            //float second;
            //{
            //    const auto now=std::chrono::system_clock::now();
            //    const auto tt=std::chrono::system_clock::to_time_t(now);
            //    const auto local_tm=*localtime(&tt);
            //    hour=local_tm.tm_hour;
            //    minute=local_tm.tm_min;
            //    second=local_tm.tm_sec;
            //}

            ubo_sky_info->data()->SetTime(10,0,0);  //早上10点

            descriptor_binding->AddUBO(ubo_sky_info);

            line_render_manager=CreateLineRenderManager(rf,rf->GetSwapchainRenderTarget());       //先用默认的RenderTarget，未来可能会需要动态切RenderTarget。到时候再说了。
        }

        root_node=new SceneNode(this);
    }

    Scene::~Scene()
    {
        SAFE_CLEAR(line_render_manager);
        SAFE_CLEAR(root_node);
        SAFE_CLEAR(descriptor_binding);
    }
}//namespace hgl::graph
