#include<hgl/framework/AppFramework.h>
#include<hgl/common/DebugChannel.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/asset/LevelAssetRegistry.h>
#include<hgl/world/assembly/LevelLoader.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/InstanceCustomData.h>
#include<hgl/graph/render/shadow/CascadedShadowController.h>
#include<hgl/graph/geo/VertexDataManager.h>
#include<hgl/vk/DebugCaptureQueue.h>
#include<hgl/vk/VKOffscreenRenderTarget.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/GPUModelRefComponent.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/light_shadow/LightSystem.h>
#include<hgl/ecs/systems/render/light_shadow/MainLightShadowSystem.h>
#include<hgl/ecs/support/debug/DebugViewPass.h>
#include<hgl/time/Time.h>
#include<hgl/log/Log.h>
#include<hgl/type/Smart.h>
#include<stdlib.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

int os_main(int argc,os_char **argv)
{
    if(!ULRE_DEBUG_ON(DebugBridge))
    {
        GLogError(u8"RenderDiagnosticsFixture requires debugbridge");
        return 1;
    }

    AppFramework app(OS_TEXT("Render diagnostics fixture"));
    if(!app.Init(640,360,argc,argv))
        return 2;
    ECSContext *world=app.GetECSContext();
    GraphicsContext *graphics=world?world->GetGraphicsContext():nullptr;
    BufferManager *buffers=graphics?graphics->GetManager<BufferManager>():nullptr;
    LevelAssetRegistry *levels=graphics?graphics->GetLevelAssetRegistry():nullptr;
    if(!buffers || !levels)
        return 3;

    GeometryVertexFormat format{
        {VertexSemantic::Position,VF_V3F},
        {VertexSemantic::TexCoord,VF_V2HF},
        {VertexSemantic::Normal,VF_V2UN8},
    };
    if(::getenv("ULRE_DIAGNOSTIC_MISSING_NORMAL"))
    {
        format.Clear();
        if(!format.Add(VertexSemantic::Position,VF_V3F)
           || !format.Add(VertexSemantic::TexCoord,VF_V2HF))
            return 12;
    }
    VertexDataManager vertices(buffers,format);
    if(!vertices.Init(HGL_SIZE_1MB,HGL_SIZE_1MB,IndexType::U32))
        return 4;
    GeometryVertexFormat color_format{
        {VertexSemantic::Position,VF_V3F},
        {VertexSemantic::TexCoord,VF_V2F},
        {VertexSemantic::Normal,VF_V3F},
    };
    if(!color_format.Add(VertexSemantic::Color,VF_V4F,4,
                         ::getenv("ULRE_DIAGNOSTIC_BAD_COLOR_STRIDE")?32:16))
        return 20;
    VertexDataManager color_vertices(buffers,color_format);
    AutoDelete<Geometry> color_geometry;
    PrimitiveAsset color_primitive;
    GlobalSSBODataAccessor custom_row;
    LevelInstance instance;
    LevelAsset *level=levels->GetOrLoad(OS_TEXT("example/Debug/RenderDiagnosticsFixture.level.toml"));
    GeometryCreater creater(&vertices);
    if(!level || !LevelLoader::Instantiate(*level,world,&creater,instance))
        return 5;

    if(::getenv("ULRE_DIAGNOSTIC_SHARED_CUBE"))
    {
        LevelInstance::RuntimeNode *cube=nullptr,*duplicate=nullptr;
        for(int i=0;i<instance.GetNodeCount();++i)
        {
            auto *node=instance.GetNode(i);
            if(node->name=="Cube") cube=node;
            if(node->name=="CubeInstance") duplicate=node;
        }
        ecs::GeometryData *geometry=duplicate?world->GetGeometryData(duplicate->entity_id):nullptr;
        if(!cube || !geometry)
            return 15;
        geometry->SetPrimitiveAsset(&cube->primitive);
        if(::getenv("ULRE_DIAGNOSTIC_VERTEX_GEOMETRY"))
        {
            if(!color_vertices.Init(HGL_SIZE_1MB,HGL_SIZE_1MB,IndexType::U32))
                return 13;
            GeometryCreater color_creator(&color_vertices);
            const math::Vector3f positions[]={
                {-1,0,0},{1,0,0},{1,0,2},{-1,0,0},{1,0,2},{-1,0,2}};
            const math::Vector2f uv[]={{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}};
            const math::Vector3f normals[]={
                {0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0}};
            const uint32_t indices[]={0,2,1,3,5,4};
            const float alpha=::getenv("ULRE_DIAGNOSTIC_VERTEX_ALPHA")?0.25f:1.0f;
            const math::Vector4f colors[]={
                {0.5f,0.75f,1,alpha},{0.5f,0.75f,1,alpha},{0.5f,0.75f,1,alpha},
                {0.5f,0.75f,1,alpha},{0.5f,0.75f,1,alpha},{0.5f,0.75f,1,alpha}};
            if(!color_creator.Init("diagnostic_color_quad",6,6)
               || !color_creator.WriteVAB(VertexSemantic::Position,VF_V3F,positions)
               || !color_creator.WriteVAB(VertexSemantic::TexCoord,VF_V2F,uv)
               || !color_creator.WriteVAB(VertexSemantic::Normal,VF_V3F,normals)
               || !color_creator.WriteVAB(VertexSemantic::Color,VF_V4F,colors)
               || !color_creator.WriteIBO(indices,6))
                return 14;
            color_geometry=color_creator.CreateWithAABB({-1,0,0},{1,0,2});
            if(!color_geometry)
                return 18;
            color_primitive=PrimitiveAsset(color_geometry,&cube->recipe);
            ecs::GeometryData *original=world->GetGeometryData(cube->entity_id);
            if(!original)
                return 19;
            original->SetPrimitiveAsset(&color_primitive);
            geometry->SetPrimitiveAsset(&color_primitive);
        }
        if(::getenv("ULRE_DIAGNOSTIC_INSTANCE_ALPHA"))
        {
            auto *registry=graphics->GetGlobalSSBOBufferRegistry();
            Entity *entity=world->GetEntity(duplicate->entity_id);
            auto *ref=entity?entity->AddComponent<GPUModelRefComponent>():nullptr;
            if(!registry || !ref)
                return 16;
            custom_row=registry->GetAccessor(GlobalSSBOType::InstanceCustomDataPool);
            if(!custom_row || !custom_row.GetDataID()
               || !registry->Write(GlobalSSBOType::InstanceCustomDataPool,custom_row.GetDataID(),
                                   ssbo::InstanceCustomData(math::Vector4f(0.5f,0.75f,1.0f,0.25f))))
                return 17;
            ref->custom_data_index=custom_row.GetDataID();
        }
    }

    auto light=world->GetSystem<LightSystem>();
    if(!light)
        light=world->RegisterSystem<LightSystem>();
    auto shadows=world->GetSystem<MainLightShadowSystem>();
    if(!shadows)
        shadows=world->RegisterSystem<MainLightShadowSystem>();
    if(!light || !shadows)
        return 7;
    light->SetMainLightDirection(math::Vector3f(0.55f,0.35f,0.75f));
    light->SetMainLightColor(Color4f(1.0f,0.94f,0.88f,1.0f));
    light->SetMainLightIntensity(1.5f);
    light->SetMainLightAngleDeg(0.53f);
    CascadedShadowConfig shadow{};
    shadow.cascade_count=1;
    shadow.max_distance=50.0f;
    shadow.split_distances[0]=50.0f;
    shadow.use_custom_splits=true;
    shadow.c0_dynamic_overlay=false;
    shadow.shadow_map_size=1024.0f;
    shadow.caster_depth_margin=120.0f;
    shadow.bias=-0.003f;
    shadow.pcf_radius=1.5f;
    shadow.darkness=0.15f;
    shadow.blend_width=0.05f;
    if(!shadows->EnableMainLightShadow(shadow,1024))
        return 8;
    auto *shadow_target=dynamic_cast<OffscreenRenderTarget *>(shadows->GetCascadeRenderTarget(0));
    if(!shadow_target)
        return 9;
    shadow_target->RegisterDebugCapture("CSM_Cascade_0");
    shadow_target->EnableManagedDebugCapture();
    const uint64_t shadow_target_id=shadow_target->GetDebugCaptureTargetID();
    if(!shadow_target_id)
        return 10;

    // 取相机只走世界级访问器（A7c：示例不直取组件，`example/` 下不得出现组件类型名）
    auto camera_system=world->GetSystem<CameraSystem>();
    auto *camera=camera_system?world->GetCameraByEntity(instance.GetCameraEntityID()):nullptr;
    if(!camera)
        return 6;
    camera_system->SetOverrideCamera(camera);

    constexpr float delta=1.0f/60.0f;
    constexpr uint32_t warmup_frames=120;
    uint32_t remaining=warmup_frames;
    auto mode=DebugViewPass::GetRequestedMode();
    bool ready_reported=false;
    Window *window=app.GetWindow();
    const bool paired_capture=::getenv("ULRE_DIAGNOSTIC_PAIRED_CAPTURE")!=nullptr;
    auto *main_target=dynamic_cast<SwapchainRenderTarget *>(world->GetRenderTarget());
    const uint64_t main_target_id=main_target?main_target->GetDebugCaptureTargetID():0;
    if(paired_capture && !main_target_id)
        return 11;
    while(window && window->Update())
    {
        const auto requested=DebugViewPass::GetRequestedMode();
        if(requested!=mode)
        {
            mode=requested;
            remaining=warmup_frames;
            ready_reported=false;
        }
        const uint64_t target=DebugViewPass::GetTargetID();
        if(remaining==0 && mode!=mtl::DebugViewMode::Off &&
           DebugViewPass::GetAppliedMode()==mode && target)
        {
            if(!ready_reported)
            {
                GLogInfo(u8"DIAGNOSTIC_FIXTURE_READY mode=%s target=%llu warmup=%u",
                         mtl::GetDebugViewModeName(mode),
                         static_cast<unsigned long long>(target),warmup_frames);
                ready_reported=true;
            }
            const bool debug_queued=DebugCaptureQueue::HasQueued(target);
            const bool main_queued=DebugCaptureQueue::HasQueued(main_target_id);
            if((paired_capture && !(debug_queued && main_queued))
               || (!paired_capture && !debug_queued && !DebugCaptureQueue::HasQueued(shadow_target_id)))
            {
                SleepSecond(0.001);
                continue;
            }
        }
        else if(remaining==0 && mode==mtl::DebugViewMode::Off)
        {
            if(!ready_reported)
            {
                GLogInfo(u8"DIAGNOSTIC_FIXTURE_READY mode=off target=0 warmup=%u",
                         warmup_frames);
                ready_reported=true;
            }
            if(!DebugCaptureQueue::HasQueued(shadow_target_id)
               && !(paired_capture && DebugCaptureQueue::HasQueued(main_target_id)))
            {
                SleepSecond(0.001);
                continue;
            }
        }
        camera_system->SetOverrideCamera(camera);
        world->Tick(delta);
        world->Render(delta,{});
        if(remaining)
            --remaining;
    }

    instance.Release();
    if(auto *geometry=graphics->GetManager<GeometryManager>())
        geometry->Release();
    return 0;
}
