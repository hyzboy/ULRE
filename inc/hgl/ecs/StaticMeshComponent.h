#pragma once

#include<hgl/ecs/Component.h>
#include<hgl/ecs/EntityHandle.h>
#include<hgl/ecs/StaticMesh.h>
#include<memory>
#include<vector>

namespace hgl::ecs
{
    class StaticMeshComponent : public Component
    {
    private:
        std::shared_ptr<StaticMesh> mesh;
        std::vector<EntityID> spawned_entities;
        bool auto_instantiate = true;
        bool spawned = false;

    public:
        explicit StaticMeshComponent(const std::string& name = "StaticMesh")
            : Component(name)
        {
        }

        ~StaticMeshComponent() override = default;

    public:
        void SetStaticMesh(const std::shared_ptr<StaticMesh>& new_mesh, bool rebuild = true);
        const std::shared_ptr<StaticMesh>& GetStaticMesh() const { return mesh; }

        void SetAutoInstantiate(bool enabled);
        bool IsAutoInstantiate() const { return auto_instantiate; }

        void BuildEntities();
        void ClearEntities();

    public:
        void OnAttach() override;
        void OnDetach() override;
    };
}
