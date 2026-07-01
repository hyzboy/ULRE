#pragma once

#include <hgl/ecs/core/System.h>
#include <hgl/ecs/support/RenderPipelineBase.h>

namespace hgl::ecs
{
    /**
     * RenderPipelineSystem - Base class for pipeline-driven ECS systems
     *
     * This class standardizes how systems interact with RenderPipelineBase objects.
     * Derived classes implement stage hooks (OnCollect, OnBuild, etc.) and
     * declare which pipeline they control via GetPipeline().
     *
     * The base class handles:
     *   - Pipeline lookup and validation
     *   - Error handling (missing pipelines, disabled pipelines)
     *
     * Derived classes should:
     *   1. Implement GetPipeline(context)
     *   2. Override the matching On*() stage hook
     *   3. Set execution phase/order in constructor as needed
     */
    class RenderPipelineSystem : public System
    {
    protected:
        OBJECT_LOGGER

    public:
        explicit RenderPipelineSystem(const std::string& name = "RenderPipelineSystem")
            : System(name) {}
        
        virtual ~RenderPipelineSystem() = default;

        /**
         * Get the pipeline associated with this system
         * Must be implemented by derived classes
         * @return pointer to RenderPipelineBase, or nullptr if not found
         */
        virtual RenderPipelineBase* GetPipeline(ECSContext* context) = 0;

        /**
         * Validate that the pipeline is available and enabled
         * @return true if pipeline exists and system is enabled
         */
        bool ValidatePipeline(ECSContext* context);
    };

    // ──────────────────────────────────────────────────────────────────────────
    // Stage-specific helper base classes
    // ──────────────────────────────────────────────────────────────────────────

    /**
     * CollectSystem - RenderCollect phase
     * Gathers visible components for rendering
     */
    class CollectSystem : public RenderPipelineSystem
    {
    public:
        explicit CollectSystem(const std::string& name = "CollectSystem")
            : RenderPipelineSystem(name) {}

    protected:
        void Update(float dt) override final;

    private:
        virtual void OnCollect(RenderPipelineBase* pipeline) = 0;
    };

    /**
     * CullSystem - Optional culling stage hook
     * Usually scheduled in RenderCollect for current ECS phase model.
     */
    class CullSystem : public RenderPipelineSystem
    {
    public:
        explicit CullSystem(const std::string& name = "CullSystem")
            : RenderPipelineSystem(name) {}

    protected:
        void Update(float dt) override final;

    private:
        virtual void OnCull(RenderPipelineBase* pipeline) = 0;
    };

    /**
     * SortSystem - Optional sorting stage hook
     * Usually scheduled in RenderBatch for current ECS phase model.
     */
    class SortSystem : public RenderPipelineSystem
    {
    public:
        explicit SortSystem(const std::string& name = "SortSystem")
            : RenderPipelineSystem(name) {}

    protected:
        void Update(float dt) override final;

    private:
        virtual void OnSort(RenderPipelineBase* pipeline) = 0;
    };

    /**
     * BuildSystem - RenderBatch phase
     * Builds batches, writes to VABs, prepares for GPU upload
     */
    class BuildSystem : public RenderPipelineSystem
    {
    public:
        explicit BuildSystem(const std::string& name = "BuildSystem")
            : RenderPipelineSystem(name) {}

    protected:
        void Update(float dt) override final;

    private:
        virtual void OnBuild(RenderPipelineBase* pipeline) = 0;
    };

    /**
     * SyncSystem - RenderFrameSync phase hook (optional)
     * Syncs descriptors/UBOs after GPU buffer upload.
     */
    class SyncSystem : public RenderPipelineSystem
    {
    public:
        explicit SyncSystem(const std::string& name = "SyncSystem")
            : RenderPipelineSystem(name) {}

    protected:
        void Update(float dt) override final;

    private:
        virtual void OnSync(RenderPipelineBase* pipeline) = 0;
    };

    /**
     * RenderPipelineDrawSystem - RenderDrawSubmit phase
     * Records GPU draw commands for all batches
     */
    class RenderPipelineDrawSystem : public RenderPipelineSystem
    {
    public:
        explicit RenderPipelineDrawSystem(const std::string& name = "RenderPipelineDrawSystem")
            : RenderPipelineSystem(name) {}

    protected:
        void Render(hgl::graph::RenderCmdBuffer* cmd, float dt) override final;

    private:
        virtual void OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd) = 0;
    };

}  // namespace hgl::ecs
