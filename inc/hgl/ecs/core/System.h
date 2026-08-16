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
         * Execution Phase - Generic runtime cycles ordered by enum value.
         *
         * Rule:
         * - Keep only common/mandatory cycles.
         * - Concrete systems sharing same lifecycle stage use the same phase.
         */
        enum class ExecutionPhase
        {
            // ── Tick (game logic, every frame) ─────────────────────────────
            TickInput,              // user input
            TickTransform,          // transforms, bounds, visibility
            TickCamera,             // camera matrices
            TickPostCamera,         // facing-policy transforms

            // ── Render setup (before command buffer opens) ─────────────────
            RenderSwapchainNextImage,   // acquire swapchain image
            RenderPreBeginFrame,        // per-frame env / viewport sync
                                        //   EnvironmentSystem, RenderTargetSystem
            RenderResourceSetup,        // lazy one-time GPU resource creation
                                        //   QuadResourcePrepareSystem
            RenderMaterialBind,         // per-entity material / texture binding
                                        //   QuadMaterialBindingSystem

            // ── Pre-pass CPU work (command buffer open, outside render pass) ─
            RenderBeginFrame,       // open command buffer, record frame UBOs
            RenderCollect,          // collect / cull visible components
            RenderBatch,            // build batches, write VABs (StagedBuffer writes)
            RenderBufferCommit,     // finalize staged CPU writes
            RenderBufferUpload,     // GPU transfer: vkCmdCopyBuffer
            RenderFrameSync,        // sync frame UBOs/descriptors after upload
                                    //   RenderFrameBusinessSyncSystem

            // ── Inside render pass ──────────────────────────────────────────
            RenderDrawSubmit,       // record draw commands
            RenderPostProcess,      // post-process effects
            RenderDebug,            // debug overlays

            // ── Post-pass ───────────────────────────────────────────────────
            RenderStat,             // stats systems
            RenderSubmit            // present to swapchain
        };

        /**
         * Base class for all systems
         * Systems handle specific types of logic and processing
         */
        class System : public Object
        {
        protected:

            OBJECT_LOGGER

            ExecutionPhase executionPhase = ExecutionPhase::TickInput;
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

            void SetEnabled(bool value) { enabled = value; }
            bool IsEnabled() const { return enabled; }

            /// Set render element type (e.g., "Primitive", "Text", "SkySphere")
            void SetRenderElementType(const std::string& type) { render_element_type = type; }
            const std::string& GetRenderElementType() const { return render_element_type; }

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
