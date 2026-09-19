#include <hgl/ecs/support/RenderItemDataStorage.h>
#include <hgl/log/Log.h>

using namespace hgl;
using namespace hgl::ecs;

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

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

    GLogInfo(u8"=== All RenderItemDataStorage Stage 1 Tests Passed Successfully! ===");
    return 0;
}
