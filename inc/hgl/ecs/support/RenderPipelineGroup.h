#pragma once

#include <hgl/ecs/support/RenderPipelineBase.h>
#include <hgl/ecs/support/RenderPipelineSystem.h>
#include <memory>
#include <string>
#include <vector>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * RenderPipelineGroup - Container for a RenderPipeline and its associated Systems
     *
     * Each group encapsulates:
     *   - One RenderPipelineBase derived instance (e.g., LineRenderPipeline)
     *   - 1-4 RenderPipelineSystem derived instances (Collect, Build, Render, etc.)
     *
     * Group is responsible for:
     *   1. Creating the GraphicsPipeline instance
     *   2. Creating and registering System instances to Context
     *   3. Lifecycle management (Initialize/Shutdown)
     *
     * Usage:
     *   auto line_group = std::make_unique<LineRenderPipelineGroup>();
     *   line_group->Initialize(context);  // Registers pipeline and systems
     *   ...
     *   line_group->Shutdown(context);    // Cleans up
     *
     * Design Pattern:
     *   - GraphicsPipeline (logical/data owner): Business logic for rendering
     *   - Systems (thin proxies): Delegates to GraphicsPipeline at specific phases
     *   - Group (container): Owns both, ensures they work together
     *
     * This architecture ensures:
     *   ✓ ECS Context stays element-agnostic (never knows about specific GraphicsPipeline types)
     *   ✓ Systems are "thin" (only delegate, no business logic)
     *   ✓ New render types can be added without modifying Context
     *   ✓ Complete encapsulation of rendering logic
     */
    class RenderPipelineGroup
    {
    protected:
        std::string name_;
        std::unique_ptr<RenderPipelineBase> pipeline_;
        std::vector<std::unique_ptr<RenderPipelineSystem>> systems_;
        bool enabled_ = true;

    public:
        explicit RenderPipelineGroup(const std::string& name) : name_(name) {}
        virtual ~RenderPipelineGroup() = default;

        // ─────────────────────────────────────────────────────────

        /**
         * Initialize the group
         * Derived classes should:
         *   1. Call CreatePipeline() to instantiate the pipeline
         *   2. Call RegisterSystems() to create and store systems
         *   3. Register pipeline to context: context->RegisterRenderPipeline(name_, pipeline)
         *   4. Register each system to context: context->AddSystem(system)
         *   5. Return true on success
         *
         * @param context The ECS context to register to
         * @return true if initialization successful
         */
        virtual bool Initialize(ECSContext* context) = 0;

        /**
         * Shutdown the group
         * Derived classes should:
         *   1. Call pipeline_->Shutdown() if needed
         *   2. Remove systems from context (if needed)
         *   3. Clear systems_ and pipeline_
         *
         * @param context The ECS context to unregister from
         */
        virtual void Shutdown(ECSContext* context) = 0;

        // ─────────────────────────────────────────────────────────

        const std::string& GetName() const { return name_; }
        RenderPipelineBase* GetPipeline() const { return pipeline_.get(); }
        const std::vector<std::unique_ptr<RenderPipelineSystem>>& GetSystems() const { return systems_; }

        bool IsEnabled() const { return enabled_; }
        void SetEnabled(bool enabled) { enabled_ = enabled; }

    protected:
        /// Subclass implements: Create the pipeline instance
        virtual std::unique_ptr<RenderPipelineBase> CreatePipeline() = 0;

        /// Subclass implements: Create and store system instances in systems_ vector
        virtual void RegisterSystems() = 0;
    };

}  // namespace hgl::ecs
