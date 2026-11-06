#include<hgl/graph/asset/StaticMeshSceneInstance.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/log/Log.h>
#include<hgl/log/Logger.h>
#include<glm/gtc/matrix_transform.hpp>

/**
 * 判据：`StaticMeshSceneInstance` 的三段渐进激活（骨架 → 几何 → 材质）。
 *
 * 本文件不碰设备：`PrimitiveAsset::IsValid()` 要真几何才成立，所以几何/材质
 * 两段只能验证**守约不崩**那一侧（空资产、错顺序调用、重复释放）。
 * 真正的显式行为由 `LoadScene` / GLTF 示例覆盖。
 *
 * 骨架段是重点 —— 它是「场景一发起加载，画面就有正确空场景」的唯一支点，
 * 尤其是**包围盒必须不依赖几何**：拿不到包围盒，相机就只能瞎猜距离。
 */

static int g_fail = 0;

#define EXPECT(cond)                                                            \
    do                                                                          \
    {                                                                           \
        if (!(cond))                                                            \
        {                                                                       \
            GLogError(u8"判据失败：%s（%s:%d）", #cond, __FILE__, __LINE__);    \
            ++g_fail;                                                           \
        }                                                                       \
    } while (0)

namespace
{
    using namespace hgl;
    using namespace hgl::ecs;
    using namespace hgl::math;
    using hgl::graph::StaticMeshNode;
    using hgl::graph::StaticMeshSceneInstance;

    constexpr float kEps = 1e-5f;

    bool NearlyEqual(float a, float b)
    {
        const float d = a - b;
        return d > -kEps && d < kEps;
    }

    /// 两节点：根在原点、子节点沿 +X 平移 2；两者各带半径 1 的节点包围盒
    void BuildTwoNodeScene(std::vector<StaticMeshNode> &nodes, std::vector<int32_t> &roots)
    {
        nodes.clear();
        nodes.resize(2);

        nodes[0].name        = "Root";
        nodes[0].parentIndex = -1;
        nodes[0].hasTRS      = false;
        nodes[0].localMatrix = Matrix4f(1.0f);
        nodes[0].worldMatrix = Matrix4f(1.0f);
        nodes[0].nodeBounds.SetFromAABB(Vector3f(-1, -1, -1), Vector3f(1, 1, 1));
        nodes[0].boundsValid = true;
        nodes[0].primitiveIndices.push_back(0);

        nodes[1].name        = "Child";
        nodes[1].parentIndex = 0;
        nodes[1].hasTRS      = true;
        nodes[1].translation = Vector3f(2, 0, 0);
        nodes[1].rotation    = Quatf(1, 0, 0, 0);
        nodes[1].scale       = Vector3f(1, 1, 1);
        nodes[1].localMatrix = glm::translate(Matrix4f(1.0f), Vector3f(2, 0, 0));
        nodes[1].worldMatrix = nodes[1].localMatrix;
        nodes[1].nodeBounds.SetFromAABB(Vector3f(-1, -1, -1), Vector3f(1, 1, 1));
        nodes[1].boundsValid = true;
        nodes[1].primitiveIndices.push_back(1);

        roots.assign(1, 0);
    }

    // ── 判据 1：空壳骨架必须完整，并且自带包围盒 ──────────────────────────

    void TestSkeleton()
    {
        ECSContext world;

        std::vector<StaticMeshNode> nodes;
        std::vector<int32_t>        roots;
        BuildTwoNodeScene(nodes, roots);

        StaticMeshSceneInstance instance;

        EXPECT(!instance.IsInstantiated());

        BoundingVolumes empty_bounds;
        EXPECT(!instance.GetSkeletonBounds(empty_bounds));

        EXPECT(instance.InstantiateSkeleton(&world, nodes, roots));
        EXPECT(instance.IsInstantiated());
        EXPECT(instance.GetNodeCount() == 2);

        // 骨架阶段不能有任何图元：几何/材质是后面两段的事。
        EXPECT(instance.GetAttachedPrimitiveCount() == 0);

        for (int i = 0; i < 2; ++i)
        {
            const EntityID entity = instance.GetNodeEntity(i);
            EXPECT(entity.IsValid());
            EXPECT(world.GetEntity(entity) != nullptr);
            EXPECT(IsValidTransformID(instance.GetNodeTransform(i)));
        }

        // 整体包围盒：根 [-1,1] ∪ 子 [+1,+3] ⇒ x ∈ [-1,3]，y/z ∈ [-1,1]
        BoundingVolumes bounds;
        EXPECT(instance.GetSkeletonBounds(bounds));
        EXPECT(!bounds.aabb.IsEmpty());
        EXPECT(NearlyEqual(bounds.aabb.GetMin().x, -1.0f));
        EXPECT(NearlyEqual(bounds.aabb.GetMax().x,  3.0f));
        EXPECT(NearlyEqual(bounds.aabb.GetMin().y, -1.0f));
        EXPECT(NearlyEqual(bounds.aabb.GetMax().y,  1.0f));

        // 二次调用必须被拒（幂等且不泄漏实体）
        EXPECT(!instance.InstantiateSkeleton(&world, nodes, roots));

        instance.Release();
        EXPECT(!instance.IsInstantiated());
        EXPECT(instance.GetNodeCount() == 0);
        EXPECT(instance.GetAttachedPrimitiveCount() == 0);

        BoundingVolumes cleared;
        EXPECT(!instance.GetSkeletonBounds(cleared));
        EXPECT(cleared.aabb.IsEmpty());

        instance.Release();     // 幂等：重复释放不得崩
    }

    // ── 判据 2：节点包围盒缺失时不得谎报包围盒 ────────────────────────────

    void TestSkeletonWithoutBounds()
    {
        ECSContext world;

        std::vector<StaticMeshNode> nodes;
        std::vector<int32_t>        roots;
        BuildTwoNodeScene(nodes, roots);

        nodes[0].boundsValid = false;
        nodes[1].boundsValid = false;

        StaticMeshSceneInstance instance;
        EXPECT(instance.InstantiateSkeleton(&world, nodes, roots));

        // 宁可说「不知道」，也不能给一个假的包围盒让相机对错焦。
        BoundingVolumes bounds;
        EXPECT(!instance.GetSkeletonBounds(bounds));
        EXPECT(bounds.aabb.IsEmpty());

        instance.Release();
    }

    // ── 判据 3：几何/材质的顺序守约 ───────────────────────────────────────

    void TestAttachGuards()
    {
        ECSContext world;

        std::vector<StaticMeshNode>        nodes;
        std::vector<int32_t>               roots;
        std::vector<graph::PrimitiveAsset> no_assets;

        BuildTwoNodeScene(nodes, roots);

        StaticMeshSceneInstance instance;

        // 骨架未建就挂几何/材质：必须被拒，而不是把状态弄坏。
        EXPECT(!instance.AttachGeometries(no_assets, nodes));
        EXPECT(!instance.AttachMaterials());
        EXPECT(instance.GetAttachedPrimitiveCount() == 0);

        EXPECT(instance.InstantiateSkeleton(&world, nodes, roots));

        // 资产一个都没到：挂不上，但骨架必须原封不动。
        EXPECT(!instance.AttachGeometries(no_assets, nodes));
        EXPECT(instance.GetAttachedPrimitiveCount() == 0);
        EXPECT(instance.GetNodeCount() == 2);

        // 材质段在没有图元时无事可做，且没有材质行时同样应当静默返回 false。
        EXPECT(!instance.AttachMaterials());

        StaticMeshSceneInstance::Options options;
        EXPECT(!instance.AttachMaterials(options));     // material_rows 为空

        // 节点数与骨架对不上：拒绝，防止把图元挂到错的实体上。
        std::vector<StaticMeshNode> shorter(nodes.begin(), nodes.end() - 1);
        EXPECT(!instance.AttachGeometries(no_assets, shorter));

        instance.Release();
    }

    // ── 判据 4：空节点集是合法输入 ───────────────────────────────────────

    void TestEmptyScene()
    {
        ECSContext world;

        std::vector<StaticMeshNode>        nodes;
        std::vector<int32_t>               roots;
        std::vector<graph::PrimitiveAsset> no_assets;

        StaticMeshSceneInstance instance;
        EXPECT(instance.InstantiateSkeleton(&world, nodes, roots));
        EXPECT(instance.IsInstantiated());
        EXPECT(instance.GetNodeCount() == 0);

        BoundingVolumes bounds;
        EXPECT(!instance.GetSkeletonBounds(bounds));

        EXPECT(!instance.AttachGeometries(no_assets, nodes));

        instance.Release();
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    logger::InitLogger(OS_TEXT("TestStaticMeshSceneProgressive"));

    TestSkeleton();
    TestSkeletonWithoutBounds();
    TestAttachGuards();
    TestEmptyScene();

    if (g_fail > 0)
    {
        GLogError(u8"=== TestStaticMeshSceneProgressive: %d 项判据失败 ===", g_fail);
        return 1;
    }

    GLogInfo(u8"=== TestStaticMeshSceneProgressive: 全部测试通过 ===");
    return 0;
}
