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
        uint64_t ECSContext::ResolveTransformDeviceAddress() const
        {
            if (auto ts = GetSystem<TransformSystem>())
                if (auto *tb = ts->GetTransformBuffer())
                    if (auto *l2w_buf = tb->GetTransformDataBuffer())
                        if (auto *l2w_gpu = l2w_buf->GetGPUBuffer())
                            if (auto *device = render_environment.GetGPUDevice())
                                return device->GetBufferDeviceAddressAligned16(l2w_gpu->GetVkDeviceBuffer());
            return 0;
        }

        uint64_t ECSContext::ResolveModelExpansionAddress() const
        {
            if (auto cull = GetSystem<GPUCullSystem>())
                return cull->GetModelExpansionArenaAddress();
            return 0;
        }

        bool ECSContext::SyncGPUResourceAddresses()
        {
            return gpu_resources.SyncWorldAddresses(
                Runner().GetFrameIndex(), ResolveTransformDeviceAddress(), ResolveModelExpansionAddress());
        }

        void ECSContext::SetEnvProfileID(graph::EnvProfileID id, bool take_ownership)
        {
            render_environment.SetEnvProfileID(id, take_ownership);
            SyncGPUResourceAddresses();
        }

        graph::EnvProfileID ECSContext::CreateEnvProfile(const AnsiString &name,
                                                         const graph::EnvironmentInfo &init)
        {
            const graph::EnvProfileID id = render_environment.CreateEnvProfile(name, init);
            if (id != graph::kEnvProfileInvalid)
                SyncGPUResourceAddresses();
            return id;
        }

} // namespace hgl::ecs
