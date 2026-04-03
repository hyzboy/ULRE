#pragma once

#include <hgl/ecs/core/System.h>
#include <hgl/ecs/support/RenderPipelineBase.h>

namespace hgl::ecs
{
    /**
     * RenderPipelineSystem - Base class for all GraphicsPipeline-driven System classes
     * 
     * This class standardizes how Systems interact with RenderPipeline objects.
     * Derived classes implement stage-specific virtual methods (OnCollect, OnBuild, etc.)
     * and declare which pipeline they control via GetPipeline().
     * 
     * The base class handles:
     *   - GraphicsPipeline lookup and validation
     *   - Error handling (missing pipelines, disabled pipelines)
     *   - Automatic phase registration
     * 
     * Derived classes only need to:
     *   1. Implement GetPipeline(context) - return the specific pipeline
     *   2. Override the appropriate On*() method for their phase
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
    // Stage-specific System base classes (one per ExecutionPhase)
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
     * CullSystem - RenderCull phase (optional, not all pipelines need it)
     * Performs frustum/occlusion culling
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
     * SortSystem - RenderSort phase (optional, not all pipelines need it)
     * Sorts visible items by distance, priority, etc.
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
     * SyncSystem - RenderBufferUpload/FrameSync phase (optional)
     * Syncs descriptors, UBOs after GPU buffer uploads
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
