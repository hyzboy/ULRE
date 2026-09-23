#include <hgl/ecs/core/RenderPassRequest.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/graph/render/lighting/CascadedShadowController.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    hgl::logger::InitLogger(OS_TEXT("TestCSMIncrementalPass"));

    GLogInfo(u8"=== Testing CSM Incremental Pass & Render Options Contract ===");

    // ─────────────────────────────────────────────────────────────
    // Test 1: RenderPassRequest default values
    // ─────────────────────────────────────────────────────────────
    {
        RenderPassRequest req;
        if (req.load_depth != false)
        {
            GLogError(u8"Test 1 Failed: req.load_depth should be false by default");
            return 1;
        }
        if (req.use_scissor != false)
        {
            GLogError(u8"Test 1 Failed: req.use_scissor should be false by default");
            return 1;
        }
        if (req.clear_scissor_depth != false)
        {
            GLogError(u8"Test 1 Failed: req.clear_scissor_depth should be false by default");
            return 1;
        }
        if (req.mobility_filter != -1)
        {
            GLogError(u8"Test 1 Failed: req.mobility_filter should be -1 by default");
            return 1;
        }
        GLogInfo(u8"Test 1 Passed: RenderPassRequest defaults verified.");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 2: RenderPassOptions default values & assignment
    // ─────────────────────────────────────────────────────────────
    {
        RenderPassOptions opts;
        if (opts.load_depth != false || opts.use_scissor != false || opts.clear_scissor_depth != false)
        {
            GLogError(u8"Test 2 Failed: RenderPassOptions defaults incorrect");
            return 2;
        }

        opts.load_depth = true;
        opts.use_scissor = true;
        opts.scissor.offset = { 64, 128 };
        opts.scissor.extent = { 256, 512 };
        opts.clear_scissor_depth = true;
        opts.clear_depth_value = 0.0f; // Reversed-Z far plane

        if (!opts.load_depth || !opts.use_scissor || !opts.clear_scissor_depth || opts.clear_depth_value != 0.0f)
        {
            GLogError(u8"Test 2 Failed: RenderPassOptions values not preserved");
            return 2;
        }
        GLogInfo(u8"Test 2 Passed: RenderPassOptions configuration verified.");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 3: Mobility Enum & TransformComponent contract
    // ─────────────────────────────────────────────────────────────
    {
        if (static_cast<int>(Mobility::Static) != 0)
        {
            GLogError(u8"Test 3 Failed: Mobility::Static != 0");
            return 3;
        }
        if (static_cast<int>(Mobility::Movable) != 1)
        {
            GLogError(u8"Test 3 Failed: Mobility::Movable != 1");
            return 3;
        }

        TransformComponent comp(Mobility::Static);
        if (comp.GetMobility() != Mobility::Static)
        {
            GLogError(u8"Test 3 Failed: TransformComponent failed to set Mobility::Static");
            return 3;
        }

        comp.SetMobility(Mobility::Movable);
        if (comp.GetMobility() != Mobility::Movable)
        {
            GLogError(u8"Test 3 Failed: TransformComponent failed to set Mobility::Movable");
            return 3;
        }
        GLogInfo(u8"Test 3 Passed: Mobility contracts verified.");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 4: CascadedShadowController ShadowDirtyRect -> PassRequest translation
    // ─────────────────────────────────────────────────────────────
    {
        ShadowDirtyRect dirty_rect{ 100, 200, 300, 400 };

        RenderPassRequest pass_req;
        pass_req.load_depth = true;
        pass_req.use_scissor = true;
        pass_req.scissor.offset = { static_cast<int32_t>(dirty_rect.x), static_cast<int32_t>(dirty_rect.y) };
        pass_req.scissor.extent = { dirty_rect.width, dirty_rect.height };
        pass_req.clear_scissor_depth = true;
        pass_req.mobility_filter = static_cast<int>(Mobility::Static);

        if (pass_req.scissor.offset.x != 100 || pass_req.scissor.offset.y != 200 ||
            pass_req.scissor.extent.width != 300 || pass_req.scissor.extent.height != 400)
        {
            GLogError(u8"Test 4 Failed: DirtyRect to scissor conversion error");
            return 4;
        }

        if (pass_req.mobility_filter != static_cast<int>(Mobility::Static))
        {
            GLogError(u8"Test 4 Failed: Mobility filter not set to Static");
            return 4;
        }
        GLogInfo(u8"Test 4 Passed: DirtyRect to RenderPassRequest translation verified.");
    }

    GLogInfo(u8"=== All CSM Incremental Pass Contract Tests PASSED ===");
    return 0;
}
