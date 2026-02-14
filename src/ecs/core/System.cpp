#include<hgl/ecs/core/System.h>
#include<hgl/ecs/core/EntityQuery.h>
#include<hgl/ecs/core/Context.h>

namespace hgl
{
    namespace ecs
    {
        System::System(const std::string& name)
            : Object(name)
        {
        }

        SystemCache* System::GetCache()
        {
            // Lazy initialization: create cache_manager on first access
            if (!cache_manager && context)
            {
                cache_manager = std::make_unique<SystemCache>(context);
            }
            return cache_manager.get();
        }

        const SystemCache* System::GetCache() const
        {
            return cache_manager.get();
        }

        void System::AddEntityManually(EntityQuery* query, EntityID entity_id)
        {
            if (!query || !context)
                return;

            Entity* entity = context->GetEntity(entity_id);
            if (!entity)
                return;

            auto cache = GetCache();
            if (cache)
            {
                cache->AddEntityManually(query, entity_id, entity);
            }
        }

        void System::RemoveEntityManually(EntityQuery* query, EntityID entity_id)
        {
            if (!query)
                return;

            auto cache = GetCache();
            if (cache)
            {
                cache->RemoveEntityManually(query, entity_id);
            }
        }
    }//namespace ecs
}//namespace hgl

