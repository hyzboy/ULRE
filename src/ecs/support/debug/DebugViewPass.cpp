#include<hgl/ecs/support/debug/DebugViewPass.h>
#include<hgl/ecs/core/render/MaterialBatch.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/render/PrimitiveRenderItem.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/support/pipeline/PipelineMaterialRenderer.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/vk/DebugCaptureQueue.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKOffscreenRenderTarget.h>
#include<hgl/vk/VKRenderbufferInfo.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/buffer/VertexAttribBuffer.h>
#include<hgl/sync/Mutex.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/ValueArray.h>
#include<hgl/log/Log.h>
#include<stdlib.h>

namespace hgl::ecs
{
    namespace
    {
        using namespace hgl::graph;
        using namespace hgl::graph::mtl;

        struct WorldState
        {
            RenderTargetManager *manager=nullptr;
            OffscreenRenderTarget *target=nullptr;
            uint64_t target_id=0;
            uint64_t recorded_frame=0;
            IRenderTarget *recorded_main=nullptr;
            DebugViewMode recorded_mode=DebugViewMode::Off;
            uint32_t recorded_batches=0;
            uint32_t recorded_entities=0;
            VkFormat color_format=VK_FORMAT_UNDEFINED;
        };

        struct Status
        {
            hgl::Mutex lock;
            DebugViewMode requested=DebugViewMode::Off;
            DebugViewMode applied=DebugViewMode::Off;
            ECSContext *owner=nullptr;
            uint64_t target_id=0;
            uint32_t batches=0;
            uint32_t entities=0;
            const char *error=nullptr;
        };

        Status &SharedStatus()
        {
            static Status status;
            static const bool initialized=[]()
            {
                const char *name=::getenv("ULRE_DEBUG_VIEW");
                if(name && name[0])
                {
                    if(!ParseDebugViewMode(name,status.requested))
                        GLogWarning(u8"[DebugView] Invalid ULRE_DEBUG_VIEW: %s",name);
                }
                return true;
            }();
            (void)initialized;
            return status;
        }

        hgl::Mutex &WorldLock()
        {
            static hgl::Mutex lock;
            return lock;
        }

        hgl::UnorderedMap<ECSContext *,WorldState> &Worlds()
        {
            static hgl::UnorderedMap<ECSContext *,WorldState> worlds;
            return worlds;
        }

        void SetStatus(ECSContext *world,DebugViewMode mode,uint64_t target_id,
                       uint32_t batches,const char *error,uint32_t entities=0)
        {
            Status &status=SharedStatus();
            hgl::MutexLock lock(&status.lock);
            status.owner=world;
            status.applied=mode;
            status.target_id=target_id;
            status.batches=batches;
            status.entities=entities;
            status.error=error;
        }

        bool DropWorld(ECSContext *world,bool shutdown=false)
        {
            WorldState *state=Worlds().GetValuePointer(world);
            if(!state) return true;
            if(state->target && state->manager && !state->manager->Destroy(state->target))
            {
                GLogError(u8"[DebugViewPass] Failed to destroy debug target");
                if(!shutdown) return false;
            }
            if(state->target_id)
                DebugCaptureQueue::UnregisterTarget(state->target_id);
            Worlds().DeleteByKey(world);
            return true;
        }

        void ClearStatus(ECSContext *world,bool shutdown)
        {
            Status &status=SharedStatus();
            hgl::MutexLock lock(&status.lock);
            if(status.owner!=world) return;
            if(shutdown && Worlds().IsEmpty()) status.requested=DebugViewMode::Off;
            status.applied=DebugViewMode::Off;
            status.owner=nullptr;
            status.target_id=0;
            status.batches=0;
            status.entities=0;
            status.error=nullptr;
        }

        bool CreateTarget(ECSContext *world,IRenderTarget *main,DebugViewMode mode,
                          WorldState &state,const char *&error)
        {
            GraphicsContext *graphics=world->GetGraphicsContext();
            RenderTargetManager *manager=graphics?graphics->GetRenderTargetManager():nullptr;
            if(!manager || !main->GetDepthTexture())
            {
                error="target_creation_failed";
                return false;
            }

            const VkFormat color_format=mode==DebugViewMode::WorldPosition
                ?VK_FORMAT_R32G32B32A32_SFLOAT:VK_FORMAT_R8G8B8A8_UNORM;
            if(mode==DebugViewMode::WorldPosition)
            {
                const VulkanPhyDevice *physical=graphics->GetPhyDevice();
                const VkFormatFeatureFlags required=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
                                                     |VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
                if(!physical || !physical->OptimalSupport(color_format,required))
                {
                    error="position_format_unavailable";
                    return false;
                }
            }
            FramebufferInfo info;
            if(!info.AddColor(color_format)
               || !info.SetDepth(main->GetDepthTexture()->GetFormat()))
            {
                error=mode==DebugViewMode::WorldPosition
                    ?"position_format_unavailable":"target_creation_failed";
                return false;
            }
            const VkExtent2D extent=main->GetExtent();
            info.SetExtent(extent.width,extent.height);
            static uint64_t serial=0;
            const AnsiString name=AnsiString("DebugView_")+AnsiString::numberOf(++serial);
            OffscreenRenderTarget *target=manager->CreateOffscreenRT(world,name,&info,1,false);
            if(!target)
            {
                error=mode==DebugViewMode::WorldPosition
                    ?"position_target_creation_failed":"target_creation_failed";
                return false;
            }
            const uint64_t id=DebugCaptureQueue::RegisterTarget(
                true,true,DebugCaptureQueue::Source::DebugView);
            if(!id)
            {
                if(!manager->Destroy(target))
                    GLogError(u8"[DebugViewPass] Failed to clean up target after registration failure");
                error="target_registration_failed";
                return false;
            }
            target->SetClearColor(Color4f(0,0,0,0));
            state.manager=manager;
            state.target=target;
            state.target_id=id;
            state.color_format=color_format;
            return true;
        }

        struct PreparedBatch
        {
            MaterialBatch *batch=nullptr;
            ShaderProgram *program=nullptr;
            Pipeline *pipeline=nullptr;
        };

        bool SameVertexFormat(const GeometryVertexFormat &a,const GeometryVertexFormat &b)
        {
            if(a.GetCount()!=b.GetCount()) return false;
            for(uint32_t i=0;i<a.GetCount();++i)
            {
                const auto *lhs=a.Get(i);
                const auto *rhs=b.Get(i);
                if(lhs->semantic!=rhs->semantic || lhs->format!=rhs->format
                   || lhs->vec_size!=rhs->vec_size || lhs->stride!=rhs->stride)
                    return false;
            }
            return true;
        }

        bool PrepareBatch(ECSContext *world,GraphicsContext *graphics,RenderPass *pass,
                          MaterialBatch *batch,DebugViewMode mode,PreparedBatch &prepared)
        {
            if(!batch->gpu_driven_override || !batch->icb_mesh_tasks || !batch->renderer
               || batch->pipeline_config.alpha_blend || batch->pipeline_config.overlay
               || !batch->pipeline_config.depth_test || !batch->pipeline_config.depth_write)
                return false;
            const MaterialRecipe *base=nullptr;
            const GeometryVertexFormat *format=nullptr;
            PrimitiveType primitive_type=PrimitiveType::Triangles;
            ResolvedMaterialRenderState base_state{};
            for(RenderItem *item:batch->items)
            {
                const auto *primitive=dynamic_cast<const PrimitiveRenderItem *>(item);
                const MaterialRuntimeRow *row=primitive?primitive->GetMaterialRuntimeRow():nullptr;
                const GeometryData *geometry=primitive?world->GetGeometryData(primitive->GetEntityID()):nullptr;
                const PrimitiveAsset *asset=geometry?geometry->GetPrimitiveAsset():nullptr;
                const auto *asset_geometry=asset?asset->GetGeometry():nullptr;
                if(!row || !asset_geometry) return false;
                const MaterialRecipe &recipe=row->cached_normalized_recipe;
                if(recipe.domain!=MaterialDomain::StandardOpaque) return false;
                const auto &binding=row->cached_effective_recipe.material_ssbo_binding;
                if(!binding.IsValid() || binding.ssbo_type!=GlobalSSBOType::PBRSurface)
                    return false;
                ResolvedMaterial definition{};
                if(!ResolveMaterialFromRecipe(recipe,definition)) return false;
                const ResolvedMaterialRenderState state=ResolveMaterialRenderState(definition,recipe);
                if(state.dither || !(state.pipeline_config==batch->pipeline_config)) return false;
                if(!recipe.surface_module_name.empty()
                   || (recipe.has_compile_defines && !recipe.compile_defines.empty())
                   || recipe.has_sampler_names || !IsDefault3DNodeConfig(recipe.vertex_node_config))
                    return false;
                const GeometryVertexFormat *item_format=&asset_geometry->GetGeometryVertexFormat();
                if(!item_format->Find(VertexSemantic::Normal)
                   || !item_format->Find(VertexSemantic::TexCoord)) return false;
                const auto *color=item_format->Find(VertexSemantic::Color);
                if(color)
                {
                    const VAB *vab=asset_geometry->GetVAB(VertexSemantic::Color);
                    if(color->format!=VF_V4F || color->vec_size!=4 || color->stride!=16
                       || !vab || vab->GetFormat()!=VF_V4F || vab->GetStride()!=16)
                        return false;
                }
                const PrimitiveType item_type=asset->GetPrimitiveType();
                if(item_type!=PrimitiveType::Triangles)
                    return false;
                if(base && (primitive_type!=item_type || !SameVertexFormat(*format,*item_format)
                            || state.alpha_test!=base_state.alpha_test
                            || state.dither!=base_state.dither
                            || (state.alpha_test && state.alpha_cutoff!=base_state.alpha_cutoff)))
                    return false;
                if(!base)
                {
                    base=&recipe;
                    format=item_format;
                    primitive_type=item_type;
                    base_state=state;
                }
            }
            if(!base) return false;
            MaterialRecipe debug_recipe;
            if(!BuildDebugViewRecipe(mode,base,debug_recipe)) return false;
            MaterialBuildRequest request{};
            request.recipe=debug_recipe;
            request.primitive_type=primitive_type;
            request.geometry_vertex_format=format;
            ResolvedMaterial definition{};
            if(!ResolveMaterialFromRecipe(debug_recipe,definition)
               || !SelectCurrentScenePassProfileRequest(definition,request,request.render_template_request))
                return false;
            ShaderProgramManager *manager=graphics->GetMaterialManager();
            ShaderProgram *program=manager?manager->AcquireShaderProgram(request):nullptr;
            Pipeline *pipeline=program?pass->CreatePipeline(program,batch->pipeline_config):nullptr;
            if(!pipeline) return false;
            bool mesh=false;
            for(const auto &stage:program->GetStageList())
                if(stage.stage==VK_SHADER_STAGE_MESH_BIT_EXT) mesh=true;
            if(!mesh) return false;
            prepared={batch,program,pipeline};
            return true;
        }
    }

    bool DebugViewPass::RequestMode(DebugViewMode mode)
    {
        if(static_cast<uint32_t>(mode)>=DebugViewModeCount) return false;
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        status.requested=mode;
        return true;
    }

    DebugViewMode DebugViewPass::GetRequestedMode()
    {
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        return status.requested;
    }

    DebugViewMode DebugViewPass::GetAppliedMode()
    {
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        return status.applied;
    }

    uint64_t DebugViewPass::GetTargetID()
    {
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        return status.target_id;
    }

    uint32_t DebugViewPass::GetTrackedCount()
    {
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        return status.entities;
    }

    AnsiString DebugViewPass::GetStatusJSON()
    {
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        AnsiString json="{\"valid\":true,\"mode\":\"";
        json+=GetDebugViewModeName(status.applied);
        json+="\",\"requested_mode\":\"";
        json+=GetDebugViewModeName(status.requested);
        json+="\",\"target_id\":";
        json+=AnsiString::numberOf(status.target_id);
        json+=",\"batches\":";
        json+=AnsiString::numberOf(status.batches);
        json+=",\"tracked_entities\":";
        json+=AnsiString::numberOf(status.entities);
        if(status.error)
        {
            json+=",\"error\":\"";
            json+=status.error;
            json+="\"";
        }
        json+="}";
        return json;
    }

    void DebugViewPass::Record(ECSContext *world,RenderCmdBuffer *cmd,IRenderTarget *main)
    {
        if(!world || !cmd || !main || !main->IsSwapchain()) return;
        auto *swapchain=dynamic_cast<SwapchainRenderTarget *>(main);
        if(!swapchain) return;
        hgl::MutexLock world_lock(&WorldLock());
        DebugViewMode mode;
        {
            Status &status=SharedStatus();
            hgl::MutexLock lock(&status.lock);
            mode=status.requested;
        }
        if(mode==DebugViewMode::Off)
        {
            if(!DropWorld(world))
            {
                WorldState *state=Worlds().GetValuePointer(world);
                DebugCaptureQueue::FailQueued(state->target_id,"target_destroy_failed");
                SetStatus(world,DebugViewMode::Off,state->target_id,0,"target_destroy_failed");
                return;
            }
            ClearStatus(world,false);
            return;
        }
        const uint64_t frame=swapchain->GetDebugCaptureFrameID();
        if(!frame || !swapchain->GetDebugCaptureTargetID())
        {
            if(WorldState *state=Worlds().GetValuePointer(world))
            {
                state->recorded_frame=0;
                state->recorded_main=nullptr;
                DebugCaptureQueue::FailQueued(state->target_id,"debug_bridge_required");
            }
            SetStatus(world,DebugViewMode::Off,0,0,"debug_bridge_required");
            return;
        }
        WorldState *state=Worlds().GetValuePointer(world);
        const VkFormat requested_format=mode==DebugViewMode::WorldPosition
            ?VK_FORMAT_R32G32B32A32_SFLOAT:VK_FORMAT_R8G8B8A8_UNORM;
        if(state && (state->color_format!=requested_format
                     || state->target->GetExtent().width!=main->GetExtent().width
                     || state->target->GetExtent().height!=main->GetExtent().height))
        {
            if(!DropWorld(world))
            {
                DebugCaptureQueue::FailQueued(state->target_id,"target_destroy_failed");
                SetStatus(world,DebugViewMode::Off,state->target_id,0,"target_destroy_failed");
                return;
            }
            state=nullptr;
        }
        if(!state)
        {
            WorldState fresh;
            const char *target_error=nullptr;
            if(!CreateTarget(world,main,mode,fresh,target_error))
            {
                SetStatus(world,DebugViewMode::Off,0,0,target_error?target_error:"target_creation_failed");
                return;
            }
            Worlds().Add(world,fresh);
            state=Worlds().GetValuePointer(world);
        }
        state->recorded_frame=0;
        state->recorded_main=nullptr;
        GraphicsContext *graphics=world->GetGraphicsContext();
        RenderPass *pass=state->target->GetRenderPass();
        ValueArray<PreparedBatch> prepared;
        const char *error=nullptr;
        if(!graphics || !pass) error="render_context_unavailable";
        const CameraInfo *camera=world->GetRenderFrameCache().cameraInfo;
        if(!error && (!camera || camera->use_reversed_z>1
                      || camera!=world->GetActiveCameraInfo()))
            error="unsupported_camera";
        if(!error)
            for(auto &pair:world->GetRenderFrameCache().materialBatches)
            {
                MaterialBatch *batch=pair.second.get();
                if(!batch || batch->items.empty()
                   || batch->pipeline_config.render_layer!=RenderLayer::Scene) continue;
                PreparedBatch entry;
                if(!PrepareBatch(world,graphics,pass,batch,mode,entry))
                {
                    error="unsupported_scene_batch";
                    break;
                }
                prepared.Add(entry);
            }
        if(!error && !main->GetDevice()->WaitIdle())
            error="device_wait_failed";
        if(error)
        {
            DebugCaptureQueue::FailQueued(state->target_id,error);
            SetStatus(world,DebugViewMode::Off,state->target_id,0,error);
            return;
        }
        DebugCaptureQueue::BeginFrame(state->target_id,frame,static_cast<uint32_t>(mode));
        if(!DebugCaptureQueue::CopyCamera(swapchain->GetDebugCaptureTargetID(),state->target_id,frame))
        {
            DebugCaptureQueue::FailQueued(state->target_id,"camera_snapshot_unavailable");
            SetStatus(world,DebugViewMode::Off,state->target_id,0,"camera_snapshot_unavailable");
            return;
        }
        cmd->SetClearColor(0,state->target->GetClearColor());
        RenderPassOptions render_options;
        render_options.clear_depth_value=camera->use_reversed_z?0.0f:1.0f;
        if(!cmd->BeginRendering(state->target,&render_options))
        {
            DebugCaptureQueue::FailQueued(state->target_id,"begin_render_failed");
            SetStatus(world,DebugViewMode::Off,state->target_id,0,"begin_render_failed");
            return;
        }
        for(const PreparedBatch &entry:prepared)
        {
            PipelineMaterialRenderer renderer(entry.program,entry.pipeline);
            renderer.Render(cmd,entry.batch->transform_buffer,entry.batch,
                            world->GetRenderContext(),world->GetActiveCameraRow(),
                            RenderPassSlot::Main);
        }
        cmd->EndRenderingPresent(state->target);
        state->recorded_frame=frame;
        state->recorded_main=main;
        state->recorded_mode=mode;
        state->recorded_batches=prepared.GetCount();
        state->recorded_entities=0;
        for(const PreparedBatch &entry:prepared)
            state->recorded_entities+=static_cast<uint32_t>(entry.batch->items.size());
    }

    void DebugViewPass::ProcessSubmitted(IRenderTarget *main,uint64_t frame_id)
    {
        hgl::MutexLock world_lock(&WorldLock());
        for(auto &[world,state]:Worlds())
        {
            if(state.recorded_main!=main || !frame_id || state.recorded_frame!=frame_id) continue;
            SetStatus(world,state.recorded_mode,state.target_id,state.recorded_batches,nullptr,
                      state.recorded_entities);
            DebugCaptureQueue::Process(state.target,state.target_id,frame_id);
            state.recorded_frame=0;
            state.recorded_main=nullptr;
        }
    }

    void DebugViewPass::SubmissionFailed(IRenderTarget *main,uint64_t frame_id)
    {
        hgl::MutexLock world_lock(&WorldLock());
        for(auto &[world,state]:Worlds())
        {
            if(state.recorded_main!=main || !frame_id || state.recorded_frame!=frame_id) continue;
            state.recorded_frame=0;
            state.recorded_main=nullptr;
            DebugCaptureQueue::FailQueued(state.target_id,"submission_failed");
            SetStatus(world,DebugViewMode::Off,state.target_id,0,"submission_failed");
        }
    }

    void DebugViewPass::Reset(ECSContext *world)
    {
        hgl::MutexLock world_lock(&WorldLock());
        DropWorld(world,true);
        ClearStatus(world,true);
    }
}
