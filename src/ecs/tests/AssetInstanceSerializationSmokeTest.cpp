#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/AssetInstanceComponent.h>
#include <hgl/ecs/components/AssetNodeMotionComponent.h>

#include <filesystem>
#include <exception>
#include <iostream>

using namespace hgl::ecs;

namespace
{
    bool RunSmoke(const std::filesystem::path& json_path)
    {
        ECSContext source_ctx("AssetInstanceSource");

        Entity* e = source_ctx.CreateEntity<Entity>("AssetInstanceHost");
        if (!e)
            return false;

        auto instance = e->AddComponent<AssetInstanceComponent>();
        if (!instance)
            return false;

        instance->SetAssetWorldID(1001ull);
        instance->SetInstanceID(88ull);
        instance->SetExpectedVersion(7u);
        instance->SetVisibilityMask(0x00FF00FF00FF00FFull);
        instance->SetFlags(0x31u);

        AssetOverrideRef override_ref{};
        override_ref.payload_ref = 0xBEEFull;
        override_ref.revision = 3u;
        instance->SetOverrideRef(override_ref);

        auto motion = e->AddComponent<AssetNodeMotionComponent>();
        if (!motion)
            return false;

        motion->SetInstanceID(88ull);
        motion->SetOverrideTableRef(0x12345678ull);
        motion->SetTableRevision(11u);

        if (!source_ctx.SaveToJson(json_path.string()))
            return false;

        ECSContext loaded_ctx("AssetInstanceLoaded");
        if (!loaded_ctx.LoadFromJson(json_path.string()))
            return false;

        std::vector<Entity*> entities;
        loaded_ctx.GetAllEntities(entities);

        for (Entity* entity : entities)
        {
            if (!entity)
                continue;

            auto loaded_instance = entity->GetComponent<AssetInstanceComponent>();
            auto loaded_motion = entity->GetComponent<AssetNodeMotionComponent>();
            if (!loaded_instance || !loaded_motion)
                continue;

            if (loaded_instance->GetAssetWorldID() != 1001ull)
                return false;
            if (loaded_instance->GetInstanceID() != 88ull)
                return false;
            if (loaded_instance->GetExpectedVersion() != 7u)
                return false;
            if (loaded_instance->GetVisibilityMask() != 0x00FF00FF00FF00FFull)
                return false;
            if (loaded_instance->GetFlags() != 0x31u)
                return false;
            if (loaded_instance->GetOverrideRef().payload_ref != 0xBEEFull)
                return false;
            if (loaded_instance->GetOverrideRef().revision != 3u)
                return false;

            if (loaded_motion->GetInstanceID() != 88ull)
                return false;
            if (loaded_motion->GetOverrideTableRef() != 0x12345678ull)
                return false;
            if (loaded_motion->GetTableRevision() != 11u)
                return false;

            return true;
        }

        return false;
    }
}

int main()
{
    try
    {
        const auto output = std::filesystem::temp_directory_path() / "ulre_asset_instance_smoke.json";

        const bool ok = RunSmoke(output);
        std::error_code ignored;
        std::filesystem::remove(output, ignored);

        if (!ok)
        {
            std::cerr << "AssetInstanceSerializationSmoke: FAIL" << std::endl;
            return 1;
        }

        std::cout << "AssetInstanceSerializationSmoke: PASS" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 2;
    }
    catch (...)
    {
        std::cerr << "Unknown exception" << std::endl;
        return 3;
    }
}
