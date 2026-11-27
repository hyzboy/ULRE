#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/support/storage/CameraInfoStorage.h>
#include <hgl/ecs/support/transform/TransformAssignmentBuffer.h>
#include <hgl/ecs/systems/tick/TransformSystem.h>
#include <hgl/ecs/support/cull/GPUCullSystem.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/module/EnvironmentManager.h>
#include <hgl/graph/ubo/WorldAddresses.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/vk/buffer/StructView.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/log/Log.h>
#include <cstring>
#include <cstdio>
#include <vector>

namespace hgl::ecs
{
        graph::GraphicsContext *ECSContext::GPUResourcesGraphicsContext()
        {
            return GetGraphicsContext();
        }

        graph::VulkanDevice *ECSContext::GPUResourcesDevice()
        {
            return GetGPUDevice();
        }

        const AnsiString &ECSContext::GPUResourcesWorldName() const
        {
            return GetName();
        }

        EntityGPUTable &ECSContext::GPUResourcesEntityTable()
        {
            return entity_render_state.EntityGPU();
        }

        EntityRenderIDTable &ECSContext::GPUResourcesRenderIDTable()
        {
            return entity_render_state.EntityRenderIDs();
        }

        LightRowTable &ECSContext::GPUResourcesLightTable()
        {
            return RenderData().Lights();
        }

        const graph::ShadowInfo &ECSContext::GPUResourcesShadowInfo() const
        {
            return render_data.Shadow();
        }

        CameraInfoStorage *ECSContext::GPUResourcesCameraStorage()
        {
            return render_data.CameraRows();
        }

        uint64_t ECSContext::GPUResourcesTransformAddress()
        {
            if (auto ts = GetSystem<TransformSystem>())
                if (auto *tb = ts->GetTransformBuffer())
                    if (auto *l2w_buf = tb->GetTransformDataBuffer())
                        if (auto *l2w_gpu = l2w_buf->GetGPUBuffer())
                            if (auto *device = GetGPUDevice())
                                return device->GetBufferDeviceAddressAligned16(l2w_gpu->GetVkDeviceBuffer());
            return 0;
        }

        uint32_t ECSContext::GPUResourcesEnvironmentProfile() const
        {
            return GetEnvProfileID();
        }

        uint64_t ECSContext::GPUResourcesAmbientAddress(const uint32_t profile) const
        {
            auto *manager = ResolveEnvManager();
            return manager ? manager->GetAmbientEnvAddress(profile) : 0;
        }

        uint64_t ECSContext::GPUResourcesModelExpansionAddress()
        {
            if (auto cull = GetSystem<GPUCullSystem>())
                return cull->GetModelExpansionArenaAddress();
            return 0;
        }

        graph::EnvProfileID ECSContext::GetEnvProfileID() const
        {
            return render_environment.GetEnvProfileID();
        }

        void ECSContext::SetEnvProfileID(graph::EnvProfileID id, bool take_ownership)
        {
            // 换 profile 前先把**旧的、本世界拥有的**那个归还（否则换一次就漏一个 profile）
            if (render_environment.IsEnvProfileOwned() &&
                render_environment.GetExplicitEnvProfileID() != id)
                if (auto *manager = ResolveEnvManager())
                    manager->Release(render_environment.GetExplicitEnvProfileID());

            render_environment.SetEnvProfile(id, take_ownership);

            // 立刻重算本世界表的 sky / shadow 地址（不等到下一帧）：换 profile 的那一帧就生效
            gpu_resources.SyncWorldAddresses(GetFrameIndex());
        }

        graph::EnvProfileID ECSContext::CreateEnvProfile(const AnsiString &name,
                                                         const graph::EnvironmentInfo &init)
        {
            auto *manager = ResolveEnvManager();
            if (!manager)
                return graph::kEnvProfileInvalid;

            const graph::EnvProfileID id = manager->Create(name, init);
            if (id == graph::kEnvProfileInvalid)
                return id;

            SetEnvProfileID(id, true);      // 本世界创建的 ⇒ Shutdown 时归还
            return id;
        }

        const AnsiString& ECSContext::GetAmbientProviderName() const
        {
            static const AnsiString empty;

            auto *manager = ResolveEnvManager();
            if (!manager)
                return empty;

            return manager->GetAmbientProvider(GetEnvProfileID());
        }

        graph::EnvironmentManager* ECSContext::ResolveEnvManager() const
        {
            graph::GraphicsContext *gc = nullptr;
            if (auto *rc = const_cast<ECSContext *>(this)->GetRenderContext())
                gc = rc->GetGraphicsContext();
            if (!gc)
                gc = const_cast<ECSContext *>(this)->GetGraphicsContext();

            return gc ? gc->GetEnvironmentManager() : nullptr;
        }

} // namespace hgl::ecs