#include <hgl/mtl/MaterialRecipeInherit.h>
#include <hgl/utf.h>

namespace hgl::graph::mtl
{
    namespace
    {
        using hgl::serialize::Presence;

        /// 是否带路径分隔符（`\` 或 `/`）—— 用来区分「配方名」和「文件路径」
        bool HasPathSeparator(const AnsiString &text)
        {
            const char *raw = text.c_str();
            if (!raw)
                return false;

            for (const char *p = raw; *p; ++p)
            {
                if (*p == '\\' || *p == '/')
                    return true;
            }
            return false;
        }

        /// 去掉前导空白（TOML 值已 trim，这里只兜内存注册路径的手写串）
        const char *SkipSpace(const char *text)
        {
            if (!text)
                return nullptr;

            while (*text == ' ' || *text == '\t')
                ++text;

            return text;
        }

        template<typename T>
        void CopyPresence(Presence<T> &dst, const Presence<T> &src)
        {
            dst.value   = src.value;
            dst.present = src.present;
        }

        template<typename T>
        void OverridePresence(Presence<T> &dst, const Presence<T> &src)
        {
            if (src.present)
            {
                dst.value   = src.value;
                dst.present = true;
            }
        }

        /// `Presence<ManagedArray<AnsiString>>` 的拷贝（move-only，只能逐元素重建）
        void CopyStringArray(Presence<hgl::ManagedArray<AnsiString>> &dst,
                             const Presence<hgl::ManagedArray<AnsiString>> &src)
        {
            if (!src.present)
                return;

            const hgl::ManagedArray<AnsiString> &list = src.value;
            const int count = list.GetCount();
            for (int i = 0; i < count; ++i)
            {
                AnsiString *slot = dst.value.Create();
                if (slot && list[i])
                    *slot = *list[i];
            }

            dst.present = true;
        }

        void OverrideStringArray(Presence<hgl::ManagedArray<AnsiString>> &dst,
                                 const Presence<hgl::ManagedArray<AnsiString>> &src)
        {
            if (src.present)
                CopyStringArray(dst, src);
        }

        void MergeTextureEntry(const RecipeTextureSpec &parent,
                               const RecipeTextureSpec &child,
                               RecipeTextureSpec &out)
        {
            out.texture_name = child.texture_name;

            CopyPresence(out.resource_id,  parent.resource_id);
            CopyPresence(out.required,     parent.required);
            CopyPresence(out.layer,        parent.layer);
            CopyPresence(out.auto_mipmaps, parent.auto_mipmaps);

            OverridePresence(out.resource_id,  child.resource_id);
            OverridePresence(out.required,     child.required);
            OverridePresence(out.layer,        child.layer);
            OverridePresence(out.auto_mipmaps, child.auto_mipmaps);
        }

        void CloneTextureEntry(const RecipeTextureSpec &src, RecipeTextureSpec &out)
        {
            out.texture_name = src.texture_name;

            CopyPresence(out.resource_id,  src.resource_id);
            CopyPresence(out.required,     src.required);
            CopyPresence(out.layer,        src.layer);
            CopyPresence(out.auto_mipmaps, src.auto_mipmaps);
        }

        const RecipeTextureSpec *FindTextureSpec(const hgl::ManagedArray<RecipeTextureSpec> &list,
                                                 const char *name)
        {
            if (!name)
                return nullptr;

            const int count = list.GetCount();
            for (int i = 0; i < count; ++i)
            {
                const RecipeTextureSpec *item = list[i];
                if (item && item->texture_name == name)
                    return item;
            }
            return nullptr;
        }

        void MergeTextures(const hgl::ManagedArray<RecipeTextureSpec> &parent,
                           const hgl::ManagedArray<RecipeTextureSpec> &child,
                           hgl::ManagedArray<RecipeTextureSpec> &out)
        {
            // 父里有的槽：子同名则逐字段覆盖，否则原样继承。顺序沿用父声明顺序，
            // 这样继承链上的纹理槽在诊断输出里保持稳定。
            const int parent_count = parent.GetCount();
            for (int i = 0; i < parent_count; ++i)
            {
                const RecipeTextureSpec *parent_item = parent[i];
                if (!parent_item)
                    continue;

                RecipeTextureSpec *slot = out.Create();
                if (!slot)
                    continue;

                const RecipeTextureSpec *child_item =
                    FindTextureSpec(child, parent_item->texture_name.c_str());

                if (child_item)
                    MergeTextureEntry(*parent_item, *child_item, *slot);
                else
                    CloneTextureEntry(*parent_item, *slot);
            }

            // 父里没有的槽：子新增，原样追加（保留子的 presence 位）。
            const int child_count = child.GetCount();
            for (int i = 0; i < child_count; ++i)
            {
                const RecipeTextureSpec *child_item = child[i];
                if (!child_item)
                    continue;

                if (FindTextureSpec(parent, child_item->texture_name.c_str()))
                    continue;

                RecipeTextureSpec *slot = out.Create();
                if (slot)
                    CloneTextureEntry(*child_item, *slot);
            }
        }

        void MergeFragment(const RecipeFragmentSpec &parent,
                           const RecipeFragmentSpec &child,
                           RecipeFragmentSpec &out)
        {
            out.surface_module_name = child.surface_module_name.IsEmpty()
                                    ? parent.surface_module_name
                                    : child.surface_module_name;

            // 采样器/宏是「整表替换」：子写过就整表用子的（含空表 = 清空），
            // 没写过才继承父。不能做成逐项合并 —— 那就没法表达「删掉父的一个宏」。
            if (child.samplers.present)
                CopyStringArray(out.samplers, child.samplers);
            else
                CopyStringArray(out.samplers, parent.samplers);

            if (child.defines.present)
                CopyStringArray(out.defines, child.defines);
            else
                CopyStringArray(out.defines, parent.defines);
        }

        void MergePipeline(const RecipePipelineSpec &parent,
                           const RecipePipelineSpec &child,
                           RecipePipelineSpec &out)
        {
            CopyPresence(out.cull_mode,          parent.cull_mode);
            CopyPresence(out.depth_test,         parent.depth_test);
            CopyPresence(out.depth_write,        parent.depth_write);
            CopyPresence(out.depth_compare_op,   parent.depth_compare_op);
            CopyPresence(out.alpha_blend,        parent.alpha_blend);
            CopyPresence(out.blend_src,          parent.blend_src);
            CopyPresence(out.blend_dst,          parent.blend_dst);
            CopyPresence(out.dynamic_line_width, parent.dynamic_line_width);
            CopyPresence(out.overlay,            parent.overlay);
            CopyPresence(out.wireframe,          parent.wireframe);
            CopyPresence(out.line_width,         parent.line_width);

            OverridePresence(out.cull_mode,          child.cull_mode);
            OverridePresence(out.depth_test,         child.depth_test);
            OverridePresence(out.depth_write,        child.depth_write);
            OverridePresence(out.depth_compare_op,   child.depth_compare_op);
            OverridePresence(out.alpha_blend,        child.alpha_blend);
            OverridePresence(out.blend_src,          child.blend_src);
            OverridePresence(out.blend_dst,          child.blend_dst);
            OverridePresence(out.dynamic_line_width, child.dynamic_line_width);
            OverridePresence(out.overlay,            child.overlay);
            OverridePresence(out.wireframe,          child.wireframe);
            OverridePresence(out.line_width,         child.line_width);
        }

        void MergeRenderState(const RecipeRenderStateSpec &parent,
                              const RecipeRenderStateSpec &child,
                              RecipeRenderStateSpec &out)
        {
            CopyPresence(out.alpha_test,   parent.alpha_test);
            CopyPresence(out.alpha_cutoff, parent.alpha_cutoff);
            CopyPresence(out.dither,       parent.dither);

            OverridePresence(out.alpha_test,   child.alpha_test);
            OverridePresence(out.alpha_cutoff, child.alpha_cutoff);
            OverridePresence(out.dither,       child.dither);

            // pipeline 整体是 `Presence`，但内部 11 个字段各有自己的 present 位：
            // 只要一侧写了这张表，结果就有这张表，键再逐字段合并。
            if (!parent.pipeline.present)
            {
                CopyPresence(out.pipeline, child.pipeline);
                return;
            }

            if (!child.pipeline.present)
            {
                CopyPresence(out.pipeline, parent.pipeline);
                return;
            }

            MergePipeline(parent.pipeline.value, child.pipeline.value, out.pipeline.value);
            out.pipeline.present = true;
        }

        void MergeTransform(const RecipeTransformSpec &parent,
                            const RecipeTransformSpec &child,
                            RecipeTransformSpec &out)
        {
            CopyPresence(out.input,            parent.input);
            CopyPresence(out.position_mapping, parent.position_mapping);
            CopyPresence(out.orientation,      parent.orientation);
            CopyPresence(out.scale,            parent.scale);
            CopyPresence(out.projection,       parent.projection);

            OverridePresence(out.input,            child.input);
            OverridePresence(out.position_mapping, child.position_mapping);
            OverridePresence(out.orientation,      child.orientation);
            OverridePresence(out.scale,            child.scale);
            OverridePresence(out.projection,       child.projection);
        }
    }//namespace

    const char *GetMaterialRecipeParentRefKindName(
        MaterialRecipeParentRefKind kind) noexcept
    {
        switch (kind)
        {
            case MaterialRecipeParentRefKind::None:   return "None";
            case MaterialRecipeParentRefKind::ByName: return "ByName";
            case MaterialRecipeParentRefKind::ByPath: return "ByPath";
            default:
                break;
        }
        return "Unknown";
    }

    bool ParseMaterialRecipeParentRef(const char *parent, MaterialRecipeParentRef &out)
    {
        out = MaterialRecipeParentRef{};

        const char *text = SkipSpace(parent);
        if (!text || !text[0])
            return true;

        // 只有 `rcp:` 前缀被当作类型段；其余一律按「省略类型」处理，
        // 这样 `@Name` / `res/a.recipe.toml` / `Core.PBR.Lit` 都能直接写。
        if (text[0] == 'r' && text[1] == 'c' && text[2] == 'p' && text[3] == ':')
        {
            text += 4;

            if (text[0] == 'f' && text[1] == 'i' && text[2] == 'l' && text[3] == 'e' && text[4] == ':')
            {
                text += 5;
                if (!text[0])
                    return false;

                out.kind = MaterialRecipeParentRefKind::ByPath;
                out.path = hgl::ToOSString(text);
                return true;
            }

            if (text[0] == '@')
            {
                ++text;
                if (!text[0])
                    return false;

                out.kind = MaterialRecipeParentRefKind::ByName;
                out.name = text;
                return true;
            }

            // `rcp:builtin:...` / `rcp:rt:...` 之类对配方没有意义：与其猜，不如报错。
            if (!text[0])
                return false;

            out.kind = MaterialRecipeParentRefKind::ByName;
            out.name = text;
            return true;
        }

        if (text[0] == '@')
        {
            ++text;
            if (!text[0])
                return false;

            out.kind = MaterialRecipeParentRefKind::ByName;
            out.name = text;
            return true;
        }

        const AnsiString literal(text);
        if (HasPathSeparator(literal) || literal.EndsWith(".toml"))
        {
            out.kind = MaterialRecipeParentRefKind::ByPath;
            out.path = hgl::ToOSString(text);
            return true;
        }

        out.kind = MaterialRecipeParentRefKind::ByName;
        out.name = text;
        return true;
    }

    void MergeMaterialRecipeSpec(const MaterialRecipeSpec &parent,
                                 const MaterialRecipeSpec &child,
                                 MaterialRecipeSpec &out)
    {
        // 身份恒取子：版本是本文件的书写格式，名字是本配方自己的名字，
        // `parent` 也保留子的（子若还有父，那是解析器下一轮的事）。
        out.schema = child.schema ? child.schema : parent.schema;
        out.name   = child.name;
        out.parent = child.parent;

        out.domain = (child.domain == MaterialDomain::Custom)
                   ? parent.domain
                   : child.domain;

        MergeFragment(parent.fragment, child.fragment, out.fragment);
        MergeTransform(parent.transform, child.transform, out.transform);
        MergeRenderState(parent.render_state, child.render_state, out.render_state);
        MergeTextures(parent.textures, child.textures, out.textures);
    }
}//namespace hgl::graph::mtl
