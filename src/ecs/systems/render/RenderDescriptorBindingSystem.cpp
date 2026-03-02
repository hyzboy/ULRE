#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/RenderFrameBusinessSyncSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/vk/VKDescriptorBindingManage.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    RenderDescriptorBindingSystem::RenderDescriptorBindingSystem(const std::string& name)
        : System(name)
    {
        SetExecutionOrder(ExecutionPhase::RenderFrameSync);
        AddDependency<RenderFrameBusinessSyncSystem>();
        AddDependency<EnvironmentSystem>();
        AddDependency<RenderTargetSystem>();
        AddDependency<CameraSystem>();

        RegisterDefaultSources();
    }

    RenderDescriptorBindingSystem::~RenderDescriptorBindingSystem()
    {
        if (view_desc_binding)
        {
            delete view_desc_binding;
            view_desc_binding = nullptr;
        }
    }

    void RenderDescriptorBindingSystem::EnsureViewBinding()
    {
        if (!view_desc_binding)
            view_desc_binding = new graph::DescriptorBinding(graph::DescriptorSetType::Camera);
    }

    void RenderDescriptorBindingSystem::RegisterBindingSource(BindingSource source)
    {
        if (!source)
            return;

        binding_sources.push_back(std::move(source));
    }

    bool RenderDescriptorBindingSystem::GetContractDiagnosticsStats(uint32_t &materials_checked,
                                                                    uint32_t &materials_unresolved,
                                                                    uint32_t &required_missing,
                                                                    uint32_t &optional_missing,
                                                                    uint32_t &fallback_hits) const
    {
        materials_checked = last_contract_stats.materials_checked;
        materials_unresolved = last_contract_stats.materials_unresolved;
        required_missing = last_contract_stats.required_missing;
        optional_missing = last_contract_stats.optional_missing;
        fallback_hits = last_contract_stats.fallback_hits;
        return true;
    }

    void RenderDescriptorBindingSystem::RegisterDefaultSources()
    {
        RegisterBindingSource([](ECSContext *ctx, graph::RenderCmdBuffer *cmd, graph::DescriptorBinding *)
        {
            if (!ctx || !cmd)
                return;

            if (auto *rt = ctx->GetRenderTarget())
            {
                if (auto *db = rt->GetDescriptorBinding())
                    cmd->SetDescriptorBinding(db);
            }
        });

        RegisterBindingSource([](ECSContext *ctx, graph::RenderCmdBuffer *, graph::DescriptorBinding *view_db)
        {
            if (!ctx || !view_db)
                return;

            auto camera_system = ctx->GetSystem<CameraSystem>();
            if (!camera_system)
                return;

            auto *camera_ubo = camera_system->GetCameraUBO();
            if (!camera_ubo)
                return;

            const AnsiString camera_name = camera_ubo->name();
            if (!view_db->GetUBO(camera_name))
                view_db->AddUBO(camera_ubo);
        });

        RegisterBindingSource([](ECSContext *ctx, graph::RenderCmdBuffer *, graph::DescriptorBinding *view_db)
        {
            if (!ctx || !view_db)
                return;

            auto environment_system = ctx->GetSystem<EnvironmentSystem>();
            if (!environment_system)
            {
                environment_system = ctx->RegisterRenderSystem<EnvironmentSystem>();
                if (environment_system && ctx->IsActive())
                {
                    environment_system->OnDependenciesReady();
                    environment_system->Initialize();
                }
            }

            if (!environment_system)
                return;

            environment_system->EditSkyInfo();

            auto *sky_ubo = environment_system->GetSkyUBO();
            if (!sky_ubo)
                return;

            const AnsiString sky_name = sky_ubo->name();
            if (!view_db->GetUBO(sky_name))
                view_db->AddUBO(sky_ubo);
        });
    }

    void RenderDescriptorBindingSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        ValidateContractsSideChannel();

        auto *cmd = context->GetCurrentRenderCmd();
        if (!cmd)
            return;

        EnsureViewBinding();

        for (const auto &source : binding_sources)
        {
            if (source)
                source(context, cmd, view_desc_binding);
        }

        if (view_desc_binding)
            cmd->SetDescriptorBinding(view_desc_binding);

        ApplyContractBindings();
    }

    const graph::IGPUBuffer *RenderDescriptorBindingSystem::ResolveViewportUBO(graph::IRenderTarget *rt,const char *preferred_name) const
    {
        if (!rt)
            return nullptr;

        auto *db = rt->GetDescriptorBinding();
        if (!db)
            return nullptr;

        if (preferred_name && *preferred_name)
        {
            if (const auto *gpu = db->GetUBO(preferred_name))
                return gpu;
        }

        if (const auto *gpu = db->GetUBO("viewport"))
            return gpu;

        return db->GetUBO("ViewportInfo");
    }

    const graph::IGPUBuffer *RenderDescriptorBindingSystem::ResolveCameraUBO() const
    {
        if (!context)
            return nullptr;

        auto camera_system = context->GetSystem<CameraSystem>();
        if (!camera_system)
            return nullptr;

        auto *camera_ubo = camera_system->GetCameraUBO();
        if (!camera_ubo)
            return nullptr;

        return camera_ubo->GetGPUBuffer();
    }

    const graph::IGPUBuffer *RenderDescriptorBindingSystem::ResolveSkyUBO()
    {
        if (!context)
            return nullptr;

        auto environment_system = context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
        {
            environment_system = context->RegisterRenderSystem<EnvironmentSystem>();
            if (environment_system && context->IsActive())
            {
                environment_system->OnDependenciesReady();
                environment_system->Initialize();
            }
        }

        if (!environment_system)
            return nullptr;

        environment_system->EditSkyInfo();

        auto *sky_ubo = environment_system->GetSkyUBO();
        if (!sky_ubo)
            return nullptr;

        return sky_ubo->GetGPUBuffer();
    }

    void RenderDescriptorBindingSystem::ApplyContractBindings()
    {
        if (!context)
            return;

        auto *rt = context->GetRenderTarget();
        const auto *camera_ubo = ResolveCameraUBO();
        const auto *sky_ubo = ResolveSkyUBO();

        const auto &cache = context->GetRenderFrameCache();

        for (const auto &pair : cache.materialBatches)
        {
            graph::Material *material = pair.first.material;
            if (!material)
                continue;

            const auto &contract = material->GetBindingContract();

            for (const auto &req : contract.requirements)
            {
                if (!req.name || !*req.name)
                    continue;

                switch (req.semantic)
                {
                case graph::mtl::DescriptorSemantic::ViewportInfo:
                {
                    const auto *ubo = ResolveViewportUBO(rt, req.name);
                    if (ubo)
                        material->BindUBO(req.set_type, req.name, ubo, false);
                    break;
                }
                case graph::mtl::DescriptorSemantic::CameraInfo:
                {
                    if (camera_ubo)
                        material->BindUBO(req.set_type, req.name, camera_ubo, false);
                    break;
                }
                case graph::mtl::DescriptorSemantic::SkyInfo:
                {
                    if (sky_ubo)
                        material->BindUBO(req.set_type, req.name, sky_ubo, false);
                    break;
                }
                default:
                    break;
                }
            }
        }
    }

    bool RenderDescriptorBindingSystem::IsSemanticResolvable(graph::mtl::DescriptorSemantic semantic) const
    {
        if (!context)
            return false;

        switch (semantic)
        {
        case graph::mtl::DescriptorSemantic::ViewportInfo:
            return context->GetRenderTarget() != nullptr;

        case graph::mtl::DescriptorSemantic::CameraInfo:
        {
            auto camera_system = context->GetSystem<CameraSystem>();
            return camera_system && camera_system->GetCameraUBO();
        }

        case graph::mtl::DescriptorSemantic::SkyInfo:
        {
            auto environment_system = context->GetSystem<EnvironmentSystem>();
            return environment_system && environment_system->GetSkyUBO();
        }

        case graph::mtl::DescriptorSemantic::LocalToWorld:
        case graph::mtl::DescriptorSemantic::MaterialInstance:
        case graph::mtl::DescriptorSemantic::MaterialTexture:
        case graph::mtl::DescriptorSemantic::MaterialSampler:
        case graph::mtl::DescriptorSemantic::Custom:
            return true;

        case graph::mtl::DescriptorSemantic::Unknown:
        default:
            return false;
        }
    }

    void RenderDescriptorBindingSystem::ValidateContractsSideChannel()
    {
        if (!contract_diagnostics_enabled || !context)
            return;

        const auto &cache = context->GetRenderFrameCache();
        ContractDiagStats frame_stats;

        for (const auto &pair : cache.materialBatches)
        {
            const auto &key = pair.first;
            const graph::Material *material = key.material;
            if (!material)
                continue;

            ++frame_stats.materials_checked;

            const auto &contract = material->GetBindingContract();

            bool all_required_ok = true;
            std::string first_error;

            for (const auto &req : contract.requirements)
            {
                const bool resolvable = IsSemanticResolvable(req.semantic);
                if (resolvable)
                    continue;

                if (req.required && !req.allow_fallback)
                {
                    ++frame_stats.required_missing;
                    all_required_ok = false;

                    if (first_error.empty())
                    {
                        first_error = "missing semantic=";
                        first_error += graph::mtl::GetDescriptorSemanticName(req.semantic);
                    }
                }
                else
                {
                    ++frame_stats.optional_missing;

                    if (req.allow_fallback)
                        ++frame_stats.fallback_hits;
                }
            }

            auto it = contract_last_ok.find(material);
            if (it == contract_last_ok.end())
            {
                contract_last_ok.emplace(material, all_required_ok);

                if (!all_required_ok)
                    ++frame_stats.materials_unresolved;

                if (!all_required_ok)
                {
                    LogWarning("[DescriptorContract] material=%s unresolved required contract: %s",
                               material->GetName().c_str(),
                               first_error.c_str());
                }
                continue;
            }

            if (it->second != all_required_ok)
            {
                if (!all_required_ok)
                {
                    LogWarning("[DescriptorContract] material=%s contract changed to unresolved: %s",
                               material->GetName().c_str(),
                               first_error.c_str());
                }
                else
                {
                    LogInfo("[DescriptorContract] material=%s contract resolved", material->GetName().c_str());
                }

                it->second = all_required_ok;
            }

            if (!all_required_ok)
                ++frame_stats.materials_unresolved;
        }

        if (frame_stats != last_contract_stats)
        {
            LogInfo("[DescriptorContract] frame stats: checked=%u unresolved=%u required_missing=%u optional_missing=%u fallback_hits=%u",
                    frame_stats.materials_checked,
                    frame_stats.materials_unresolved,
                    frame_stats.required_missing,
                    frame_stats.optional_missing,
                    frame_stats.fallback_hits);

            last_contract_stats = frame_stats;
        }
    }
}
