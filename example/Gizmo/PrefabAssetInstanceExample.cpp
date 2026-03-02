#include<hgl/framework/WorkManager.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include<vector>
#include<string>
#include<iostream>

using namespace hgl;

class PrefabAssetInstanceApp : public WorkObject
{
private:
    hgl::ecs::ECSContext *ecs_world = nullptr;

    struct HouseInstance
    {
        hgl::ecs::Entity *root = nullptr;
        hgl::ecs::ECSContext::AssetInstance asset_instance;
    };

    std::vector<HouseInstance> houses;

    uint32_t tick_count = 0;
    bool removed_second_house = false;

private:

    bool SpawnHouseInstance(const std::string &root_name,
                            const math::Vector3f &position,
                            const std::string &asset_path,
                            bool binary)
    {
        if(!ecs_world)
            return false;

        HouseInstance item;
        item.root = ecs_world->CreateEntity<hgl::ecs::Entity>(root_name.c_str());
        if(!item.root)
            return false;

        auto transform = item.root->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        if(transform)
        {
            transform->SetLocalTRS(glm::vec3(position), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
            transform->SetMovable(true);
        }

        if(!ecs_world->InstantiateAssetAsChildren(asset_path, item.root->GetID(), binary, &item.asset_instance))
        {
            std::cout << "InstantiateAssetAsChildren failed: " << asset_path << std::endl;
            ecs_world->DestroyEntity(item.root->GetID());
            return false;
        }

        houses.push_back(std::move(item));
        return true;
    }

    bool InitScene()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        const std::string house_asset_path = "res/prefab/house.json";

        SpawnHouseInstance("House_Root_A", math::Vector3f(-15.0f, 0.0f, 0.0f), house_asset_path, false);
        SpawnHouseInstance("House_Root_B", math::Vector3f(  0.0f, 0.0f, 0.0f), house_asset_path, false);
        SpawnHouseInstance("House_Root_C", math::Vector3f( 15.0f, 0.0f, 0.0f), house_asset_path, false);

        std::cout << "PrefabAssetInstanceExample started. houses=" << houses.size() << std::endl;
        std::cout << "After ~5 seconds, the middle house instance will be removed." << std::endl;

        return true;
    }

public:

    bool Init() override
    {
        return InitScene();
    }

    ~PrefabAssetInstanceApp()
    {
        if(!ecs_world)
            return;

        for(auto &h : houses)
        {
            ecs_world->DestroyAssetInstance(h.asset_instance);
            if(h.root)
                ecs_world->DestroyEntity(h.root->GetID());
        }

        houses.clear();
    }

    void Tick(double delta) override
    {
        (void)delta;

        ++tick_count;

        if(!removed_second_house && tick_count > 300 && houses.size() >= 2)
        {
            ecs_world->DestroyAssetInstance(houses[1].asset_instance);
            if(houses[1].root)
                ecs_world->DestroyEntity(houses[1].root->GetID());

            removed_second_house = true;
            std::cout << "Removed middle house instance." << std::endl;
        }

        WorkObject::Tick(delta);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<PrefabAssetInstanceApp>(OS_TEXT("Prefab Asset Instance Example"), argc, argv, 1280, 720);
}
