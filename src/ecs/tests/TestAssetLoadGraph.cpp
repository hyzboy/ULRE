// TestAssetLoadGraph — 资产装载顺序图（阶段三）
//
// 只测纯逻辑（无设备 / 无文件 IO），因此无显卡机器与 CI 也能跑。

#include <hgl/graph/asset/AssetLoadGraph.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <cstring>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    bool CheckNormalize(const char *ref, asset::AssetType expect_type, const char *expect_path,
                        asset::AssetType declared = asset::AssetType::Unknown)
    {
        AssetDepKey key;

        if(!NormalizeAssetRef(AnsiString(ref), declared, key))
        {
            GLogError(u8"NormalizeAssetRef 失败：%s", ref);
            return false;
        }

        if(key.type != expect_type || strcmp(key.path, expect_path) != 0)
        {
            GLogError(u8"NormalizeAssetRef 结果不符：%s -> %s，期望 %s", ref, key.path, expect_path);
            return false;
        }

        return true;
    }

    bool TestNormalize()
    {
        // 历史物理后缀 → 类型 + 剥掉后缀（路径折小写）
        if(!CheckNormalize("ABeautifulGame.StaticMesh/ABeautifulGame.Chessboard_normal.Tex2D",
                           asset::AssetType::Texture,
                           "abeautifulgame.staticmesh/abeautifulgame.chessboard_normal"))
            return false;

        if(!CheckNormalize("ABeautifulGame.StaticMesh/ABeautifulGame.0.geometry",
                           asset::AssetType::Geometry,
                           "abeautifulgame.staticmesh/abeautifulgame.0"))
            return false;

        // `.recipe.toml` 要连剥两段
        if(!CheckNormalize("LoadScene.Chess.Chessboard.recipe.toml",
                           asset::AssetType::MaterialRecipe,
                           "loadscene.chess.chessboard"))
            return false;

        if(!CheckNormalize("load_scene.material_data.toml",
                           asset::AssetType::MaterialData,
                           "load_scene"))
            return false;

        // 目录名里的点必须活着（只剥文件名部分的后缀）
        if(!CheckNormalize("ABeautifulGame.StaticMesh/ABeautifulGame.model",
                           asset::AssetType::Model,
                           "abeautifulgame.staticmesh/abeautifulgame"))
            return false;

        // 反斜杠 + 大写一起折
        if(!CheckNormalize("A\\B\\C.TEX2D", asset::AssetType::Texture, "a/b/c"))
            return false;

        // 显式类型优先于后缀推断
        if(!CheckNormalize("foo.bin", asset::AssetType::Texture, "foo", asset::AssetType::Texture))
            return false;

        // 无类型信息且后缀不认识 → 拒绝
        {
            AssetDepKey key;
            if(NormalizeAssetRef(AnsiString("geo/presets/torus"), key))
            {
                GLogError(u8"未知后缀且未声明类型时应判失败");
                return false;
            }

            if(NormalizeAssetRef(AnsiString(""), key))
            {
                GLogError(u8"空引用应判失败");
                return false;
            }

            // 只剩后缀（文件名部分剥空）→ 拒绝
            if(NormalizeAssetRef(AnsiString("dir/.Tex2D"), key))
            {
                GLogError(u8"文件名部分被剥空时应判失败");
                return false;
            }
        }

        return true;
    }

    bool MakeKey(const char *ref, AssetDepKey &key)
    {
        return NormalizeAssetRef(AnsiString(ref), key);
    }

    int IndexOf(const ValueArray<AssetDepKey> &list, const AssetDepKey &key)
    {
        const AnsiString id = key.MakeId();

        for(int i = 0; i < list.GetCount(); i++)
            if(list[i].MakeId() == id)
                return i;

        return -1;
    }

    bool TestGraphBasics()
    {
        AssetLoadGraph graph;

        AssetDepKey scene, mat, geo, tex;

        if(!MakeKey("s.Scene.model", scene)
           || !MakeKey("m.material", mat)
           || !MakeKey("g.geometry", geo)
           || !MakeKey("t.Tex2D", tex))
            return false;

        const AssetDepEdge scene_edges[] =
        {
            { geo, AssetDepKind::Geometry },
            { mat, AssetDepKind::Material  },
            { mat, AssetDepKind::Material  },   // 重复目标必须被去重
            { scene, AssetDepKind::Model   },   // 自环必须被丢掉
        };

        if(graph.SetDependencies(scene, scene_edges, 4) != 2)
        {
            GLogError(u8"重复目标/自环未被过滤");
            return false;
        }

        if(graph.GetDependencyCount(scene) != 2)
            return false;

        // 被引用的资产即使没登记过依赖也要入图
        if(graph.GetRefCount(mat) != 1 || graph.GetRefCount(geo) != 1)
        {
            GLogError(u8"引用计数不符");
            return false;
        }

        const AssetDepEdge mat_edges[] = { { tex, AssetDepKind::Texture } };

        if(graph.SetDependencies(mat, mat_edges, 1) != 1)
            return false;

        if(graph.GetRefCount(tex) != 1 || graph.GetDependencyCount(mat) != 1)
            return false;

        // 整体替换：mat 不再引用 tex，tex 的引用计数必须归零
        if(graph.SetDependencies(mat, nullptr, 0) != 0)
            return false;

        if(graph.GetRefCount(tex) != 0)
        {
            GLogError(u8"整体替换后旧目标的引用计数未回落");
            return false;
        }

        if(graph.GetRefCount(mat) != 1)
        {
            GLogError(u8"替换 mat 的依赖不该影响别人对 mat 的引用计数");
            return false;
        }

        graph.Clear();

        return graph.GetNodeCount() == 0 && graph.GetRefCount(mat) == 0;
    }

    bool TestClosure()
    {
        AssetLoadGraph graph;

        AssetDepKey scene, mat_a, mat_b, tex;

        if(!MakeKey("s.Scene.model", scene)
           || !MakeKey("a.material", mat_a)
           || !MakeKey("b.recipe.toml", mat_b)
           || !MakeKey("t.Tex2D", tex))
            return false;

        const AssetDepEdge scene_edges[] =
        {
            { mat_a, AssetDepKind::Material       },
            { mat_b, AssetDepKind::MaterialRecipe },
        };

        const AssetDepEdge mat_b_edges[] = { { tex, AssetDepKind::Texture } };
        const AssetDepEdge mat_a_edges[] = { { tex, AssetDepKind::Texture } };

        graph.SetDependencies(scene, scene_edges, 2);
        graph.SetDependencies(mat_a, mat_a_edges, 1);
        graph.SetDependencies(mat_b, mat_b_edges, 1);

        ValueArray<AssetDepKey> closure;

        if(!graph.SolveClosure(scene, closure))
        {
            GLogError(u8"无环图被误判为有环");
            return false;
        }

        // 4 个节点：scene / mat_a / mat_b / tex，闭包不含根 → 3 项，且去重
        if(closure.GetCount() != 3)
        {
            GLogError(u8"闭包数量不符，实际 %d", closure.GetCount());
            return false;
        }

        if(IndexOf(closure, scene) >= 0)
        {
            GLogError(u8"闭包不该包含根自身");
            return false;
        }

        // 拓扑序：贴图必须排在使用它的材质之前
        const int tex_index  = IndexOf(closure, tex);
        const int mata_index = IndexOf(closure, mat_a);
        const int matb_index = IndexOf(closure, mat_b);

        if(tex_index < 0 || mata_index < 0 || matb_index < 0)
            return false;

        if(!(tex_index < mata_index && tex_index < matb_index))
        {
            GLogError(u8"拓扑序错误：依赖未排在使用者之前");
            return false;
        }

        // 无依赖的节点：闭包为空且无环
        if(!graph.SolveClosure(tex, closure) || closure.GetCount() != 0)
            return false;

        // 未登记过的节点同样安全
        AssetDepKey unknown;
        MakeKey("n.Scene.model", unknown);

        if(!graph.SolveClosure(unknown, closure) || closure.GetCount() != 0)
            return false;

        return true;
    }

    bool TestCycle()
    {
        AssetLoadGraph graph;

        AssetDepKey a, b, c;

        if(!MakeKey("a.Scene.model", a) || !MakeKey("b.Scene.model", b) || !MakeKey("c.Scene.model", c))
            return false;

        const AssetDepEdge a_edges[] = { { b, AssetDepKind::Model } };
        const AssetDepEdge b_edges[] = { { c, AssetDepKind::Model } };
        const AssetDepEdge c_edges[] = { { a, AssetDepKind::Model } };

        graph.SetDependencies(a, a_edges, 1);
        graph.SetDependencies(b, b_edges, 1);
        graph.SetDependencies(c, c_edges, 1);

        ValueArray<AssetDepKey> closure;

        // 有环：必须返回 false，而且**必须终止**
        if(graph.SolveClosure(a, closure))
        {
            GLogError(u8"环未被检测出来");
            return false;
        }

        if(closure.GetCount() != 2)
        {
            GLogError(u8"环上的节点应各出现一次，实际 %d", closure.GetCount());
            return false;
        }

        return true;
    }
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestAssetLoadGraph"));

    if(!TestNormalize()
       || !TestGraphBasics()
       || !TestClosure()
       || !TestCycle())
    {
        GLogError(u8"=== TestAssetLoadGraph: 测试失败 ===");
        return 1;
    }

    GLogInfo(u8"=== TestAssetLoadGraph: 全部测试通过 ===");
    return 0;
}
