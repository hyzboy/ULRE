#include <hgl/framework/WorkManager.h>
#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/ecs/components/MaterialData.h>
#include <hgl/ecs/components/CameraControlMode.h>
#include <hgl/ecs/components/ShadowProxy.h>
#include <hgl/ecs/core/render/ModelTemplateRenderItem.h>
#include <hgl/ecs/systems/render/collect/RenderCollectSystem.h>
#include <hgl/ecs/systems/render/light_shadow/EnvironmentSystem.h>
#include <hgl/ecs/systems/render/light_shadow/LightSystem.h>
#include <hgl/ecs/systems/render/light_shadow/MainLightShadowSystem.h>
#include <hgl/ecs/systems/render/streaming/TextureStreamingSystem.h>
#include <hgl/ecs/support/cull/GPUCullSystem.h>
#include <hgl/ecs/support/debug/ProfOverlaySystem.h>
#include <hgl/ecs/support/material/ModelTemplateBuilder.h>
#include <hgl/ecs/support/storage/MainFrameTopologySnapshot.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/graph/render/shadow/CascadedShadowController.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/vk/buffer/IGPUBuffer.h>
#include <hgl/vk/VKRenderTargetSwapchain.h>
#include <hgl/vk/VKTextureReadback.h>
#include <hgl/util/hash/FNV1a.h>
#include "../Shared/ExampleAppBase.h"
#include <cstdlib>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    constexpr float PartX(uint32_t part) { return (float(part) - 1.0f) * 0.85f; }
    constexpr float ModelX(uint32_t model) { return (float(model) - 1.0f) * 3.2f; }
}

class GPUModelMultiMaterialApp final : public ulre_example::ExampleAppBase
{
    ECSContext *world = nullptr;
    GlobalSSBOBufferRegistry *registry = nullptr;
    Entity *camera_entity = nullptr;
    Entity *seeds[3]{};
    Entity *models[3]{};
    Entity *probe = nullptr;
    Entity *ground = nullptr;
    Entity *lit_seeds[2]{};
    Geometry *cube = nullptr;
    Geometry *sphere = nullptr;
    mtl::MaterialRecipe recipes[3];
    PrimitiveAsset assets[3];
    PrimitiveAsset sphere_asset;
    PrimitiveAsset lit_assets[2];
    PrimitiveAsset ground_asset;
    mtl::MaterialRecipe lit_recipes[2];
    mtl::MaterialRecipe ground_recipe;
    GlobalSSBOBinding data[3]{};
    ModelTemplatePartInput parts[3];
    uint32_t template_id = 0;
    uint32_t frame = 0;
    uint32_t phase = 0;
    uint32_t phase_samples = 0;
    uint32_t frame_limit = 240;
    uint64_t baseline_hash = 0;
    uint64_t previous_hash = 0;
    bool selfcheck = false;
    bool sample_done = false;
    bool sample_ok = false;
    bool finished = false;
    bool core_only = false;
    bool reverse_creation = false;
    uint64_t placeholder_hash = 0;
    uint32_t placeholder_texture = 0;
    uint32_t warmup = 0;
    uint32_t lit_templates[2]{};
    uint64_t lit_hash = 0, shadow_hash = 0;
    ShaderProgram *lit_program = nullptr;
    uint32_t retired_id = 0, retired_first = 0;
    uint32_t baseline_templates = 0, baseline_parts = 0, baseline_refs[3]{};
    uint32_t lifecycle_cycles = 0, lifecycle_frames = 0;

    MaterialRuntimeRowID EntityRow(const Entity *entity) const
    {
        const auto *slot = world->GetMaterialRuntimeTable()->GetSlot(entity->GetEntityID());
        return slot && slot->valid ? slot->row : INVALID_MATERIAL_RUNTIME_ROW_ID;
    }

    void Fail(const char *reason)
    {
        GLogError(u8"[GMM] FAIL phase=%u frame=%u: %s", phase, frame, reason);
        exit(1);
    }

    MaterialRuntimeRowID SeedRow(uint32_t index) const
    {
        return EntityRow(seeds[index]);
    }

    Entity *CreatePrimitive(const char *name, const math::Vector3f &position, PrimitiveAsset *asset)
    {
        auto *entity = world->CreateEntity<Entity>(name);
        auto transform = world->GetTransform(world->CreateTransform(entity->GetEntityID(), Mobility::Static));
        transform.SetLocalPosition(position);
        world->GetOrCreateGeometryData(entity)->SetPrimitiveAsset(asset);
        entity->AddComponent<ShadowProxy>()->SetCastShadow(false);
        return entity;
    }

    bool Publish()
    {
        for (uint32_t p = 0; p < 3; ++p)
        {
            const uint32_t material = p == 1 ? 1u : 0u;
            parts[p].material.runtime_row = SeedRow(material);
        }
        ModelTemplateBuildStatus status;
        const uint32_t built = BuildMultiMaterialModelTemplate(registry, world, parts, 3u, &status);
        if (status == ModelTemplateBuildStatus::Failed)
            Fail("template build failed");
        if (!built)
            return false;
        template_id = built;
        const auto *binding = world->GetModelTemplateMaterialTable()->FindBinding(template_id);
        if (!binding || binding->groups.GetCount() != 2)
            Fail("expected two real shading groups");
        for (uint32_t n = 0; n < 3; ++n)
        {
            const uint32_t i = reverse_creation ? 2u - n : n;
            world->SetEntityVisible(seeds[i]->GetEntityID(), false);
            models[i] = world->CreateEntity<Entity>("MultiMaterialModel");
            auto transform = world->GetTransform(world->CreateTransform(models[i]->GetEntityID(), Mobility::Static));
            transform.SetLocalPosition({ModelX(i), 0.0f, 0.5f});
            models[i]->AddComponent<GPUModelRefComponent>()->template_id = template_id;
            models[i]->AddComponent<ShadowProxy>();
            if (models[i]->GetComponent<hgl::ecs::GeometryData>())
                Fail("model must not have representative GeometryData");
        }
        for (auto *seed : lit_seeds)
            world->SetEntityVisible(seed->GetEntityID(), false);
        if (phase == 0u && selfcheck && !core_only)
        {
            ReplaceWithLit(0u);
            models[2]->GetComponent<GPUModelRefComponent>()->template_id = template_id;
            const auto *row = world->GetMaterialRuntimeTable()->Get(EntityRow(lit_seeds[0]));
            placeholder_hash = row->material_texture_configuration_hash;
            const auto *model = registry->GetTypedRow<ssbo::ModelTemplateRow>(
                GlobalSSBOType::ModelTemplatePool, lit_templates[0]);
            placeholder_texture = registry->GetTypedRow<ssbo::ModelPartRow>(
                GlobalSSBOType::ModelPartPool, model->part_offset)->texture_id;
            if (lit_recipes[0].textures[0].asset->IsReady())
                Fail("streaming fixture must start with an unresolved texture");
            GLogInfo(u8"[GMM] TEXTURE placeholder template=%u row=%u", lit_templates[0], EntityRow(lit_seeds[0]));
        }
        return true;
    }

    void ReplaceWithLit(uint32_t index)
    {
        if (lit_templates[index])
        {
            models[2]->GetComponent<GPUModelRefComponent>()->template_id = lit_templates[index];
            world->MarkTopologyChanged(TopologyChange::ModelTemplate);
            return;
        }
        ModelTemplatePartInput replacement[3];
        for (uint32_t p = 0; p < 3; ++p)
        {
            replacement[p] = parts[p];
            replacement[p].material.runtime_row = EntityRow(lit_seeds[index]);
            replacement[p].material.can_cast_shadow = true;
        }
        ModelTemplateBuildStatus status;
        const auto built = BuildMultiMaterialModelTemplate(registry, world, replacement, 3u, &status);
        if (!built || status != ModelTemplateBuildStatus::Ready)
            Fail("ready replacement did not publish");
        lit_templates[index] = built;
        models[2]->GetComponent<GPUModelRefComponent>()->template_id = built;
        world->MarkTopologyChanged(TopologyChange::ModelTemplate);
    }

    void CheckPendingReplacement()
    {
        auto *templates = registry->GetPool(GlobalSSBOType::ModelTemplatePool);
        auto *part_pool = registry->GetPool(GlobalSSBOType::ModelPartPool);
        const auto template_count = templates->GetActiveCount();
        const auto part_count = part_pool->GetActiveCount();
        const auto old_id = models[2]->GetComponent<GPUModelRefComponent>()->template_id;
        ModelTemplatePartInput replacement[3];
        for (uint32_t p = 0; p < 3; ++p) replacement[p] = parts[p];
        replacement[1].geometry = nullptr;
        ModelTemplateBuildStatus status;
        if (BuildMultiMaterialModelTemplate(registry, world, replacement, 3u, &status)
         || status != ModelTemplateBuildStatus::Pending)
            Fail("missing geometry must wait without publishing");
        replacement[1] = parts[1];
        replacement[1].material.runtime_row = INVALID_MATERIAL_RUNTIME_ROW_ID;
        if (BuildMultiMaterialModelTemplate(registry, world, replacement, 3u, &status)
         || status != ModelTemplateBuildStatus::Pending
         || templates->GetActiveCount() != template_count || part_pool->GetActiveCount() != part_count
         || models[2]->GetComponent<GPUModelRefComponent>()->template_id != old_id)
            Fail("pending replacement allocated rows or changed the visible template");
        GLogInfo(u8"[GMM] Pending replacements preserved template=%u and pool counts", old_id);
    }

    void CheckFailedPublication()
    {
        auto *templates = registry->GetPool(GlobalSSBOType::ModelTemplatePool);
        auto *part_pool = registry->GetPool(GlobalSSBOType::ModelPartPool);
        const auto template_count = templates->GetActiveCount();
        const auto part_count = part_pool->GetActiveCount();
        const auto *binding = world->GetModelTemplateMaterialTable()->FindBinding(template_id);
        const auto refs = world->GetMaterialRuntimeTable()->GetRefCount(SeedRow(0));
        if (DestroyMultiMaterialModelTemplate(registry, world, template_id)
         || templates->IsPendingRelease(template_id)
         || world->GetModelTemplateMaterialTable()->FindBinding(template_id) != binding)
            Fail("destroying a live shared template must be rejected");

        ValueArray<uint32_t> held;
        const auto available = part_pool->GetRowCapacity() - part_count;
        for (uint32_t i = 0; i < available; ++i)
        {
            const auto row = part_pool->Acquire();
            if (row == ActiveRowPool::InvalidRowID)
                Fail("could not prepare allocation rollback fixture");
            held.Add(row);
        }
        ModelTemplateBuildStatus status;
        const auto failed = BuildMultiMaterialModelTemplate(registry, world, parts, 3u, &status);
        for (const auto row : held)
            if (!part_pool->Release(row))
                Fail("could not release allocation rollback fixture");
        if (failed || status != ModelTemplateBuildStatus::Failed
         || templates->GetActiveCount() != template_count || part_pool->GetActiveCount() != part_count
         || world->GetMaterialRuntimeTable()->GetRefCount(SeedRow(0)) != refs
         || world->GetModelTemplateMaterialTable()->FindBinding(template_id) != binding
         || models[2]->GetComponent<GPUModelRefComponent>()->template_id != template_id)
            Fail("allocation failure changed live template, references or pool counts");
        GLogInfo(u8"[GMM] ROLLBACK live destroy rejected; exhausted part pool preserved published template");
    }

    void RetireInstances()
    {
        const auto *row = registry->GetTypedRow<ssbo::ModelTemplateRow>(
            GlobalSSBOType::ModelTemplatePool, template_id);
        retired_id = template_id;
        retired_first = row->part_offset;
        for (auto *&model : models)
        {
            world->DestroyEntity(model->GetEntityID());
            model = nullptr;
        }
        if (!DestroyMultiMaterialModelTemplate(registry, world, template_id)
         || world->GetModelTemplateMaterialTable()->FindBinding(template_id)
         || !registry->GetPool(GlobalSSBOType::ModelTemplatePool)->IsPendingRelease(template_id))
            Fail("template retirement did not detach and defer GPU rows");
        template_id = 0u;
        lifecycle_frames = 0u;
    }

    void BeginLifecycle()
    {
        world->GetModelTemplateMaterialTable()->CollectRetiredTemplates();
        auto *pool = registry->GetPool(GlobalSSBOType::ModelTemplatePool);
        const auto a = pool->Acquire();
        const auto b = pool->Acquire();
        if (a == ActiveRowPool::InvalidRowID || b == ActiveRowPool::InvalidRowID
         || !pool->ReleaseDeferred(a, 1u, this)
         || !pool->ReleaseDeferred(b, 1u, &lifecycle_cycles)
         || pool->CollectRecyclable(0u, nullptr, this) != 0u
         || !pool->IsActive(a) || !pool->IsActive(b)
         || pool->CollectRecyclable(1u, nullptr, this) != 1u
         || pool->IsActive(a) || !pool->IsActive(b)
         || pool->CollectRecyclable(1u, nullptr, &lifecycle_cycles) != 1u
         || pool->IsActive(b))
            Fail("retirement completion epochs or owner domains crossed");
        baseline_templates = registry->GetPool(GlobalSSBOType::ModelTemplatePool)->GetActiveCount() - 1u;
        baseline_parts = registry->GetPool(GlobalSSBOType::ModelPartPool)->GetActiveCount() - 3u;
        for (uint32_t i = 0; i < 3; ++i)
            baseline_refs[i] = world->GetMaterialRuntimeTable()->GetRefCount(SeedRow(i)) - (i < 2u ? 1u : 0u);
        phase = 17u;
        RetireInstances();
    }

    void RunLifecycle()
    {
        auto *table = world->GetModelTemplateMaterialTable();
        table->CollectRetiredTemplates();
        if (template_id)
        {
            if (++lifecycle_frames >= 4u)
                RetireInstances();
            return;
        }
        auto *templates = registry->GetPool(GlobalSSBOType::ModelTemplatePool);
        auto *part_pool = registry->GetPool(GlobalSSBOType::ModelPartPool);
        if (templates->IsActive(retired_id))
            return;
        if (templates->GetActiveCount() != baseline_templates || part_pool->GetActiveCount() != baseline_parts)
            Fail("template/part active counts did not return to their baseline");
        for (uint32_t i = 0; i < 3; ++i)
            if (world->GetMaterialRuntimeTable()->GetRefCount(SeedRow(i)) != baseline_refs[i])
                Fail("shared runtime material references leaked");
        GLogInfo(u8"[GMM] LIFETIME cycle=%u templates=%u parts=%u refs=%u,%u,%u",
                 lifecycle_cycles, baseline_templates, baseline_parts,
                 baseline_refs[0], baseline_refs[1], baseline_refs[2]);
        if (++lifecycle_cycles < 4u)
        {
            if (!Publish())
                Fail("retired template could not be rebuilt");
            const auto *row = registry->GetTypedRow<ssbo::ModelTemplateRow>(
                GlobalSSBOType::ModelTemplatePool, template_id);
            if (row->part_offset != retired_first)
                Fail("contiguous part rows were not reused after retirement");
            return;
        }
        auto collect = world->GetSystem<RenderCollectSystem>();
        const auto hits = collect ? collect->GetCollectCacheHits() : 0u;
        if (world->IsMainFrameCollectCacheEnabled() && !hits)
            Fail("cache enabled but never replayed");
        GLogInfo(u8"[GMM] CACHE hits=%llu", static_cast<unsigned long long>(hits));
        world->Shutdown();
        if (templates->GetActiveCount() != templates->GetReservedRowCount()
         || part_pool->GetActiveCount() != part_pool->GetReservedRowCount())
            Fail("world shutdown leaked owned GPU templates or parts");
        finished = true;
        GLogInfo(u8"[GMM] PASS stages, pending publication, routes, cache replay and GPU retirement");
        exit(0);
    }

    int ReadColor(const ValueArray<uint8_t> &pixels, const TextureReadbackInfo &info,
                  const math::Vector3f &position)
    {
        const auto *camera = world->GetCameraByEntity(camera_entity);
        const auto clip = camera->camera_info.Project(position);
        if (clip.w <= 0.0f)
            return -1;
        const int x = int((clip.x / clip.w * 0.5f + 0.5f) * float(info.width));
        const int y = int((clip.y / clip.w * 0.5f + 0.5f) * float(info.height));
        if (x < 2 || y < 2 || x + 2 >= int(info.width) || y + 2 >= int(info.height))
            return -1;
        uint32_t channels[3]{};
        for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx)
        {
            const uint32_t offset = (uint32_t(y + dy) * info.width + uint32_t(x + dx)) * 4u;
            if (info.format == PF_BGRA8UN || info.format == PF_RGBA8UN)
            {
                channels[0] += pixels[offset + (info.format == PF_BGRA8UN ? 2 : 0)];
                channels[1] += pixels[offset + 1];
                channels[2] += pixels[offset + (info.format == PF_BGRA8UN ? 0 : 2)];
            }
            else if (info.format == PF_A2BGR10UN || info.format == PF_A2RGB10UN)
            {
                const uint32_t value = *reinterpret_cast<const uint32_t *>(pixels.GetData() + offset);
                const uint32_t c0 = value & 1023u;
                const uint32_t c2 = (value >> 20) & 1023u;
                channels[0] += info.format == PF_A2BGR10UN ? c0 : c2;
                channels[1] += (value >> 10) & 1023u;
                channels[2] += info.format == PF_A2BGR10UN ? c2 : c0;
            }
            else
                return -1;
        }
        for (int c = 0; c < 3; ++c)
            if (channels[c] > 2u * channels[(c + 1) % 3]
             && channels[c] > 2u * channels[(c + 2) % 3])
                return c;
        return -1;
    }

    bool CheckGPU()
    {
        auto cull = world->GetSystem<GPUCullSystem>();
        if (!cull || !cull->SyncForParityReadback())
            return false;
        const auto *counts = cull->MapCurrentBatchCounters(0u);
        if (!counts)
            return false;
        const uint32_t batches = cull->GetBatchCount();
        bool ok = batches >= 2u && counts[batches] == 0u;
        uint32_t histogram[3]{};
        uint32_t total = 0;
        uint32_t lit_texture_count = 0;
        const auto lit_row_id = phase == 12u ? EntityRow(lit_seeds[1]) : EntityRow(lit_seeds[0]);
        const auto *lit_row = world->GetMaterialRuntimeTable()->Get(lit_row_id);
        for (uint32_t b = 0; b < batches; ++b)
        {
            uint32_t base = 0, capacity = 0;
            if (!cull->GetBatchLayout(0u, b, base, capacity) || counts[b] > capacity)
            {
                ok = false;
                break;
            }
            const uint32_t count = counts[b];
            if (!count)
                continue;
            auto *buffer = cull->GetMtlOutBuffer();
            const auto *rows = static_cast<const mtl::MaterialInstanceAddresses *>(
                buffer->GetGPUBuffer()->Map(uint64_t(base) * sizeof(mtl::MaterialInstanceAddresses),
                            uint64_t(count) * sizeof(mtl::MaterialInstanceAddresses)));
            if (!rows)
            {
                ok = false;
                break;
            }
            for (uint32_t r = 0; r < count; ++r)
            {
                bool found = false;
                for (uint32_t c = 0; c < 3; ++c)
                    if (rows[r].payload_index == data[c].data_index)
                    {
                        ++histogram[c];
                        found = true;
                        break;
                    }
                if (phase <= 8u || phase == 13u)
                    ok &= found;
                if (lit_row && rows[r].texture_reference_index == lit_row->material_texture_configuration.row_index)
                    ++lit_texture_count;
            }
            buffer->GetGPUBuffer()->Unmap();
            total += count;
        }
        cull->UnmapCurrentBatchCounters();
        uint32_t expected[3]{7u, 3u, 0u};
        if (phase == 1) { expected[0] = 5; expected[1] = 2; expected[2] = 3; }
        if (phase == 2) { expected[0] = 3; expected[1] = 4; expected[2] = 3; }
        if (phase == 3) { expected[0] = 3; expected[1] = 4; expected[2] = 2; }
        if (phase == 5 || phase == 6) { expected[0] = 6; expected[1] = 3; expected[2] = 1; }
        if (phase <= 8u || phase == 13u)
            for (uint32_t c = 0; c < 3; ++c)
                ok &= histogram[c] == expected[c];
        else
            ok &= total == 11u;
        if (phase >= 9u && phase <= 12u)
            ok &= lit_row && lit_row->material_texture_configuration.IsValid()
               && lit_texture_count == 3u;
        if (phase == 9u)
        {
            const auto *model = registry->GetTypedRow<ssbo::ModelTemplateRow>(
                GlobalSSBOType::ModelTemplatePool, lit_templates[0]);
            const auto *part = registry->GetTypedRow<ssbo::ModelPartRow>(
                GlobalSSBOType::ModelPartPool, model->part_offset);
            ok &= lit_row && lit_row->material_texture_configuration_hash != placeholder_hash
               && part->texture_id == placeholder_texture;
        }
        GLogInfo(u8"[GMM] GPU phase=%u buckets=%u parts=%u material_hist=%u,%u,%u expected=%u,%u,%u",
                 phase, batches, total, histogram[0], histogram[1], histogram[2],
                 expected[0], expected[1], expected[2]);
        return ok;
    }

    void Sample()
    {
        ValueArray<uint8_t> pixels;
        TextureReadbackInfo info;
        bool ok = ReadbackColorTarget(world->GetRenderTarget(), pixels, 0, &info) && !pixels.IsEmpty();
        if (ok)
        {
            for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t p = 0; p < 3; ++p)
            {
                if ((phase == 3 && i == 2 && p == 1)
                 || (phase >= 9u && phase != 13u && i == 2u))
                    continue;
                int expected = p == 1 ? 1 : 0;
                if (phase >= 1 && phase <= 3 && i == 2) expected = 2;
                if (phase >= 2 && phase <= 3 && i == 1) expected = 1;
                const int actual = ReadColor(pixels, info, {ModelX(i) + PartX(p), 0.0f, 0.5f});
                if (actual != expected)
                {
                    GLogError(u8"[GMM] ROI phase=%u model=%u part=%u actual=%d expected=%d", phase, i, p, actual, expected);
                    ok = false;
                }
            }
            ok &= ReadColor(pixels, info, {0.0f, 0.0f, 2.2f}) == ((phase == 5 || phase == 6) ? 2 : 0);
            hash::FNV1aHasher64 hasher;
            hasher.AppendBytes(pixels.GetData(), pixels.GetCount());
            const uint64_t digest = hasher;
            if (phase_samples && digest != previous_hash)
                ok = false;
            previous_hash = digest;
            if (phase == 0) baseline_hash = digest;
            if (phase == 4 || phase == 7 || phase == 8 || phase == 13) ok &= digest == baseline_hash;
            if (phase == 9) lit_hash = digest;
            if (phase == 10 || phase == 12) ok &= digest != lit_hash;
            if (phase == 11) ok &= digest == lit_hash;
            if (phase == 14) shadow_hash = digest;
            if (phase == 15) ok &= digest != shadow_hash;
            if (phase == 16) ok &= digest == shadow_hash;
            if (phase >= 14u)
            {
                ValueArray<uint8_t> depth;
                TextureReadbackInfo depth_info;
                auto shadows = world->GetSystem<MainLightShadowSystem>();
                if (!shadows || !ReadbackDepthTarget(shadows->GetCascadeRenderTarget(0u), depth, &depth_info)
                 || depth_info.format != PF_D32F)
                    ok = false;
                else
                {
                    hash::FNV1aHasher64 depth_hasher;
                    depth_hasher.AppendBytes(depth.GetData(), depth.GetCount());
                    uint32_t written = 0;
                    const auto *values = reinterpret_cast<const float *>(depth.GetData());
                    for (int i = 0; i < depth.GetCount() / int(sizeof(float)); ++i)
                        written += values[i] > 0.0f && values[i] < 1.0f;
                    GLogInfo(u8"[GMM] SHADOW phase=%u hash=%016llx written=%u",
                             phase, static_cast<unsigned long long>(uint64_t(depth_hasher)), written);
                    ok &= phase == 15u ? written == 0u : written > 0u;
                }
            }
            if (phase >= 9u && phase <= 11u)
            {
                ShaderProgram *program = nullptr;
                for (const auto &item : world->GetRenderFrameCache().renderItems)
                    if (item->GetEntity() == models[2])
                        program = item->GetShaderProgram();
                if (phase == 9u) lit_program = program;
                ok &= program && (phase == 10u ? program != lit_program : program == lit_program);
            }
            GLogInfo(u8"[GMM] FRAME phase=%u sample=%u hash=%016llx", phase, phase_samples,
                     static_cast<unsigned long long>(digest));
            if (!phase_samples)
            {
                const auto &snapshot = world->GetMainFrameTopologySnapshot();
                const auto &ledger = world->GetPrimitiveTopologyLedger();
                GLogInfo(u8"[GMM] CACHE_STATE steady=%d gpu=%d ready=%d items=%d/%u revision=%llu/%llu hits=%llu fallbacks=%llu",
                         world->GetRenderFrameCache().is_collect_steady, ledger.GetLastGPUCullEngaged(),
                         snapshot.IsReady(), snapshot.GetItemCount(), snapshot.GetCollectedItemCount(),
                         static_cast<unsigned long long>(snapshot.GetTopologyRevision()),
                         static_cast<unsigned long long>(ledger.GetTopologyRevision()),
                         static_cast<unsigned long long>(world->GetSystem<RenderCollectSystem>()->GetCollectCacheHits()),
                         static_cast<unsigned long long>(world->GetSystem<RenderCollectSystem>()->GetCollectCacheFallbacks()));
            }
            ok &= CheckGPU();
        }
        sample_ok = ok;
        if (!ok && !pixels.IsEmpty())
        {
            GLogError(u8"[GMM] Readback width=%u height=%u format=%u", info.width, info.height, uint32_t(info.format));
            if (const char *path = getenv("GMM_DUMP"))
                filesystem::SaveMemoryToFile(ToOSString(AnsiString(path)), pixels.GetData(), pixels.GetCount());
        }
        sample_done = true;
    }

    void Advance()
    {
        if (!sample_ok)
            Fail("GPU data or per-part pixel validation failed");
        if (++phase_samples < 3u)
            return;
        phase_samples = 0;
        ++phase;
        warmup = 2u;
        auto *b = models[1]->GetComponent<GPUModelRefComponent>();
        auto *c = models[2]->GetComponent<GPUModelRefComponent>();
        if (phase == 1 && !c->SetOverrideMaterialRow(SeedRow(2))) Fail("blue override");
        if (phase == 2 && !b->SetOverrideMaterialRow(SeedRow(1))) Fail("green override");
        if (phase == 3) c->HidePart(1u);
        if (phase == 4)
        {
            b->ClearOverrideMaterialRow();
            c->ClearOverrideMaterialRow();
            c->ShowAllParts();
        }
        if (phase == 5) world->GetOrCreateMaterialData(probe)->SetRecipe(recipes[2]);
        if (phase == 6) world->GetOrCreateGeometryData(probe)->SetPrimitiveAsset(&sphere_asset);
        if (phase == 7)
        {
            world->GetOrCreateMaterialData(probe)->ClearRecipeOverride();
            world->GetOrCreateGeometryData(probe)->SetPrimitiveAsset(&assets[0]);
        }
        if (phase == 8)
        {
            CheckPendingReplacement();
            CheckFailedPublication();
            if (core_only) { BeginLifecycle(); return; }
            for (auto &recipe : lit_recipes)
                recipe.textures[0].asset->ClearAsyncRequest();
            world->GetSystem<TextureStreamingSystem>()->SetEnabled(true);
        }
        if (phase == 9) { ReplaceWithLit(0u); world->SetEntityVisible(ground->GetEntityID(), true); }
        if (phase == 10 && !world->GetSystem<EnvironmentSystem>()->SetAmbientProvider("ambient_light_none"))
            Fail("ambient provider switch rejected");
        if (phase == 11 && !world->GetSystem<EnvironmentSystem>()->SetAmbientProvider("ambient_light_sh"))
            Fail("ambient provider restoration rejected");
        if (phase == 12) ReplaceWithLit(1u);
        if (phase == 13)
        {
            c->template_id = template_id;
            world->SetEntityVisible(ground->GetEntityID(), false);
            for (auto &id : lit_templates)
            {
                if (!DestroyMultiMaterialModelTemplate(registry, world, id))
                    Fail("replacement template retirement rejected");
                id = 0u;
            }
        }
        if (phase == 14) { ReplaceWithLit(0u); world->SetEntityVisible(ground->GetEntityID(), true); }
        if (phase == 15) models[2]->GetComponent<ShadowProxy>()->SetCastShadow(false);
        if (phase == 16) models[2]->GetComponent<ShadowProxy>()->SetCastShadow(true);
        if (phase == 17)
        {
            c->template_id = template_id;
            if (!DestroyMultiMaterialModelTemplate(registry, world, lit_templates[0]))
                Fail("shadow template retirement rejected");
            world->SetEntityVisible(ground->GetEntityID(), false);
            BeginLifecycle();
        }
    }

    bool OnInit() override
    {
        world = GetECSContext();
        registry = GetManager<GlobalSSBOBufferRegistry>();
        if (!world || !registry) return false;
        selfcheck = getenv("GMM_SELFCHECK") != nullptr;
        reverse_creation = getenv("GMM_REVERSE_CREATE") != nullptr;
        if (const char *value = getenv("GMM_CASE"))
        {
            core_only = strcmp(value, "core") == 0;
            if (!core_only && strcmp(value, "all") != 0)
                return false;
        }
        if (selfcheck && !core_only)
            world->GetSystem<TextureStreamingSystem>()->SetEnabled(false);
        if (selfcheck)
            if (auto overlay = world->GetSystem<ProfOverlaySystem>())
                overlay->Disable();
        if (const char *value = getenv("GMM_FRAMES"))
        {
            const int limit = atoi(value);
            if (limit <= 0) return false;
            frame_limit = uint32_t(limit);
        }
        SetClearColor(Color4f(0.025f, 0.025f, 0.025f, 1.0f));
        world->SetScenePipelineMode(ScenePipelineMode::StandardLitCSM);
        auto environment = world->GetSystem<EnvironmentSystem>();
        if (!environment || !environment->SetAmbientProvider("ambient_light_sh"))
            return false;
        auto *ambient = environment->EditAmbientEnvInfo();
        if (!ambient) return false;
        ambient->sh[0] = {0.35f, 0.35f, 0.35f, 0.0f};
        environment->MarkAmbientEnvDirty();
        auto light = world->GetSystem<LightSystem>();
        if (!light) light = world->RegisterSystem<LightSystem>();
        light->SetMainLightDirection({0.55f, -0.75f, 0.8f});
        light->SetMainLightIntensity(2.0f);
        auto shadows = world->GetSystem<MainLightShadowSystem>();
        if (!shadows) shadows = world->RegisterSystem<MainLightShadowSystem>();
        CascadedShadowConfig shadow_config;
        shadow_config.cascade_count = 1;
        shadow_config.max_distance = 30.0f;
        shadow_config.split_distances[0] = 30.0f;
        shadow_config.use_custom_splits = true;
        shadow_config.c0_dynamic_overlay = false;
        shadow_config.bias_world = 0.01f;
        if (!shadows->EnableMainLightShadow(shadow_config, 512u)) return false;
        GeometryVertexFormat format{{VertexSemantic::Position, VF_V3F}, {VertexSemantic::Normal, VF_V3F},
                                    {VertexSemantic::TexCoord, VF_V2F}};
        cube = GetGeometry("geo:builtin:cube", &format);
        sphere = GetGeometry("geo:builtin:sphere_64", &format);
        if (!cube || !sphere) return false;
        cube->EnsureMeshDrawParams(registry, GetDevice());
        sphere->EnsureMeshDrawParams(registry, GetDevice());
        const auto *solid = GetRecipe("rcp:pack(ShaderLibrary):asset/recipe/Examples/Instancing/GPUModelMultiMaterial.Solid");
        const auto *double_sided = GetRecipe("rcp:pack(ShaderLibrary):asset/recipe/Examples/Instancing/GPUModelMultiMaterial.DoubleSided");
        if (!solid || !double_sided) return false;
        for (uint32_t i = 0; i < 3; ++i)
        {
            if (!GetMaterialDataBinding("mtldata:pack(ShaderLibrary):asset/material_data/gpu_model_multi_material", i, data[i]))
                return false;
            recipes[i] = i == 1 ? *double_sided : *solid;
            recipes[i].material_ssbo_binding = data[i];
            assets[i] = PrimitiveAsset(cube, &recipes[i], PrimitiveType::Triangles);
            seeds[i] = CreatePrimitive("MaterialPreparation", {1000.0f + float(i) * 2.0f, 0.0f, 0.0f}, &assets[i]);
            parts[i].geometry = cube;
            parts[i].material.can_cast_shadow = false;
            parts[i].local_bounds = cube->GetBoundingVolumes().aabb;
            parts[i].local_matrix = glm::translate(math::Matrix4f(1.0f), math::Vector3f(PartX(i), 0.0f, 0.0f))
                                  * glm::scale(math::Matrix4f(1.0f), math::Vector3f(0.65f, 0.55f, 0.8f));
        }
        sphere_asset = PrimitiveAsset(sphere, &recipes[2], PrimitiveType::Triangles);
        probe = CreatePrimitive("DynamicPrimitive", {0.0f, 0.0f, 2.2f}, &assets[0]);
        GlobalSSBOBinding lit_data;
        if (!GetMaterialDataBinding("mtldata:pack(ShaderLibrary):asset/material_data/dual_world_shadows", 0u, lit_data))
            return false;
        const char *lit_uris[]{
            "rcp:pack(ShaderLibrary):asset/recipe/Examples/Instancing/GPUModelMultiMaterial.LitBrick",
            "rcp:pack(ShaderLibrary):asset/recipe/Examples/Instancing/GPUModelMultiMaterial.LitConcrete"};
        for (uint32_t i = 0; i < 2; ++i)
        {
            const auto *recipe = GetRecipe(lit_uris[i]);
            if (!recipe) return false;
            lit_recipes[i] = *recipe;
            if (selfcheck && !core_only)
                lit_recipes[i].textures[0].asset->FailAsyncLoad("GMM expected placeholder retry fixture");
            lit_recipes[i].material_ssbo_binding = lit_data;
            lit_assets[i] = PrimitiveAsset(cube, &lit_recipes[i], PrimitiveType::Triangles);
            lit_seeds[i] = CreatePrimitive("LitPreparation", {1100.0f + float(i) * 2.0f, 0.0f, 0.0f}, &lit_assets[i]);
        }
        const auto *ground_source = GetRecipe("rcp:pack(ShaderLibrary):asset/recipe/Examples/Shadow/DualWorldShadows.Lit");
        if (!ground_source) return false;
        ground_recipe = *ground_source;
        ground_recipe.material_ssbo_binding = lit_data;
        ground_asset = PrimitiveAsset(cube, &ground_recipe, PrimitiveType::Triangles);
        ground = CreatePrimitive("ShadowReceiver", {0.0f, 0.0f, -0.12f}, &ground_asset);
        world->GetTransformByEntity(ground->GetEntityID()).SetLocalScale({14.0f, 8.0f, 0.1f});
        world->SetEntityVisible(ground->GetEntityID(), false);
        if (!world->EnsureCameraSystem()) return false;
        camera_entity = world->CreateEntity<Entity>("MainCamera");
        auto *camera = world->GetOrCreateCamera(camera_entity);
        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = {0.0f, 0.0f, 0.8f};
        camera->distance = 12.0f;
        camera->yaw = 90.0f;
        camera->pitch = -12.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;
        camera->viewport_info = GetViewportInfo();
        return true;
    }

    void OnPreTick(double) override
    {
        ++frame;
        if (sample_done)
        {
            sample_done = false;
            Advance();
        }
        if (phase >= 17u)
            RunLifecycle();
        else if (!template_id)
        {
            ModelTemplateBuildStatus status;
            const uint32_t pending = BuildMultiMaterialModelTemplate(registry, world, parts, 3u, &status);
            if (pending || status != ModelTemplateBuildStatus::Pending)
                Fail("unprepared material must not publish a template");
            if (SeedRow(0) != INVALID_MATERIAL_RUNTIME_ROW_ID
             && SeedRow(1) != INVALID_MATERIAL_RUNTIME_ROW_ID
             && SeedRow(2) != INVALID_MATERIAL_RUNTIME_ROW_ID
             && EntityRow(lit_seeds[0]) != INVALID_MATERIAL_RUNTIME_ROW_ID
             && EntityRow(lit_seeds[1]) != INVALID_MATERIAL_RUNTIME_ROW_ID)
                Publish();
        }
        if (frame >= frame_limit)
        {
            if (selfcheck && !finished) Fail("frame budget exhausted before acceptance completed");
            exit(0);
        }
    }

    void OnTick(double) override
    {
        if (selfcheck && template_id && phase < 17u)
        {
            if (phase >= 9u && (!lit_recipes[0].textures[0].asset->IsReady()
                            || !lit_recipes[1].textures[0].asset->IsReady()))
                return;
            if (warmup) { --warmup; return; }
            auto *rt = dynamic_cast<SwapchainRenderTarget *>(world->GetRenderTarget());
            if (!rt) Fail("self-check requires a swapchain");
            rt->SetInFrameReadbackHook([this]() { Sample(); });
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<GPUModelMultiMaterialApp>(
        OS_TEXT("GPU multi-material model regression"), argc, argv, 1280, 720);
}
