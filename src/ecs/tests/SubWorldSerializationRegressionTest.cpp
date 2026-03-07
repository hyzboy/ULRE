#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/SubWorldComponent.h>
#include <hgl/ecs/components/SubSceneMembershipComponent.h>

#include <filesystem>
#include <exception>
#include <iostream>
#include <vector>

using namespace hgl::ecs;

namespace
{
    struct Snapshot
    {
        SubWorldMode mode = SubWorldMode::SharedContext;
        bool render_shared = true;
        bool logic_isolated = false;
        uint64_t subscene_id = 0;
        EntityID root_entity_id;
        bool paused = false;
        bool tick_enabled = true;
        bool render_enabled = true;
        std::string asset_path;
        bool asset_binary = false;
        uint64_t membership_subscene_id = 0;
    };

    bool BuildAndSaveFixture(const std::filesystem::path& json_path, Snapshot& out_expected)
    {
        ECSContext source_ctx("SerializeSource");

        Entity* host = source_ctx.CreateEntity<Entity>("SubWorldHost");
        if (!host)
            return false;

        auto sw = host->AddComponent<SubWorldComponent>();
        if (!sw)
            return false;

        sw->SetRenderShared(true);
        sw->SetLogicIsolated(false);
        sw->SetSubsceneID(424242ull);
        sw->SetRootEntityID(EntityID(7u, 3u));
        sw->SetPaused(true);
        sw->SetTickEnabled(false);
        sw->SetRenderEnabled(true);
        sw->SetAssetPath("example/subworld_fixture.json", false);

        Entity* member = source_ctx.CreateEntity<Entity>("MemberEntity");
        if (!member)
            return false;

        auto membership = member->AddComponent<SubSceneMembershipComponent>(sw->GetSubsceneID());
        if (!membership)
            return false;

        out_expected.mode = sw->GetMode();
        out_expected.render_shared = sw->IsRenderShared();
        out_expected.logic_isolated = sw->IsLogicIsolated();
        out_expected.subscene_id = sw->GetSubsceneID();
        out_expected.root_entity_id = sw->GetRootEntityID();
        out_expected.paused = sw->IsPaused();
        out_expected.tick_enabled = sw->IsTickEnabled();
        out_expected.render_enabled = sw->IsRenderEnabled();
        out_expected.asset_path = sw->GetAssetPath();
        out_expected.asset_binary = sw->IsAssetBinary();
        out_expected.membership_subscene_id = membership->GetSubsceneID();

        return source_ctx.SaveToJson(json_path.string());
    }

    bool LoadAndValidateFixture(const std::filesystem::path& json_path, const Snapshot& expected)
    {
        ECSContext loaded_ctx("SerializeLoaded");
        if (!loaded_ctx.LoadFromJson(json_path.string()))
            return false;

        std::vector<Entity*> entities;
        loaded_ctx.GetAllEntities(entities);

        std::shared_ptr<SubWorldComponent> loaded_sw;
        std::shared_ptr<SubSceneMembershipComponent> loaded_membership;

        for (Entity* e : entities)
        {
            if (!e)
                continue;

            if (!loaded_sw)
                loaded_sw = e->GetComponent<SubWorldComponent>();

            if (!loaded_membership)
                loaded_membership = e->GetComponent<SubSceneMembershipComponent>();

            if (loaded_sw && loaded_membership)
                break;
        }

        if (!loaded_sw || !loaded_membership)
        {
            std::cerr << "Missing components after load: subworld="
                      << (loaded_sw ? "yes" : "no")
                      << " membership="
                      << (loaded_membership ? "yes" : "no")
                      << std::endl;
            return false;
        }

        if (loaded_sw->GetMode() != expected.mode)
        {
            std::cerr << "Mismatch mode: loaded=" << static_cast<int>(loaded_sw->GetMode())
                      << " expected=" << static_cast<int>(expected.mode) << std::endl;
            return false;
        }

        if (loaded_sw->IsRenderShared() != expected.render_shared)
        {
            std::cerr << "Mismatch render_shared: loaded=" << loaded_sw->IsRenderShared()
                      << " expected=" << expected.render_shared << std::endl;
            return false;
        }

        if (loaded_sw->IsLogicIsolated() != expected.logic_isolated)
        {
            std::cerr << "Mismatch logic_isolated: loaded=" << loaded_sw->IsLogicIsolated()
                      << " expected=" << expected.logic_isolated << std::endl;
            return false;
        }

        if (loaded_sw->GetSubsceneID() != expected.subscene_id)
        {
            std::cerr << "Mismatch subscene_id: loaded=" << loaded_sw->GetSubsceneID()
                      << " expected=" << expected.subscene_id << std::endl;
            return false;
        }

        if (loaded_sw->GetRootEntityID() != expected.root_entity_id)
        {
            const auto loaded_root = loaded_sw->GetRootEntityID();
            std::cerr << "Mismatch root_entity_id: loaded=(index=" << loaded_root.index
                      << ", gen=" << loaded_root.generation
                      << ") expected=(index=" << expected.root_entity_id.index
                      << ", gen=" << expected.root_entity_id.generation
                      << ")" << std::endl;
            return false;
        }

        if (loaded_sw->IsPaused() != expected.paused)
        {
            std::cerr << "Mismatch paused: loaded=" << loaded_sw->IsPaused()
                      << " expected=" << expected.paused << std::endl;
            return false;
        }

        if (loaded_sw->IsTickEnabled() != expected.tick_enabled)
        {
            std::cerr << "Mismatch tick_enabled: loaded=" << loaded_sw->IsTickEnabled()
                      << " expected=" << expected.tick_enabled << std::endl;
            return false;
        }

        if (loaded_sw->IsRenderEnabled() != expected.render_enabled)
        {
            std::cerr << "Mismatch render_enabled: loaded=" << loaded_sw->IsRenderEnabled()
                      << " expected=" << expected.render_enabled << std::endl;
            return false;
        }

        if (loaded_sw->GetAssetPath() != expected.asset_path)
        {
            std::cerr << "Mismatch asset_path: loaded='" << loaded_sw->GetAssetPath()
                      << "' expected='" << expected.asset_path << "'" << std::endl;
            return false;
        }

        if (loaded_sw->IsAssetBinary() != expected.asset_binary)
        {
            std::cerr << "Mismatch asset_binary: loaded=" << loaded_sw->IsAssetBinary()
                      << " expected=" << expected.asset_binary << std::endl;
            return false;
        }

        if (loaded_membership->GetSubsceneID() != expected.membership_subscene_id)
        {
            std::cerr << "Mismatch membership_subscene_id: loaded=" << loaded_membership->GetSubsceneID()
                      << " expected=" << expected.membership_subscene_id << std::endl;
            return false;
        }

        return true;
    }
}

static int RunSerializationRegression()
{
    try
    {
        const std::filesystem::path output =
            std::filesystem::temp_directory_path() / "ulre_subworld_serialization_regression.json";

        Snapshot expected{};

        if (!BuildAndSaveFixture(output, expected))
        {
            std::cerr << "BuildAndSaveFixture failed" << std::endl;
            return 1;
        }

        if (!LoadAndValidateFixture(output, expected))
        {
            std::cerr << "LoadAndValidateFixture failed" << std::endl;
            std::error_code ignored;
            std::filesystem::remove(output, ignored);
            return 2;
        }

        std::error_code ignored;
        std::filesystem::remove(output, ignored);

        std::cout << "ECS_SubWorldSerializationRegression: PASS" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 3;
    }
    catch (...)
    {
        std::cerr << "Unknown exception" << std::endl;
        return 4;
    }
}

int main()
{
    return RunSerializationRegression();
}
