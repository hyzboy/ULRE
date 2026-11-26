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

        bool ECSContext::InitializeWorldAddressesTable()
        {
            return gpu_resources.InitializeWorldAddressesTable();
        }

        void ECSContext::UploadEntityGPUDirtyRows(const uint32_t frame_slot)
        {
            gpu_resources.UploadEntityGPUDirtyRows(frame_slot);
        }

        void ECSContext::UploadEntityRenderIDDirtyRows()
        {
            gpu_resources.UploadEntityRenderIDDirtyRows();
        }

        void ECSContext::UploadLightRowDirtyRows()
        {
            gpu_resources.UploadLightRowDirtyRows();
        }

        bool ECSContext::InitializeEntityRenderIDTable()
        {
            return gpu_resources.InitializeEntityRenderIDTable();
        }

        bool ECSContext::InitializeEntityGPUTable()
        {
            return gpu_resources.InitializeEntityGPUTable();
        }

        bool ECSContext::InitializeLightRowTable()
        {
            return gpu_resources.InitializeLightRowTable();
        }

        bool ECSContext::InitializeShadowInfoTable()
        {
            return gpu_resources.InitializeShadowInfoTable();
        }

        void ECSContext::CommitShadowInfo(const uint32_t frame_slot)
        {
            gpu_resources.CommitShadowInfo(frame_slot);
        }

        uint64_t ECSContext::GetWorldAddressesAddress(const uint32_t frame_slot) const
        {
            return gpu_resources.GetWorldAddressesAddress(frame_slot);
        }

        graph::EnvProfileID ECSContext::GetEnvProfileID() const
        {
            if (env_profile_explicit)
                return env_profile;

            // 未显式设置 ⇒ 跟随**本世界 RT**（作者侧 SetEnvironmentProfile 可能发生在世界创建之后，
            // 所以这里每帧解析而不是初始化时取一次）
            if (render_target)
                return render_target->GetEnvironmentProfile();

            return graph::kEnvProfileDefault;
        }

        void ECSContext::SetEnvProfileID(graph::EnvProfileID id, bool take_ownership)
        {
            // 换 profile 前先把**旧的、本世界拥有的**那个归还（否则换一次就漏一个 profile）
            if (env_profile_owned && env_profile != id)
                if (auto *manager = ResolveEnvManager())
                    manager->Release(env_profile);

            env_profile = id;
            env_profile_explicit = true;
            env_profile_owned = take_ownership;

            // 立刻重算本世界表的 sky / shadow 地址（不等到下一帧）：换 profile 的那一帧就生效
            SyncWorldAddresses();
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

        bool ECSContext::SyncWorldAddresses()
        {
            return gpu_resources.SyncWorldAddresses(frame_index);
        }
} // namespace hgl::ecs