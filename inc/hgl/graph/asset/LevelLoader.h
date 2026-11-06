#pragma once

#include <hgl/ecs/core/entity/EntityHandle.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/asset/GeometryAsset.h>
#include <hgl/graph/asset/LevelSpec.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/type/ManagedArray.h>

namespace hgl
{
    namespace ecs
    {
        class ECSContext;
    }

    namespace graph
    {
        class GeometryCreater;
        class GeometryManager;
        class Geometry;

        class LevelInstance
        {
        public:
            struct RuntimeMaterial
            {
                AnsiString name;
                GlobalSSBODataAccessor accessor;
                ManagedArray<LevelTextureSpec> textures;
            };

            struct RuntimeNode
            {
                AnsiString name;
                ecs::EntityID entity_id = ecs::EntityID::Invalid();
                Geometry *geometry = nullptr;
                const GeometryAsset *geometry_asset = nullptr;
                mtl::MaterialRecipe recipe;
                PrimitiveVariant variant;
                PrimitiveAsset primitive;
            };

        private:
            ecs::ECSContext *world = nullptr;
            GeometryManager *geometry_manager = nullptr;
            ecs::EntityID camera_entity_id = ecs::EntityID::Invalid();
            Color4f previous_clear_color{0.0f, 0.0f, 0.0f, 1.0f};
            bool clear_color_changed = false;
            ManagedArray<RuntimeMaterial> materials;
            ManagedArray<RuntimeNode> nodes;

            friend class LevelLoader;

        public:
            LevelInstance() = default;
            ~LevelInstance();

            LevelInstance(const LevelInstance &) = delete;
            LevelInstance &operator=(const LevelInstance &) = delete;

            bool IsInstantiated() const { return world != nullptr; }
            void Release();
            int GetMaterialCount() const { return materials.GetCount(); }
            const RuntimeMaterial *GetMaterial(int index) const
            {
                return index >= 0 && index < materials.GetCount() ? materials[index] : nullptr;
            }
            RuntimeNode *GetNode(int index)
            {
                return index >= 0 && index < nodes.GetCount() ? nodes[index] : nullptr;
            }
            int GetNodeCount() const { return nodes.GetCount(); }
        };

        class LevelLoader
        {
        public:
            static bool Instantiate(const LevelSpec &spec,
                                    ecs::ECSContext *world,
                                    GeometryCreater *creater,
                                    LevelInstance &out_instance,
                                    const OSString &level_filename = {});
        };
    }
}
