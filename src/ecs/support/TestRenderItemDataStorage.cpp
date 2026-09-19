#include <hgl/ecs/support/RenderItemDataStorage.h>
#include <hgl/ecs/support/DrawItemIDStorage.h>
#include <hgl/ecs/support/DrawItemCompaction.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/PrimitiveComponent.h>
#include <hgl/ecs/components/InstancedPrimitiveComponent.h>
#include <hgl/graph/ubo/GlobalAddresses.h>
#include <hgl/ShaderCompilerAPI.h>
#include <vulkan/vulkan.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::ecs;

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    hgl::logger::InitLogger(OS_TEXT("TestRenderItemDataStorage"));

    GLogInfo(u8"=== Testing RenderItemDataStorage (Stage 1 Infrastructure) ===");

    RenderItemDataStorage storage;

    // Test 1: Single allocation and initial values
    auto h0 = storage.Allocate();
    auto h1 = storage.Allocate();
    auto h2 = storage.Allocate();

    if (h0 != 0 || h1 != 1 || h2 != 2)
    {
        GLogError(u8"Test 1 Failed: Expected handles 0, 1, 2, got %u, %u, %u", h0, h1, h2);
        return 1;
    }

    if (storage.GetCount() != 3 || storage.GetActiveCount() != 3)
    {
        GLogError(u8"Test 1 Failed: Count mismatch, count=%u, active=%u",
                  storage.GetCount(),
                  storage.GetActiveCount());
        return 2;
    }

    // Test 2: Field setting and retrieval
    storage.Set4ID(h0, 100, 200, 300, 400);
    const auto *d0 = storage.Get(h0);
    if (!d0 || d0->transform_id != 100 || d0->geometry_id != 200 || d0->material_id != 300 || d0->texture_id != 400)
    {
        GLogError(u8"Test 2 Failed: Set4ID mismatch on h0");
        return 3;
    }

    // Test 3: FreeList slot reuse
    if (!storage.Release(h1))
    {
        GLogError(u8"Test 3 Failed: Release(h1) returned false");
        return 4;
    }
    if (storage.GetActiveCount() != 2 || storage.GetFreeCount() != 1)
    {
        GLogError(u8"Test 3 Failed: Active/Free count mismatch after release");
        return 5;
    }

    auto h_reused = storage.Allocate();
    if (h_reused != h1)
    {
        GLogError(u8"Test 3 Failed: Expected reused handle %u, got %u", h1, h_reused);
        return 6;
    }

    // Test 4: Contiguous block allocation
    const uint32_t block_count = 50;
    auto base_handle = storage.AllocateContiguous(block_count);
    if (base_handle != 3)
    {
        GLogError(u8"Test 4 Failed: Expected contiguous base 3, got %u", base_handle);
        return 7;
    }

    for (uint32_t i = 0; i < block_count; ++i)
    {
        storage.Set4ID(base_handle + i, 1000 + i, 2000 + i, 3000 + i, 4000 + i);
    }

    for (uint32_t i = 0; i < block_count; ++i)
    {
        const auto *d = storage.Get(base_handle + i);
        if (!d || d->transform_id != 1000 + i || d->geometry_id != 2000 + i)
        {
            GLogError(u8"Test 4 Failed: Block item %u verification failed", i);
            return 8;
        }
    }

    // Test 5: Dirty range tracking
    storage.ClearDirty();
    if (storage.IsDirty())
    {
        GLogError(u8"Test 5 Failed: Storage should not be dirty after ClearDirty()");
        return 9;
    }

    storage.SetMaterialID(base_handle + 10, 9999);
    storage.SetTextureID(base_handle + 20, 8888);

    uint32_t dirty_min = 0, dirty_max = 0;
    if (!storage.GetDirtyRange(dirty_min, dirty_max))
    {
        GLogError(u8"Test 5 Failed: GetDirtyRange returned false when dirty");
        return 10;
    }

    if (dirty_min != base_handle + 10 || dirty_max != base_handle + 20)
    {
        GLogError(u8"Test 5 Failed: Dirty range mismatch, expected [%u, %u], got [%u, %u]",
                  base_handle + 10,
                  base_handle + 20,
                  dirty_min,
                  dirty_max);
        return 11;
    }

    // Test 6: Contiguous release
    if (!storage.ReleaseContiguous(base_handle, block_count))
    {
        GLogError(u8"Test 6 Failed: ReleaseContiguous returned false");
        return 12;
    }

    if (storage.GetFreeCount() != block_count)
    {
        GLogError(u8"Test 6 Failed: Free count expected %u, got %u", block_count, storage.GetFreeCount());
        return 13;
    }

    // Test 7: Stage 2 Integration - PrimitiveComponent & ECSContext
    GLogInfo(u8"--- Testing Stage 2: PrimitiveComponent & ECSContext 4-ID Integration ---");
    {
        ECSContext context("TestContext");
        auto *world_storage = context.GetRenderItemStorage();
        if (!world_storage)
        {
            GLogError(u8"Test 7 Failed: Context RenderItemStorage is null");
            return 14;
        }

        auto entity = context.CreateEntity("TestRenderableEntity");
        auto prim_comp = entity->AddComponent<PrimitiveComponent>();

        auto handle = prim_comp->GetRenderItemHandle();
        if (handle == INVALID_RENDER_ITEM_HANDLE)
        {
            GLogError(u8"Test 7 Failed: Expected valid RenderItemHandle after AddComponent");
            return 15;
        }

        prim_comp->Set4ID(111, 222, 333, 444);

        if (prim_comp->GetTransformID() != 111 ||
            prim_comp->GetGeometryID()  != 222 ||
            prim_comp->GetMaterialID()  != 333 ||
            prim_comp->GetTextureID()   != 444)
        {
            GLogError(u8"Test 7 Failed: Component 4-ID getters mismatch");
            return 16;
        }

        const auto *storage_desc = world_storage->Get(handle);
        if (!storage_desc ||
            storage_desc->transform_id != 111 ||
            storage_desc->geometry_id  != 222 ||
            storage_desc->material_id  != 333 ||
            storage_desc->texture_id   != 444)
        {
            GLogError(u8"Test 7 Failed: Storage 4-ID mismatch from component update");
            return 17;
        }

        // Test component detachment releases the slot
        const uint32_t active_before = world_storage->GetActiveCount();
        entity->RemoveComponent<PrimitiveComponent>();

        if (world_storage->GetActiveCount() != active_before - 1)
        {
            GLogError(u8"Test 7 Failed: Active count did not decrement on RemoveComponent");
            return 18;
        }

        // Verify slot reuse by adding another primitive component
        auto prim_comp2 = entity->AddComponent<PrimitiveComponent>();
        auto handle2 = prim_comp2->GetRenderItemHandle();
        if (handle2 != handle)
        {
            GLogError(u8"Test 7 Failed: Expected reused handle %u, got %u", handle, handle2);
            return 19;
        }
    }

    // Test 8: Stage 3 Verification - GlobalAddresses UBO Layout & Shader BDA Compilation
    GLogInfo(u8"--- Testing Stage 3: GlobalAddresses UBO & Shader BDA Resolution ---");
    {
        // 1. Memory layout verification
        static_assert(sizeof(graph::GlobalAddresses) == 48, "GlobalAddresses must be exactly 48 bytes");
        static_assert(offsetof(graph::GlobalAddresses, addr_mesh_draw_params) == 0);
        static_assert(offsetof(graph::GlobalAddresses, addr_pbr_surface) == 8);
        static_assert(offsetof(graph::GlobalAddresses, addr_emissive_surface) == 16);
        static_assert(offsetof(graph::GlobalAddresses, addr_transmission_surface) == 24);
        static_assert(offsetof(graph::GlobalAddresses, addr_global_render_items) == 32);
        static_assert(offsetof(graph::GlobalAddresses, addr_draw_item_ids) == 40);

        graph::GlobalAddresses ga{};
        if (ga.addr_global_render_items != 0 || ga.addr_draw_item_ids != 0)
        {
            GLogError(u8"Test 8 Failed: Expected 0 initialized addresses in GlobalAddresses");
            return 20;
        }

        ga.addr_global_render_items = 0xABCD12340000ULL;
        ga.addr_draw_item_ids       = 0xDCBA43210000ULL;

        if (ga.addr_global_render_items != 0xABCD12340000ULL ||
            ga.addr_draw_item_ids       != 0xDCBA43210000ULL)
        {
            GLogError(u8"Test 8 Failed: GlobalAddresses field assignment mismatch");
            return 21;
        }

        // 2. Test shader compilation with RenderItemResolve constructs
        if (graph::InitShaderCompiler())
        {
            const char *test_comp_glsl = R"(
                #version 460
                #extension GL_EXT_buffer_reference : require
                #extension GL_EXT_scalar_block_layout : require
                #extension GL_ARB_gpu_shader_int64 : require
                #extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

                #define SCENE_SET 0
                #define GLOBAL_ADDRESSES_BINDING 4

                layout(set = SCENE_SET, binding = GLOBAL_ADDRESSES_BINDING) uniform GlobalAddressesInfo
                {
                    uint64_t addr_mesh_draw_params;
                    uint64_t addr_pbr_surface;
                    uint64_t addr_emissive_surface;
                    uint64_t addr_transmission_surface;
                    uint64_t addr_global_render_items;
                    uint64_t addr_draw_item_ids;
                } global_addresses;

                struct RenderItemDescriptor
                {
                    uint transform_id;
                    uint geometry_id;
                    uint material_id;
                    uint texture_id;
                };

                layout(buffer_reference, scalar, buffer_reference_align = 16) buffer RenderItemBufferRef
                {
                    RenderItemDescriptor items[];
                };

                layout(buffer_reference, scalar, buffer_reference_align = 4) buffer DrawItemIDBufferRef
                {
                    uint ids[];
                };

                layout(local_size_x = 64) in;

                layout(binding = 0) buffer OutputBuffer
                {
                    uvec4 results[];
                };

                void main()
                {
                    uint draw_id = gl_GlobalInvocationID.x;
                    uint item_id = DrawItemIDBufferRef(global_addresses.addr_draw_item_ids).ids[draw_id];
                    RenderItemDescriptor desc = RenderItemBufferRef(global_addresses.addr_global_render_items).items[item_id];
                    results[draw_id] = uvec4(desc.transform_id, desc.geometry_id, desc.material_id, desc.texture_id);
                }
            )";

            auto *spv = graph::CompileShader(VK_SHADER_STAGE_COMPUTE_BIT, test_comp_glsl);
            if (!spv || !spv->result)
            {
                GLogError(u8"Test 8 Failed: GLSL BDA shader compilation failed: %s",
                          spv && spv->log ? spv->log : "unknown error");
                if (spv) graph::FreeSPVData(spv);
                graph::CloseShaderCompiler();
                return 22;
            }

            GLogInfo(u8"Test 8: GLSL BDA Shader successfully compiled to SPIR-V (size: %u words)",
                     spv->spv_length);
            graph::FreeSPVData(spv);
            graph::CloseShaderCompiler();
        }
    }

    // Test 9: Stage 4 Verification - DrawItemIDStorage Lifecycle & ECSContext Integration
    GLogInfo(u8"--- Testing Stage 4: DrawItemIDStorage Secondary Index Table ---");
    {
        DrawItemIDStorage id_storage;
        if (id_storage.GetCount() != 0)
        {
            GLogError(u8"Test 9 Failed: Initial count must be 0");
            return 23;
        }

        uint32_t off0 = id_storage.Append(101);
        uint32_t off1 = id_storage.Append(202);
        if (off0 != 0 || off1 != 1 || id_storage.GetCount() != 2)
        {
            GLogError(u8"Test 9 Failed: Append single handle mismatch");
            return 24;
        }

        uint32_t batch_handles[] = { 303, 404, 505 };
        uint32_t off_batch = id_storage.Append(batch_handles, 3);
        if (off_batch != 2 || id_storage.GetCount() != 5)
        {
            GLogError(u8"Test 9 Failed: Append batch handles mismatch");
            return 25;
        }

        const uint32_t *data = id_storage.GetData();
        if (data[0] != 101 || data[1] != 202 || data[2] != 303 || data[3] != 404 || data[4] != 505)
        {
            GLogError(u8"Test 9 Failed: Data content mismatch");
            return 26;
        }

        id_storage.Reset();
        if (id_storage.GetCount() != 0)
        {
            GLogError(u8"Test 9 Failed: Count must be 0 after Reset()");
            return 27;
        }

        // Test Context ownership
        ECSContext ctx("TestContextStage4");
        auto *world_id_storage = ctx.GetDrawItemIDStorage();
        if (!world_id_storage)
        {
            GLogError(u8"Test 9 Failed: ECSContext must own DrawItemIDStorage");
            return 28;
        }
    }

    // Test 10: Stage 4 Verification - Run-Length Compaction & Throughput Savings
    GLogInfo(u8"--- Testing Stage 4: Run-Length Compaction & Secondary Index Reduction ---");
    {
        DrawItemIDStorage id_storage;

        // Case 1: Purely contiguous handles: [100, 101, 102, 103, 104]
        {
            uint32_t handles[] = { 100, 101, 102, 103, 104 };
            hgl::ValueArray<CompactedDrawRange> ranges;
            CompactionStats stats{};

            CompactRenderItemHandles(handles, 5, &id_storage, ranges, &stats);

            if (ranges.GetCount() != 1)
            {
                GLogError(u8"Test 10 Case 1 Failed: Expected 1 range, got %d", ranges.GetCount());
                return 29;
            }
            if (ranges[0].first_instance != 100 || ranges[0].instance_count != 5 || !ranges[0].is_direct)
            {
                GLogError(u8"Test 10 Case 1 Failed: Range content mismatch");
                return 30;
            }
            if (stats.direct_items != 5 || stats.indexed_items != 0 || id_storage.GetCount() != 0)
            {
                GLogError(u8"Test 10 Case 1 Failed: Secondary table must not receive any writes for contiguous runs!");
                return 31;
            }
            if (stats.bytes_saved_over_full != 60) // 5 * 12 bytes = 60 bytes saved
            {
                GLogError(u8"Test 10 Case 1 Failed: Bytes saved mismatch, got %u expected 60", stats.bytes_saved_over_full);
                return 32;
            }
        }

        // Case 2: Purely scattered handles: [10, 25, 40, 75, 90]
        id_storage.Reset();
        {
            uint32_t handles[] = { 10, 25, 40, 75, 90 };
            hgl::ValueArray<CompactedDrawRange> ranges;
            CompactionStats stats{};

            CompactRenderItemHandles(handles, 5, &id_storage, ranges, &stats);

            if (ranges.GetCount() != 1)
            {
                GLogError(u8"Test 10 Case 2 Failed: Expected 1 batched range, got %d", ranges.GetCount());
                return 33;
            }
            if (!IsIndexedDraw(ranges[0].first_instance) || ranges[0].is_direct)
            {
                GLogError(u8"Test 10 Case 2 Failed: Must be indexed draw range");
                return 34;
            }
            if (GetDrawIndexOffset(ranges[0].first_instance) != 0 || ranges[0].instance_count != 5)
            {
                GLogError(u8"Test 10 Case 2 Failed: Offset or instance count mismatch");
                return 35;
            }
            if (id_storage.GetCount() != 5)
            {
                GLogError(u8"Test 10 Case 2 Failed: DrawItemIDStorage count must be 5");
                return 36;
            }
            if (stats.bytes_saved_over_full != 40) // 5 * 12 - 5 * 4 = 40 bytes saved
            {
                GLogError(u8"Test 10 Case 2 Failed: Bytes saved mismatch, got %u expected 40", stats.bytes_saved_over_full);
                return 37;
            }
        }

        // Case 3: Mixed handles: [10, 11, 12, 50, 70, 100, 101, 102, 103, 120]
        id_storage.Reset();
        {
            uint32_t handles[] = { 10, 11, 12, 50, 70, 100, 101, 102, 103, 120 };
            hgl::ValueArray<CompactedDrawRange> ranges;
            CompactionStats stats{};

            CompactRenderItemHandles(handles, 10, &id_storage, ranges, &stats);

            if (ranges.GetCount() != 4)
            {
                GLogError(u8"Test 10 Case 3 Failed: Expected 4 ranges, got %d", ranges.GetCount());
                return 38;
            }
            // Range 0: [10, 11, 12] -> Direct (start=10, count=3)
            if (!ranges[0].is_direct || ranges[0].first_instance != 10 || ranges[0].instance_count != 3)
            {
                GLogError(u8"Test 10 Case 3 Failed: Range 0 mismatch");
                return 39;
            }
            // Range 1: [50, 70] -> Indexed (offset=0, count=2)
            if (ranges[1].is_direct || !IsIndexedDraw(ranges[1].first_instance) ||
                GetDrawIndexOffset(ranges[1].first_instance) != 0 || ranges[1].instance_count != 2)
            {
                GLogError(u8"Test 10 Case 3 Failed: Range 1 mismatch");
                return 40;
            }
            // Range 2: [100, 101, 102, 103] -> Direct (start=100, count=4)
            if (!ranges[2].is_direct || ranges[2].first_instance != 100 || ranges[2].instance_count != 4)
            {
                GLogError(u8"Test 10 Case 3 Failed: Range 2 mismatch");
                return 41;
            }
            // Range 3: [120] -> Direct isolated (start=120, count=1)
            if (!ranges[3].is_direct || ranges[3].first_instance != 120 || ranges[3].instance_count != 1)
            {
                GLogError(u8"Test 10 Case 3 Failed: Range 3 mismatch");
                return 42;
            }
            if (stats.direct_items != 8 || stats.indexed_items != 2 || id_storage.GetCount() != 2)
            {
                GLogError(u8"Test 10 Case 3 Failed: Statistics or storage count mismatch");
                return 43;
            }
        }

        // Case 4: Test Auto-Resolving BDA Shader with RENDER_ITEM_INDEXED_FLAG in SPIR-V
        if (graph::InitShaderCompiler())
        {
            const char *test_auto_resolve_glsl = R"(
                #version 460
                #extension GL_EXT_buffer_reference : require
                #extension GL_EXT_scalar_block_layout : require
                #extension GL_ARB_gpu_shader_int64 : require
                #extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

                #define SCENE_SET 0
                #define GLOBAL_ADDRESSES_BINDING 4

                layout(set = SCENE_SET, binding = GLOBAL_ADDRESSES_BINDING) uniform GlobalAddressesInfo
                {
                    uint64_t addr_mesh_draw_params;
                    uint64_t addr_pbr_surface;
                    uint64_t addr_emissive_surface;
                    uint64_t addr_transmission_surface;
                    uint64_t addr_global_render_items;
                    uint64_t addr_draw_item_ids;
                } global_addresses;

                struct RenderItemDescriptor
                {
                    uint transform_id;
                    uint geometry_id;
                    uint material_id;
                    uint texture_id;
                };

                layout(buffer_reference, scalar, buffer_reference_align = 16) buffer RenderItemBufferRef
                {
                    RenderItemDescriptor items[];
                };

                layout(buffer_reference, scalar, buffer_reference_align = 4) buffer DrawItemIDBufferRef
                {
                    uint ids[];
                };

                #define RENDER_ITEM_INDEXED_FLAG 0x80000000u

                RenderItemDescriptor ResolveRenderItemAuto(uint first_instance, uint instance_offset)
                {
                    if ((first_instance & RENDER_ITEM_INDEXED_FLAG) != 0u)
                    {
                        uint draw_id = (first_instance & ~RENDER_ITEM_INDEXED_FLAG) + instance_offset;
                        uint item_id = DrawItemIDBufferRef(global_addresses.addr_draw_item_ids).ids[draw_id];
                        return RenderItemBufferRef(global_addresses.addr_global_render_items).items[item_id];
                    }
                    else
                    {
                        uint item_id = first_instance + instance_offset;
                        return RenderItemBufferRef(global_addresses.addr_global_render_items).items[item_id];
                    }
                }

                layout(local_size_x = 64) in;

                layout(binding = 0) buffer OutputBuffer
                {
                    uvec4 results[];
                };

                void main()
                {
                    uint idx = gl_GlobalInvocationID.x;
                    // Test both Direct and Indexed auto resolution in shader
                    RenderItemDescriptor d_dir = ResolveRenderItemAuto(100u, idx);
                    RenderItemDescriptor d_idx = ResolveRenderItemAuto(0x80000000u | 50u, idx);
                    results[idx] = uvec4(d_dir.transform_id + d_idx.transform_id,
                                         d_dir.geometry_id  + d_idx.geometry_id,
                                         d_dir.material_id  + d_idx.material_id,
                                         d_dir.texture_id   + d_idx.texture_id);
                }
            )";

            auto *spv = graph::CompileShader(VK_SHADER_STAGE_COMPUTE_BIT, test_auto_resolve_glsl);
            if (!spv || !spv->result)
            {
                GLogError(u8"Test 10 Failed: Auto-Resolving BDA shader compilation failed: %s",
                          spv && spv->log ? spv->log : "unknown error");
                if (spv) graph::FreeSPVData(spv);
                graph::CloseShaderCompiler();
                return 44;
            }

            GLogInfo(u8"Test 10: Auto-Resolving BDA Shader successfully compiled to SPIR-V (size: %u words)",
                     spv->spv_length);
            graph::FreeSPVData(spv);
            graph::CloseShaderCompiler();
        }
    }

    // Test 11: Stage 5 Verification - InstancedPrimitiveComponent CPU-driven Instancing & Contiguous 4-ID
    GLogInfo(u8"--- Testing Stage 5: InstancedPrimitiveComponent Contiguous Allocation ---");
    {
        ECSContext ctx("TestContextStage5");
        auto *entity = ctx.CreateEntity<Entity>("TestInstancedEntity");
        auto inst_comp = entity->AddComponent<InstancedPrimitiveComponent>();

        constexpr uint32_t kInstanceCount = 100;
        inst_comp->SetInstanceCount(kInstanceCount);
        inst_comp->SetMaxInstances(kInstanceCount);

        if (!inst_comp->AllocateContiguousInstances(kInstanceCount))
        {
            GLogError(u8"Test 11 Failed: AllocateContiguousInstances returned false");
            return 45;
        }

        if (inst_comp->GetAllocatedInstanceCapacity() != kInstanceCount)
        {
            GLogError(u8"Test 11 Failed: Expected capacity %u, got %u",
                      kInstanceCount, inst_comp->GetAllocatedInstanceCapacity());
            return 46;
        }

        const auto base_handle = inst_comp->GetRenderItemHandle();
        if (base_handle == graph::INVALID_RENDER_ITEM_HANDLE)
        {
            GLogError(u8"Test 11 Failed: Base render item handle is invalid");
            return 47;
        }

        // Set 4-ID for all 100 instances with sequential transform IDs
        constexpr uint32_t kBaseTransformID = 1000;
        constexpr uint32_t kGeometryID      = 25;
        constexpr uint32_t kMaterialID      = 7;
        constexpr uint32_t kTextureID       = 3;

        if (!inst_comp->SetAllInstances4ID(kBaseTransformID, kGeometryID, kMaterialID, kTextureID, true))
        {
            GLogError(u8"Test 11 Failed: SetAllInstances4ID returned false");
            return 48;
        }

        auto *world_storage = ctx.GetRenderItemStorage();
        for (uint32_t i = 0; i < kInstanceCount; ++i)
        {
            const auto handle = inst_comp->GetInstanceHandle(i);
            if (handle != base_handle + i)
            {
                GLogError(u8"Test 11 Failed: Instance handle non-contiguous at %u", i);
                return 49;
            }

            const auto *desc = world_storage->Get(handle);
            if (!desc || desc->transform_id != kBaseTransformID + i ||
                desc->geometry_id != kGeometryID ||
                desc->material_id != kMaterialID ||
                desc->texture_id  != kTextureID)
            {
                GLogError(u8"Test 11 Failed: Descriptor content mismatch at index %u", i);
                return 50;
            }
        }

        // Test custom instance override
        inst_comp->SetInstance4ID(42, 9999, 30, 8, 4);
        const auto *desc42 = world_storage->Get(base_handle + 42);
        if (!desc42 || desc42->transform_id != 9999 || desc42->geometry_id != 30 ||
            desc42->material_id != 8 || desc42->texture_id != 4)
        {
            GLogError(u8"Test 11 Failed: Per-instance override failed at index 42");
            return 51;
        }

        // Verify Run-Length Compaction: 100 contiguous instances must fold into 1 Direct range!
        hgl::ValueArray<RenderItemHandle> handles;
        handles.Reserve(kInstanceCount);
        for (uint32_t i = 0; i < kInstanceCount; ++i)
            handles.Add(inst_comp->GetInstanceHandle(i));

        DrawItemIDStorage test_id_storage;
        hgl::ValueArray<CompactedDrawRange> ranges;
        CompactionStats stats{};

        CompactRenderItemHandles(handles.GetData(), kInstanceCount, &test_id_storage, ranges, &stats);

        if (ranges.GetCount() != 1)
        {
            GLogError(u8"Test 11 Failed: Expected 1 folded direct range for 100 instances, got %d", ranges.GetCount());
            return 52;
        }
        if (!ranges[0].is_direct || ranges[0].first_instance != base_handle || ranges[0].instance_count != kInstanceCount)
        {
            GLogError(u8"Test 11 Failed: Folded range contents mismatch");
            return 53;
        }
        if (test_id_storage.GetCount() != 0 || stats.indexed_items != 0 || stats.direct_items != kInstanceCount)
        {
            GLogError(u8"Test 11 Failed: Secondary table must have 0 items for contiguous instances");
            return 54;
        }
        if (stats.bytes_saved_over_full != kInstanceCount * 12)
        {
            GLogError(u8"Test 11 Failed: Bytes saved mismatch, expected %u got %u",
                      kInstanceCount * 12, stats.bytes_saved_over_full);
            return 55;
        }

        // Releasing instances
        inst_comp->ReleaseInstances();
        if (inst_comp->GetAllocatedInstanceCapacity() != 0)
        {
            GLogError(u8"Test 11 Failed: Allocated capacity must be 0 after ReleaseInstances");
            return 56;
        }
    }

    // Test 12: Stage 5 Verification - 100% GPU-Driven Alignment & DrawItemIDStorage External Buffer Override
    GLogInfo(u8"--- Testing Stage 5: 100% GPU-Driven Alignment & External Buffer Override ---");
    {
        DrawItemIDStorage id_storage;
        constexpr uint64_t kExternalGPUAddr = 0xFEDCBA9876540000ULL;

        if (id_storage.HasExternalGPUAddress())
        {
            GLogError(u8"Test 12 Failed: HasExternalGPUAddress should be false initially");
            return 57;
        }

        id_storage.SetExternalGPUAddress(kExternalGPUAddr);
        if (!id_storage.HasExternalGPUAddress() || id_storage.GetGPUAddress() != kExternalGPUAddr)
        {
            GLogError(u8"Test 12 Failed: External GPU address override failed");
            return 58;
        }

        // In external buffer mode, SyncToGPU must succeed without copying CPU data
        if (!id_storage.SyncToGPU(nullptr))
        {
            GLogError(u8"Test 12 Failed: SyncToGPU must return true with external buffer");
            return 59;
        }

        id_storage.ClearExternalGPUAddress();
        if (id_storage.HasExternalGPUAddress() || id_storage.GetGPUAddress() != 0)
        {
            GLogError(u8"Test 12 Failed: ClearExternalGPUAddress failed");
            return 60;
        }

        // Verify SPIR-V shader compilation for 100% GPU-Driven indirect culling & auto-resolution
        if (graph::InitShaderCompiler())
        {
            const char *test_gpu_driven_comp_glsl = R"(
                #version 460
                #extension GL_EXT_buffer_reference : require
                #extension GL_EXT_scalar_block_layout : require
                #extension GL_ARB_gpu_shader_int64 : require
                #extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

                #define SCENE_SET 0
                #define GLOBAL_ADDRESSES_BINDING 4

                layout(set = SCENE_SET, binding = GLOBAL_ADDRESSES_BINDING) uniform GlobalAddressesInfo
                {
                    uint64_t addr_mesh_draw_params;
                    uint64_t addr_pbr_surface;
                    uint64_t addr_emissive_surface;
                    uint64_t addr_transmission_surface;
                    uint64_t addr_global_render_items;
                    uint64_t addr_draw_item_ids;
                } global_addresses;

                struct RenderItemDescriptor
                {
                    uint transform_id;
                    uint geometry_id;
                    uint material_id;
                    uint texture_id;
                };

                layout(buffer_reference, scalar, buffer_reference_align = 16) buffer RenderItemBufferRef
                {
                    RenderItemDescriptor items[];
                };

                layout(buffer_reference, scalar, buffer_reference_align = 4) buffer DrawItemIDBufferRef
                {
                    uint ids[];
                };

                layout(buffer_reference, scalar, buffer_reference_align = 4) buffer VisibleCountBufferRef
                {
                    uint count;
                };

                layout(local_size_x = 64) in;

                void main()
                {
                    uint idx = gl_GlobalInvocationID.x;
                    // Simulate GPU-Driven frustum culling: write surviving handles to DrawItemIDBuffer
                    bool visible = (idx % 2 == 0);
                    if (visible)
                    {
                        uint slot = atomicAdd(VisibleCountBufferRef(global_addresses.addr_mesh_draw_params).count, 1);
                        DrawItemIDBufferRef(global_addresses.addr_draw_item_ids).ids[slot] = idx;
                    }
                }
            )";

            auto *spv = graph::CompileShader(VK_SHADER_STAGE_COMPUTE_BIT, test_gpu_driven_comp_glsl);
            if (!spv || !spv->result)
            {
                GLogError(u8"Test 12 Failed: GPU-Driven Compute Shader compilation failed: %s",
                          spv && spv->log ? spv->log : "unknown error");
                if (spv) graph::FreeSPVData(spv);
                graph::CloseShaderCompiler();
                return 61;
            }

            GLogInfo(u8"Test 12: GPU-Driven CS Shader successfully compiled to SPIR-V (size: %u words)",
                     spv->spv_length);
            graph::FreeSPVData(spv);
            graph::CloseShaderCompiler();
        }
    }

    GLogInfo(u8"=== All RenderItemDataStorage Stage 1, 2, 3, 4 & 5 Tests Passed Successfully! ===");
    return 0;
}
