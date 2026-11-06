#include<hgl/graph/asset/AssetLoadGraph.h>
#include<hgl/utf.h>
#include<hgl/log/Log.h>

#include<cstring>

DEFINE_LOGGER_MODULE(AssetDep)
USE_MODULE_LOGGER(AssetDep)

namespace hgl::graph
{
    using namespace hgl;

    const char *GetAssetDepKindName(AssetDepKind kind)
    {
        switch(kind)
        {
            case AssetDepKind::Geometry:        return "geometry";
            case AssetDepKind::Material:        return "material";
            case AssetDepKind::MaterialRecipe:  return "material_recipe";
            case AssetDepKind::Texture:         return "texture";
            case AssetDepKind::MaterialData:    return "material_data";
            case AssetDepKind::Scene:           return "scene";
            case AssetDepKind::Primitive:       return "primitive";
            case AssetDepKind::Environment:     return "environment";
        }

        return "unknown";
    }

    namespace
    {
        /**
         * 编码/容器后缀：先剥掉，剥完再判断类型后缀。
         * `.recipe.toml` → `.recipe` → 命中材质配方规则。
         */
        constexpr const char *kEncodingSuffix[] =
        {
            ".toml", ".fb", ".flat", ".bin",
        };

        /**
         * 类型后缀 / 历史物理后缀表。
         *
         * 后缀**一律剥掉**：`X.Tex2D` 与 `tex:X` 是同一份资产的不同写法，
         * `.recipe.toml`（源配方）与 `.material`（烘焙结果）也是同一份逻辑材质
         * 的不同编码 —— 这与 CMCore 里 toml/fb 伴生仲裁的判定是同一套语义。
         */
        struct SuffixRule
        {
            const char        *suffix;
            asset::AssetType   type;
        };

        constexpr SuffixRule kTypeSuffix[] =
        {
            { ".geometry",       asset::AssetType::Geometry       },
            { ".geo",            asset::AssetType::Geometry       },
            { ".mesh",           asset::AssetType::Geometry       },

            { ".tex2d",          asset::AssetType::Texture        },
            { ".texcube",        asset::AssetType::Texture        },
            { ".texoct",         asset::AssetType::Texture        },
            { ".texture",        asset::AssetType::Texture        },
            { ".tex",            asset::AssetType::Texture        },
            { ".png",            asset::AssetType::Texture        },
            { ".jpg",            asset::AssetType::Texture        },
            { ".jpeg",           asset::AssetType::Texture        },
            { ".tga",            asset::AssetType::Texture        },
            { ".bmp",            asset::AssetType::Texture        },
            { ".psd",            asset::AssetType::Texture        },
            { ".hdr",            asset::AssetType::Texture        },
            { ".exr",            asset::AssetType::Texture        },
            { ".dds",            asset::AssetType::Texture        },
            { ".ktx2",           asset::AssetType::Texture        },
            { ".ktx",            asset::AssetType::Texture        },

            { ".material_data",  asset::AssetType::MaterialData   },
            { ".mtldata",        asset::AssetType::MaterialData   },

            { ".material",       asset::AssetType::MaterialRecipe },
            { ".recipe",         asset::AssetType::MaterialRecipe },
            { ".rcp",            asset::AssetType::MaterialRecipe },

            { ".model",          asset::AssetType::Scene          },
            { ".prim",           asset::AssetType::Primitive      },
            { ".env",            asset::AssetType::Environment    },
        };

        inline int StrLen(const char *str)
        {
            int len = 0;

            while(str[len])
                len++;

            return len;
        }

        inline int LastSeparator(const char *str, int len)
        {
            int res = -1;

            for(int i = 0; i < len; i++)
                if(str[i] == '/')
                    res = i;

            return res;
        }

        /// 判断 `str[0..len)` 是否以 `suffix` 结尾（都已经小写，直接比）
        inline bool EndsWithSuffix(const char *str, int len, const char *suffix)
        {
            const int sl = StrLen(suffix);

            if(sl <= 0 || len < sl)
                return false;

            const char *tail = str + (len - sl);

            for(int i = 0; i < sl; i++)
                if(tail[i] != suffix[i])
                    return false;

            return true;
        }

        OSString MakeDepSummary(const AssetDepKey &root, int node_count, int closure_count, bool acyclic)
        {
            OSString msg = OS_TEXT("依赖图: 根=") + ToOSString(root.MakeId().c_str())
                         + OS_TEXT(" 节点=") + OSString::numberOf(node_count)
                         + OS_TEXT(" 闭包=") + OSString::numberOf(closure_count);

            msg += acyclic ? OSString(OS_TEXT(" 无环")) : OSString(OS_TEXT(" 有环"));

            return msg;
        }
    }//anonymous namespace

    bool AssetDepKey::SetPath(const AnsiString &new_path)
    {
        const int len = new_path.Length();

        if(len <= 0)
        {
            path[0] = 0;
            return false;
        }

        const int copy_len = (len < AssetDepPathMax - 1) ? len : (AssetDepPathMax - 1);

        memcpy(path, new_path.c_str(), (size_t)copy_len);
        path[copy_len] = 0;

        return copy_len == len;
    }

    AnsiString AssetDepKey::MakeId() const
    {
        const char *type_name = asset::GetAssetTypeName(type);

        return AnsiString(type_name ? type_name : "unknown") + ':' + path;
    }

    bool NormalizeAssetRef(const AnsiString &physical_ref,
                           asset::AssetType declared_type,
                           AssetDepKey &out_key)
    {
        out_key.type = asset::AssetType::Unknown;
        out_key.path[0] = 0;

        if(physical_ref.IsEmpty())
            return false;

        // 统一分隔符并折成小写：包裹索引本身就是大小写折叠的，依赖身份也必须
        // 折叠，否则 `Foo.Tex2D` 与 `foo.tex2d` 会变成两个节点。
        AnsiString work = physical_ref;
        const int  full_len = work.Length();
        char      *buf = work.c_str();

        if(buf == nullptr)
            return false;

        for(int i = 0; i < full_len; i++)
        {
            if(buf[i] == '\\')
                buf[i] = '/';
            else if(buf[i] >= 'A' && buf[i] <= 'Z')
                buf[i] = (char)(buf[i] - 'A' + 'a');
        }

        asset::AssetType derived = asset::AssetType::Unknown;

        // 阶段1：反复剥编码后缀（`.recipe.toml` 需要剥两次）
        bool stripped = true;

        while(stripped)
        {
            stripped = false;

            const int len = work.Length();
            const int sep = LastSeparator(work.c_str(), len);

            for(const char *enc : kEncodingSuffix)
            {
                if(!EndsWithSuffix(work.c_str(), len, enc))
                    continue;

                const int sl = StrLen(enc);

                // 只剥文件名部分，且剥完文件名不能为空
                if(len - sl <= sep + 1)
                    continue;

                work = work.SubString(0, len - sl);
                stripped = true;
                break;
            }
        }

        // 阶段2：认类型后缀（只认一次，且只认文件名部分）
        {
            const int len = work.Length();
            const int sep = LastSeparator(work.c_str(), len);

            for(const SuffixRule &rule : kTypeSuffix)
            {
                if(!EndsWithSuffix(work.c_str(), len, rule.suffix))
                    continue;

                const int sl = StrLen(rule.suffix);

                if(len - sl <= sep + 1)
                    continue;

                work = work.SubString(0, len - sl);
                derived = rule.type;
                break;
            }
        }

        // 去掉末尾多余的分隔符（`a/b/` → `a/b`）
        while(work.Length() > 0 && work[work.Length() - 1] == '/')
            work = work.SubString(0, work.Length() - 1);

        if(work.IsEmpty())
            return false;

        // 显式声明的类型优先：调用方明确说了 `tex:`，就不要让后缀去猜。
        out_key.type = (declared_type != asset::AssetType::Unknown) ? declared_type : derived;

        if(!out_key.SetPath(work))
            FLogWarning(OS_TEXT("资产引用过长，已截断：") + ToOSString(physical_ref.c_str()));

        return out_key.IsValid();
    }

    AssetLoadGraph::AssetLoadGraph() {}
    AssetLoadGraph::~AssetLoadGraph() {}

    void AssetLoadGraph::Clear()
    {
        edges_.Clear();
        refs_.Clear();
        node_index_.Clear();
        order_.Clear();
    }

    int AssetLoadGraph::EnsureNode(const AssetDepKey &key)
    {
        const AnsiString id = key.MakeId();

        int index = -1;

        if(node_index_.Get(id, index))
            return index;

        index = order_.Add(key);
        node_index_.Add(id, index);
        refs_.Add(id, 0);

        return index;
    }

    void AssetLoadGraph::AddRef(const AnsiString &id, int delta)
    {
        if(delta == 0)
            return;

        int count = 0;

        if(!refs_.Get(id, count))
            count = 0;

        count += delta;

        if(count < 0)
            count = 0;

        if(!refs_.Update(id, count))
            refs_.Add(id, count);
    }

    int AssetLoadGraph::SetDependencies(const AssetDepKey &node,
                                              const AssetDepEdge *edges,
                                              int edge_count)
    {
        if(!node.IsValid())
            return 0;

        const AnsiString node_id = node.MakeId();

        // 先撤掉旧边：同一场景重载时必须把旧目标的引用计数减回去，
        // 否则引用计数只增不减，资产永远卸载不掉。
        if(const ValueArray<AssetDepEdge> *old = edges_.GetValuePointer(node_id))
        {
            for(int i = 0; i < old->GetCount(); i++)
                AddRef((*old)[i].target.MakeId(), -1);
        }

        ValueArray<AssetDepEdge> list;

        if(edges && edge_count > 0)
        {
            for(int i = 0; i < edge_count; i++)
            {
                const AssetDepEdge &e = edges[i];

                if(!e.target.IsValid())
                    continue;

                const AnsiString target_id = e.target.MakeId();

                // 自环没有意义，直接丢掉
                if(target_id == node_id)
                    continue;

                // 同一条边只记一次，否则引用计数会被同一次登记重复抬高
                bool duplicated = false;

                for(int j = 0; j < list.GetCount(); j++)
                {
                    if(list[j].target.MakeId() == target_id)
                    {
                        duplicated = true;
                        break;
                    }
                }

                if(duplicated)
                    continue;

                EnsureNode(e.target);
                list.Add(e);
            }
        }

        EnsureNode(node);

        if(list.GetCount() == 0)
        {
            if(edges_.ContainsKey(node_id))
                edges_.DeleteByKey(node_id);

            return 0;
        }

        for(int i = 0; i < list.GetCount(); i++)
            AddRef(list[i].target.MakeId(), 1);

        if(!edges_.Update(node_id, list))
            edges_.Add(node_id, list);

        return list.GetCount();
    }

    const AssetDepEdge *AssetLoadGraph::GetDependencies(const AssetDepKey &node,
                                                              int *out_count) const
    {
        const ValueArray<AssetDepEdge> *list = edges_.GetValuePointer(node.MakeId());

        if(list == nullptr || list->GetCount() <= 0)
        {
            if(out_count)
                *out_count = 0;

            return nullptr;
        }

        if(out_count)
            *out_count = list->GetCount();

        return &(*list)[0];
    }

    int AssetLoadGraph::GetDependencyCount(const AssetDepKey &node) const
    {
        int count = 0;

        GetDependencies(node, &count);

        return count;
    }

    int AssetLoadGraph::GetRefCount(const AssetDepKey &target) const
    {
        if(!target.IsValid())
            return 0;

        int count = 0;

        refs_.Get(target.MakeId(), count);

        return count;
    }

    const AssetDepKey *AssetLoadGraph::GetNodeAt(int index) const
    {
        if(index < 0 || index >= order_.GetCount())
            return nullptr;

        return &order_[index];
    }

    bool AssetLoadGraph::SolveClosure(const AssetDepKey &root,
                                            ValueArray<AssetDepKey> &out) const
    {
        out.Clear();

        if(!root.IsValid())
            return false;

        // 显式栈的 DFS：后序出栈即「先依赖后使用」的拓扑序。
        // 场景依赖只有 4~5 层，但这里仍不用递归 —— 环 + 递归会直接爆栈。
        // 栈用两个平行数组保存：`AssetDepKey` 是 POD，能进 ValueArray。
        ValueArray<AssetDepKey> stack_key;
        ValueArray<int>         stack_next;

        // 0/未登记 = 未访问，1 = 在栈上，2 = 已完成
        UnorderedMap<AnsiString, uint8_t> state;

        const AnsiString root_id = root.MakeId();

        state.Add(root_id, 1);

        stack_key.Add(root);
        stack_next.Add(0);

        bool acyclic = true;

        while(stack_key.GetCount() > 0)
        {
            const int top = stack_key.GetCount() - 1;
            const AssetDepKey top_key = stack_key[top];

            int edge_count = 0;
            const AssetDepEdge *edges = GetDependencies(top_key, &edge_count);

            if(edges && stack_next[top] < edge_count)
            {
                const AssetDepEdge edge = edges[stack_next[top]];

                stack_next[top]++;

                if(!edge.target.IsValid())
                    continue;

                const AnsiString target_id = edge.target.MakeId();

                uint8_t st = 0;

                if(!state.Get(target_id, st))
                {
                    state.Add(target_id, 1);

                    stack_key.Add(edge.target);
                    stack_next.Add(0);
                }
                else if(st == 1)
                {
                    // 环：不再入栈，标记后继续 —— 不能死循环，也不能静默当无环
                    acyclic = false;
                }

                continue;
            }

            state.Update(top_key.MakeId(), 2);

            if(top > 0)
                out.Add(top_key);

            stack_key.Delete(top, 1);
            stack_next.Delete(top, 1);
        }

        return acyclic;
    }

    void AssetLoadGraph::DumpToLog(const AssetDepKey &root) const
    {
        ValueArray<AssetDepKey> closure;

        const bool acyclic = SolveClosure(root, closure);

        FLogInfo(MakeDepSummary(root, GetNodeCount(), closure.GetCount(), acyclic));

        for(int i = 0; i < closure.GetCount(); i++)
            FLogInfo(OS_TEXT("  [") + OSString::numberOf(i) + OS_TEXT("] ")
                     + ToOSString(closure[i].MakeId().c_str()));
    }
}//namespace hgl::graph
