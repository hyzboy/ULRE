#include <hgl/ecs/support/TransformDataStorage.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/log/Log.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace hgl;
using namespace hgl::ecs;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    GLogInfo(u8"=== Testing TransformDataStorage Flat Triplet & Topological Evaluation ===");

    TransformDataStorage storage;

    // Test 1: Allocate flat nodes
    auto root_id = storage.Allocate();
    auto child1_id = storage.Allocate();
    auto child2_id = storage.Allocate();
    auto grand_child_id = storage.Allocate();

    if (storage.GetCount() != 4)
    {
        GLogError(u8"Test 1 Failed: storage.GetCount() != 4");
        return 1;
    }

    // Test 2: Hierarchy setup
    // Root: pos(10, 0, 0)
    storage.SetPosition(root_id, glm::vec3(10.0f, 0.0f, 0.0f));

    // Child1: parent = Root, pos(0, 5, 0)
    storage.SetPosition(child1_id, glm::vec3(0.0f, 5.0f, 0.0f));
    storage.SetParent(child1_id, root_id);

    // Child2: parent = Root, pos(0, -5, 0)
    storage.SetPosition(child2_id, glm::vec3(0.0f, -5.0f, 0.0f));
    storage.SetParent(child2_id, root_id);

    // GrandChild: parent = Child1, pos(0, 0, 2)
    storage.SetPosition(grand_child_id, glm::vec3(0.0f, 0.0f, 2.0f));
    storage.SetParent(grand_child_id, child1_id);

    // Test 3: Topology sort
    storage.RebuildTopologyOrder();

    if (storage.GetLevelCount() != 3)
    {
        GLogError(u8"Test 3 Failed: expected 3 levels, got %u", storage.GetLevelCount());
        return 2;
    }

    if (storage.GetHierarchyDepth(root_id) != 0 ||
        storage.GetHierarchyDepth(child1_id) != 1 ||
        storage.GetHierarchyDepth(child2_id) != 1 ||
        storage.GetHierarchyDepth(grand_child_id) != 2)
    {
        GLogError(u8"Test 3 Failed: depth mismatch");
        return 3;
    }

    // Test 4: Flat matrix evaluation
    storage.UpdateAllWorldMatricesFlat();

    glm::mat4 root_world = storage.GetWorldMatrix(root_id);
    glm::mat4 child1_world = storage.GetWorldMatrix(child1_id);
    glm::mat4 grand_child_world = storage.GetWorldMatrix(grand_child_id);

    glm::vec3 root_pos(root_world[3]);
    glm::vec3 child1_pos(child1_world[3]);
    glm::vec3 grand_child_pos(grand_child_world[3]);

    if (glm::distance(root_pos, glm::vec3(10.0f, 0.0f, 0.0f)) > 1e-4f)
    {
        GLogError(u8"Test 4 Failed: root_pos mismatch");
        return 4;
    }
    if (glm::distance(child1_pos, glm::vec3(10.0f, 5.0f, 0.0f)) > 1e-4f)
    {
        GLogError(u8"Test 4 Failed: child1_pos mismatch");
        return 5;
    }
    if (glm::distance(grand_child_pos, glm::vec3(10.0f, 5.0f, 2.0f)) > 1e-4f)
    {
        GLogError(u8"Test 4 Failed: grand_child_pos mismatch");
        return 6;
    }

    // Test 5: Dynamic root update propagation
    storage.SetPosition(root_id, glm::vec3(20.0f, 0.0f, 0.0f));
    storage.UpdateDirtyWorldMatricesFlat();

    grand_child_world = storage.GetWorldMatrix(grand_child_id);
    grand_child_pos = glm::vec3(grand_child_world[3]);

    if (glm::distance(grand_child_pos, glm::vec3(20.0f, 5.0f, 2.0f)) > 1e-4f)
    {
        GLogError(u8"Test 5 Failed: dynamic cascade update failed, expected (20,5,2), got (%.2f, %.2f, %.2f)",
                 grand_child_pos.x, grand_child_pos.y, grand_child_pos.z);
        return 7;
    }

    // Test 6: Cycle protection
    storage.SetParent(root_id, grand_child_id);
    storage.RebuildTopologyOrder();
    storage.UpdateAllWorldMatricesFlat();
    GLogInfo(u8"Test 6 Passed: Cycle handled successfully without crash");

    // Test 7: ECSContext integration
    ECSContext context;
    auto* ecs_storage = context.GetTransformStorage();
    if (!ecs_storage)
    {
        GLogError(u8"Test 7 Failed: ecs_storage is null");
        return 8;
    }

    auto ent1 = context.CreateEntity<Entity>("RootEnt");
    auto tc1 = ent1->AddComponent<TransformComponent>(Mobility::Static);
    tc1->SetLocalPosition(glm::vec3(1.0f, 2.0f, 3.0f));

    auto ent2 = context.CreateEntity<Entity>("ChildEnt");
    auto tc2 = ent2->AddComponent<TransformComponent>(Mobility::Static);
    tc2->SetParent(ent1->GetEntityID());
    tc2->SetLocalPosition(glm::vec3(0.0f, 10.0f, 0.0f));

    glm::vec3 child_world_pos = tc2->GetWorldPosition();
    if (glm::distance(child_world_pos, glm::vec3(1.0f, 12.0f, 3.0f)) > 1e-4f)
    {
        GLogError(u8"Test 7 Failed: ECS entity hierarchy mismatch, expected (1, 12, 3), got (%.2f, %.2f, %.2f)",
                 child_world_pos.x, child_world_pos.y, child_world_pos.z);
        return 9;
    }

    GLogInfo(u8"=== All TransformDataStorage tests passed successfully! ===");
    return 0;
}
