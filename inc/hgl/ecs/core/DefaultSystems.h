#pragma once

#include <memory>
#include <string>
#include <hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>

namespace hgl
{
    namespace graph
    {
        class IRenderTarget;
    }

    namespace ecs
    {
        class ECSContext;
        class InputSystem;
        class CameraSystem;
        class LineBoundsUpdateSystem;
        class LineCollectSystem;
        class LineRenderSystem;
        class LineStatsSystem;

        struct DefaultEcsSystems
        {
            std::shared_ptr<InputSystem> input_system;
            std::shared_ptr<CameraSystem> camera_system;
            std::shared_ptr<LineBoundsUpdateSystem> line_bounds_update_system;
            std::shared_ptr<LineCollectSystem> line_collect_system;
            std::shared_ptr<LineRenderSystem> line_render_system;
            std::shared_ptr<LineStatsSystem> line_stats_system;
        };

        struct DefaultEcsDebugConfig
        {
            bool descriptor_contract_diag_log_enabled = false;
            bool material_binding_query_log_enabled = false;
            BindSlotSummaryLogMode bind_slot_summary_log_mode = BindSlotSummaryLogMode::Throttled;
        };

        void ApplyDefaultEcsDebugConfig(ECSContext *ctx, const DefaultEcsDebugConfig &config);

        void EnsureCoreEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt = nullptr);
        bool EnsureSystemGroupSystems(ECSContext *ctx, const std::string& group_name, graph::IRenderTarget *default_rt = nullptr);

        DefaultEcsSystems RegisterDefaultEcsSystems(ECSContext *ctx,
                                                    graph::IRenderTarget *default_rt,
                                                    const DefaultEcsDebugConfig *debug_config = nullptr);
    }
}
