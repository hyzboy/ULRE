#pragma once

#include<hgl/ecs/core/Object.h>
#include<hgl/ecs/core/EntityQuery.h>
#include<hgl/log/Log.h>
#include<vector>
#include<typeindex>
#include<memory>

namespace hgl { namespace graph { class RenderCmdBuffer; } }

namespace hgl
{
    namespace ecs
    {
        /**
         * Execution Phase - Defines the stage (may have multiple systems per phase)
         * ordered by enum value.
         *
         * Refinement rule (2026-02-22): one concrete runtime system maps to one
         * concrete phase enum item, so developers can infer execution location by
         * reading phase name only.
         *
         * Mapping guideline:
         * - TickInput_*                  : input collection systems
         * - TickTransform_*              : transform/bounds/visibility systems
         * - TickCamera_*                 : camera simulation systems
         * - TickPostCamera_*             : post-camera transform alignment systems
         * - RenderSwapchainNextImage_*   : acquire systems
         * - RenderPreBeginFrame_*        : RT/context/material pre-pass preparation
         * - RenderBeginFrame_*           : frame-index-ready hooks
         * - RenderBufferCommit_*         : commit queue systems
         * - RenderBufferUpload_*         : upload/barrier systems
         * - RenderPostBeginFrame_*       : begin-frame business sync systems
         * - RenderCollect_*              : collect/cull systems
         * - RenderBatch_*                : sort/build/finalize systems
         * - RenderDrawSubmit_*           : draw submission systems
         * - RenderPostProcess_*          : overlay/post systems
         * - RenderSubmit_*               : present/submit systems
         *
         * Maintenance contract:
         * - New runtime system SHOULD define a new dedicated phase enum item.
         * - If a range API in ECSContext depends on phase intervals, keep new item
         *   inside the corresponding contiguous stage block.
         */
        enum class ExecutionPhase
        {
            // ===== Tick Phase =====
            TickInput_InputSystem,                  // InputSystem
            TickTransform_TransformSystem,          // TransformSystem
            TickTransform_BoundingBoxUpdateSystem,  // BoundingBoxUpdateSystem
            TickTransform_VisibilitySystem,         // VisibilitySystem
            TickCamera_CameraSystem,                // CameraSystem
            TickPostCamera_FacingTransformSystem,   // FacingTransformSystem
            TickPostCamera_SunDirectionControlSystem, // SunDirectionControlSystem
            TickPostCamera_TransformGizmoSystem,     // TransformGizmoSystem

            // ===== Pre-Render Phase =====
            RenderSwapchainNextImage_SwapchainAcquireSystem, // SwapchainNextImageSystem
            RenderPreBeginFrame_RenderTargetSystem,           // RenderTargetSystem
            RenderPreBeginFrame_EnvironmentSystem,            // EnvironmentSystem
            RenderPreBeginFrame_QuadResourcePrepareSystem,    // QuadResourcePrepareSystem
            RenderPreBeginFrame_QuadMaterialBindingSystem,    // QuadMaterialBindingSystem
            RenderBeginFrame_FrameIndexReady,                 // Reserved begin-frame hook
            RenderBufferCommit_RenderBufferCommitSystem,      // RenderBufferCommitSystem
            RenderBufferUpload_RenderBufferUploadSystem,      // RenderBufferUploadSystem
            RenderPostBeginFrame_RenderFrameBusinessSyncSystem, // RenderFrameBusinessSyncSystem

            // ===== Render Collection Phase (may have multiple collectors) =====
            RenderCollect_RenderPrimitiveCollectSystem, // RenderPrimitiveCollectSystem
            RenderCollect_RenderPrimitiveCullSystem,    // RenderPrimitiveCullSystem
            RenderCollect_TextCollectSystem,            // TextCollectSystem

            // ===== Render Batch Phase =====
            RenderBatch_RenderPrimitiveSortSystem,          // RenderPrimitiveSortSystem
            RenderBatch_RenderPrimitiveBatchBuildSystem,    // RenderPrimitiveBatchBuildSystem
            RenderBatch_RenderPrimitiveBatchFinalizeSystem, // RenderPrimitiveBatchFinalizeSystem
            RenderBatch_TextBuildSystem,                    // TextBuildSystem
            RenderBatch_TextResourceSyncSystem,             // TextResourceSyncSystem

            // ===== Render Submit Phase =====
            RenderDrawSubmit_RenderPrimitiveSubmitSystem, // RenderPrimitiveSubmitSystem
            RenderDrawSubmit_TextRenderSubmitSystem,      // TextRenderSubmitSystem

            // ===== Post-Render Phase =====
            RenderPostProcess_LineRenderSystem, // LineRenderSystem

            // ===== Frame Submit Phase =====
            RenderSubmit_SwapchainSubmitSystem // SwapchainSubmitSystem
        };

        /**
         * System type identifiers for dependency management
         */
        enum class SystemType
        {
            Unknown,
            Input,
            Transform,
            Camera,
            BoundingBox,
            RenderCollect,
            RenderBatch,
            RenderSubmit,
            Physics,
            Animation,
            Material,
            // Add more as needed
        };

        /**
         * Base class for all systems
         * Systems handle specific types of logic and processing
         */
        class System : public Object
        {
        protected:

            OBJECT_LOGGER

            bool initialized = false;
            SystemType systemType = SystemType::Unknown;
            ExecutionPhase executionPhase = ExecutionPhase::TickInput_InputSystem;
            std::vector<std::type_index> dependencies; // Type IDs of systems this depends on
            std::unique_ptr<SystemCache> cache_manager;  // Component query cache
            class ECSContext* context = nullptr;  // Owning context
            bool enabled = true;
            std::string render_element_type;  // Render element type (e.g., "Primitive", "Text", "SkySphere")

        public:

            explicit System(const std::string& name = "System");
            virtual ~System() = default;

        public:

            /// Initialize the system
            virtual void Initialize() {}

            /// Shut down the system
            virtual void Shutdown() {}

            /// Update the system (called once per frame)
            virtual void Update(float deltaTime) {}

            /// Render hook (optional). Default no-op.
            virtual void Render(graph::RenderCmdBuffer *, float /*deltaTime*/ ) {}

            /// Get if system is initialized
            bool IsInitialized() const { return initialized; }

            void SetEnabled(bool value) { enabled = value; }
            bool IsEnabled() const { return enabled; }

            /// Set render element type (e.g., "Primitive", "Text", "SkySphere")
            void SetRenderElementType(const std::string& type) { render_element_type = type; }
            const std::string& GetRenderElementType() const { return render_element_type; }

            /// Get system type
            SystemType GetSystemType() const { return systemType; }

            /// Get execution phase
            ExecutionPhase GetExecutionPhase() const { return executionPhase; }

            /// Get dependencies (systems that must run before this one)
            const std::vector<std::type_index>& GetDependencies() const { return dependencies; }

            /// Called after all dependencies are ready
            virtual void OnDependenciesReady() {}

            /// Get the cache manager for this system
            SystemCache* GetCache();
            const SystemCache* GetCache() const;

            /// Manual participation: Explicitly add an entity to a query
            /// Used for custom logic where entity doesn't match standard signature
            /// Example: Add entity to LOD system when close to camera
            void AddEntityManually(EntityQuery* query, EntityID entity_id);

            /// Manual participation: Explicitly remove an entity from a query
            /// Used to remove entities from optional processing
            /// Example: Remove entity from AI update when too far from player
            void RemoveEntityManually(EntityQuery* query, EntityID entity_id);

            /// Set the context (called by Context when system is registered)
            void SetContext(ECSContext* ctx) { context = ctx; }

        protected:

            void SetInitialized(bool value) { initialized = value; }

            /// Set system type (call in derived constructor)
            void SetSystemType(SystemType type) { systemType = type; }

            /// Set execution order by phase
            void SetExecutionOrder(ExecutionPhase phase)
            {
                executionPhase = phase;
            }

            /// Add a dependency to another system type
            template<typename T>
            void AddDependency()
            {
                dependencies.push_back(std::type_index(typeid(T)));
            }

            /// Create a query for finding entities with specific components
            /// Usage: auto query = CreateQuery<TransformComponent, RenderComponent>();
            template<typename FirstComponent, typename... RestComponents>
            EntityQuery* CreateQuery()
            {
                auto cache = GetCache();
                if (!cache)
                    return nullptr;
                return cache->CreateQuery<FirstComponent, RestComponents...>();
            }
        };
    }//namespace ecs
}//namespace hgl

