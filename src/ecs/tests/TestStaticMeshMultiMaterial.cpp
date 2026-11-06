// 阶段二轻量验证：多材质场景的资产级材质下标透传 + 实例化行选择逻辑。
// 纯数据用例，不需要设备/窗口：
//   1) PrimitiveAsset 的 material_index 构造/复制/移动/缺省语义；
//   2) StaticMeshModelInstance::ResolveMaterialRowIndex 的
//      「按材质下标」与「挂载顺序轮转回退」两种模式。

#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/asset/StaticMeshModelInstance.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    // 材质下标透传：构造入参、默认值、setter、拷贝/移动语义。
    bool TestPrimitiveAssetMaterialIndex()
    {
        mtl::MaterialRecipe recipe_a{};
        mtl::MaterialRecipe recipe_b{};

        // 默认构造 ⇒ 无材质下标（既有单材质/手工资产保持 -1）。
        PrimitiveAsset empty;
        if (empty.GetMaterialIndex() != -1) return false;

        // 构造函数携带下标。
        PrimitiveAsset with_index(nullptr, &recipe_a, PrimitiveType::Triangles, 7);
        if (with_index.GetMaterialIndex() != 7) return false;
        if (with_index.GetMaterialRecipe() != &recipe_a) return false;

        // 构造函数缺省下标 = -1（既有调用行为不变）。
        PrimitiveAsset legacy(nullptr, &recipe_b);
        if (legacy.GetMaterialIndex() != -1) return false;
        if (legacy.GetMaterialRecipe() != &recipe_b) return false;

        // setter 可覆盖。
        legacy.SetMaterialIndex(2);
        if (legacy.GetMaterialIndex() != 2) return false;

        // 拷贝保留下标与内联变体（variants_ 重指向自身 inline_variant_）。
        PrimitiveAsset copied(with_index);
        if (copied.GetMaterialIndex() != 7) return false;
        if (copied.GetDefaultVariant() == nullptr
            || copied.GetDefaultVariant()->material_recipe != &recipe_a) return false;

        PrimitiveAsset assigned = copied;
        if (assigned.GetMaterialIndex() != 7) return false;

        // 移动后源对象复位为默认值。
        PrimitiveAsset moved(std::move(with_index));
        if (moved.GetMaterialIndex() != 7) return false;
        if (with_index.GetMaterialIndex() != -1) return false;

        return true;
    }

    StaticMeshModelInstance::Options MakeRowOptions(const GlobalSSBOBinding *rows,
                                                    uint32_t count,
                                                    bool by_material_index)
    {
        StaticMeshModelInstance::Options options;
        options.material_rows = rows;
        options.material_row_count = count;
        options.rows_by_material_index = by_material_index;
        return options;
    }

    // 行选择：默认轮转；开启 rows_by_material_index 后按下标，越界/缺失回退轮转。
    bool TestResolveMaterialRowIndex()
    {
        // 行数组内容无关紧要，只用下标做断言。
        GlobalSSBOBinding rows[4]{};

        mtl::MaterialRecipe recipe{};
        const StaticMeshModelInstance::Options cycle = MakeRowOptions(rows, 4, false);
        const StaticMeshModelInstance::Options direct = MakeRowOptions(rows, 4, true);

        PrimitiveAsset m0(nullptr, &recipe, PrimitiveType::Triangles, 0);
        PrimitiveAsset m3(nullptr, &recipe, PrimitiveType::Triangles, 3);
        PrimitiveAsset none(nullptr, &recipe, PrimitiveType::Triangles, -1);
        PrimitiveAsset oob(nullptr, &recipe, PrimitiveType::Triangles, 9);

        // 关闭开关：始终挂载顺序轮转（既有行为）。
        if (StaticMeshModelInstance::ResolveMaterialRowIndex(m3, cycle, 0) != 0) return false;
        if (StaticMeshModelInstance::ResolveMaterialRowIndex(m3, cycle, 5) != 1) return false;
        if (StaticMeshModelInstance::ResolveMaterialRowIndex(none, cycle, 7) != 3) return false;

        // 开启开关：有效下标直接命中行。
        if (StaticMeshModelInstance::ResolveMaterialRowIndex(m0, direct, 2) != 0) return false;
        if (StaticMeshModelInstance::ResolveMaterialRowIndex(m3, direct, 0) != 3) return false;

        // 下标缺失或越界：回退轮转。
        if (StaticMeshModelInstance::ResolveMaterialRowIndex(none, direct, 6) != 2) return false;
        if (StaticMeshModelInstance::ResolveMaterialRowIndex(oob, direct, 3) != 3) return false;

        // 空行表：返回 0（调用方以 material_row_count>0 为准跳过绑定）。
        const StaticMeshModelInstance::Options empty_rows = MakeRowOptions(nullptr, 0, true);
        if (StaticMeshModelInstance::ResolveMaterialRowIndex(m0, empty_rows, 0) != 0) return false;

        return true;
    }
}//anonymous namespace

int main(int, char **)
{
    hgl::logger::InitLogger(OS_TEXT("TestStaticMeshMultiMaterial"));

    bool ok = true;
    ok = ok && TestPrimitiveAssetMaterialIndex();
    ok = ok && TestResolveMaterialRowIndex();

    if (ok)
    {
        GLogInfo(u8"=== TestStaticMeshMultiMaterial: 全部测试通过 ===");
        return 0;
    }

    GLogError(u8"=== TestStaticMeshMultiMaterial: 存在测试失败 ===");
    return 1;
}
