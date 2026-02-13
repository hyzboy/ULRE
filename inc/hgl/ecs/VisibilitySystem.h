#pragma once

#include<hgl/ecs/System.h>
#include<hgl/ecs/Context.h>

namespace hgl::ecs
{
    class VisibilityDataStorage;

    /**
     * VisibilitySystem - Manages VisibilityDataStorage for fast rendering queries
     * 
     * Initializes all VisibilityComponents with storage reference.
     * Components automatically update storage on visibility changes.
     * RenderPrimitiveCollectSystem queries storage for O(1) visibility checks.
     */
    class VisibilitySystem : public System
    {
    private:
        ECSContext* world;
        VisibilityDataStorage* visibility_storage;

    public:
        VisibilitySystem(const std::string& name = "VisibilitySystem");
        ~VisibilitySystem() override;

        void SetWorld(ECSContext* ctx) { world = ctx; }
        VisibilityDataStorage* GetStorage() const { return visibility_storage; }
        
        void Initialize() override;
        void Update(float deltaTime) override;
        void OnDependenciesReady() override;
    };
}//namespace hgl::ecs
