#include <hgl/mtl/MaterialRecipeFile.h>
#include <hgl/mtl/DomainEntrySource.h>
#include <hgl/mtl/VkEnumText.h>

#include <hgl/asset/generated/MaterialRecipeMeta.gen.h>
#include <hgl/asset/generated/MaterialRecipeToml.gen.h>
#include <hgl/serialize/Archive.h>

#include <toml/toml.hpp>

#include <cmath>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        // 诊断上下文：一次解析只记录**第一个**失败，字段与既有
        // `MaterialRecipeFileParseError` 一一对应，调用方与日志格式不变。
        struct ParseErrorContext
        {
            AnsiString table = "root";
            AnsiString key;
            AnsiString value;
            AnsiString detail;
            int line = 0;
            bool recorded = false;

            /// 表内失败：`table_path` 用 `FailedPath()` 的写法（`render_state.pipeline`）。
            void RecordIn(const AnsiString &table_path,
                          const char *in_key,
                          const char *in_value,
                          const char *in_detail)
            {
                if (recorded)
                    return;

                table = table_path.IsEmpty() ? AnsiString("root") : table_path;
                key = in_key ? in_key : "";
                value = in_value ? in_value : "";
                detail = in_detail ? in_detail : "";
                recorded = true;
            }

            /// 顶层（表路径 = root）失败。
            void Record(const char *in_key, const char *in_value, const char *in_detail)
            {
                RecordIn(AnsiString("root"), in_key, in_value, in_detail);
            }
        };

        AnsiString JoinPath(const AnsiString &parent, const char *child)
        {
            if (!child || !child[0])
                return parent;
            if (parent.IsEmpty())
                return AnsiString(child);
            return parent + "." + child;
        }

        // `render_state.pipeline.cull_mode` ⇒ 表 "render_state.pipeline" + 键 "cull_mode"。
        void SplitTablePath(const AnsiString &path, AnsiString &table, AnsiString &key)
        {
            key = path;
            table = "root";

            if (path.IsEmpty())
                return;

            const char *begin = path.c_str();
            const char *last_dot = nullptr;
            for (const char *cursor = begin; cursor[0]; ++cursor)
            {
                if (cursor[0] == '.')
                    last_dot = cursor;
            }

            if (!last_dot)
                return;

            table = AnsiString(begin, static_cast<int>(last_dot - begin));
            key = AnsiString(last_dot + 1);
        }

        // 按 TOML 路径取节点，只用于把作者的原始写法（如 "Orthographic2D"）带回诊断。
        // 表数组（`[[textures]]`）不进路径，遇到时取第一个能命中剩余路径的元素。
        const toml::value *FindTomlNode(const toml::value &node, const char *path)
        {
            if (!path || !path[0])
                return &node;

            if (node.is_array())
            {
                for (const toml::value &item : node.as_array())
                {
                    const toml::value *found = FindTomlNode(item, path);
                    if (found)
                        return found;
                }
                return nullptr;
            }

            if (!node.is_table())
                return nullptr;

            const char *dot = strchr(path, '.');
            const int head_length =
                dot ? static_cast<int>(dot - path) : static_cast<int>(strlen(path));
            const char *tail = dot ? dot + 1 : nullptr;

            for (const auto &pair : node.as_table())
            {
                const char *candidate = pair.first.c_str();
                if (candidate && static_cast<int>(strlen(candidate)) == head_length
                 && strncmp(candidate, path, static_cast<size_t>(head_length)) == 0)
                    return FindTomlNode(pair.second, tail);
            }

            return nullptr;
        }

        // 生成 codec 的错误措辞是通用的「字段类型或取值不合法」；枚举键的真实原因
        // 只有「拼写不在真源表里」，这里按键给出与旧解析器一致的措辞。
        const char *DetailForFailedKey(const char *key, const char *fallback)
        {
            struct Entry
            {
                const char *key;
                const char *detail;
            };

            static constexpr Entry kEntries[] = {
                { "cull_mode",        "未知剔除模式"     },
                { "depth_compare_op", "未知深度比较操作" },
                { "blend_src",        "未知源混合因子"   },
                { "blend_dst",        "未知目标混合因子" },
                { "source",           "未知顶点输入模式" },
                { "mapping",          "未知位置映射模式" },
                { "orientation",      "未知朝向模式"     },
                { "scale",            "未知缩放模式"     },
                { "projection",       "未知投影模式"     },
                { "domain",           "未知材质领域"     },
            };

            if (key && key[0])
            {
                for (const Entry &entry : kEntries)
                {
                    if (strcmp(entry.key, key) == 0)
                        return entry.detail;
                }
            }

            return fallback;
        }

        void ApplyCodecFailure(const hgl::serialize::Archive &archive,
                               const toml::value &root,
                               ParseErrorContext &error)
        {
            AnsiString table;
            AnsiString key;
            SplitTablePath(AnsiString(archive.FailedPath()), table, key);

            const char *raw_value = nullptr;
            AnsiString raw_text;
            const toml::value *node = FindTomlNode(root, archive.FailedPath());
            if (node && node->is_string())
            {
                raw_text = node->as_string().c_str();
                raw_value = raw_text.c_str();
            }

            error.RecordIn(table, key.c_str(), raw_value,
                           DetailForFailedKey(key.c_str(), archive.ErrorMessage()));
        }

        // 严格键校验的唯一真源是生成的 `ClassMetaInfo`（`toml_key` + `struct_meta`
        // 递归指针）。这里只判「键名是否合法」，取值/形态错误交给生成的 codec。
        bool ValidateKeysByMeta(const toml::value &node,
                                const hgl::asset::ClassMetaInfo &meta,
                                const AnsiString &path,
                                ParseErrorContext &error)
        {
            if (node.is_array())
            {
                for (const toml::value &item : node.as_array())
                {
                    if (!ValidateKeysByMeta(item, meta, path, error))
                        return false;
                }
                return true;
            }

            if (!node.is_table())
                return true;

            for (const auto &pair : node.as_table())
            {
                const char *input_key = pair.first.c_str();
                const hgl::asset::PropertyMetaInfo *property = nullptr;
                for (uint32_t i = 0; i < meta.property_count; ++i)
                {
                    const hgl::asset::PropertyMetaInfo &candidate = meta.properties[i];
                    const char *toml_key = candidate.toml_key ? candidate.toml_key : candidate.name;
                    if (toml_key && input_key && strcmp(toml_key, input_key) == 0)
                    {
                        property = &candidate;
                        break;
                    }
                }

                if (!property)
                {
                    error.RecordIn(path, input_key, nullptr, "未知键");
                    return false;
                }

                if (property->struct_meta
                 && !ValidateKeysByMeta(pair.second, *property->struct_meta,
                                        JoinPath(path, input_key), error))
                    return false;
            }

            return true;
        }

        // 字段抽取只走生成的 codec：字段名、类型、缺键语义、枚举拼写表全部由
        // `MaterialRecipeSpec` + `AssetGen` 产物单点决定。
        bool ReadSpec(const toml::value &root,
                      MaterialRecipeSpec &spec,
                      ParseErrorContext &error)
        {
            hgl::serialize::Value node;
            hgl::serialize::FromToml(root, node);

            hgl::serialize::Archive archive = hgl::serialize::Archive::ForRead(node);
            hgl::serialize::StructSerializer<MaterialRecipeSpec>::Do(archive, spec);
            if (archive.Ok())
                return true;

            ApplyCodecFailure(archive, root, error);
            return false;
        }

        // `domain` 直接写领域契约名（MaterialDomainContract::name）。
        // 不再支持 `material_definition` 之类的中间名：那层映射正是本次要消灭的东西。
        bool ApplyDomain(const MaterialRecipeSpec &spec,
                         MaterialRecipe &recipe,
                         ParseErrorContext &error)
        {
            recipe.domain = spec.domain;

            if (spec.domain == MaterialDomain::Custom)
            {
                error.Record("domain", GetMaterialDomainName(spec.domain),
                             "Custom 无契约，配方不得使用");
                return false;
            }

            // 有契约 ≠ 能编出着色器。Foliage / Terrain / UI2D / HumanSkin 目前
            // 只有契约、没有固定主着色器入口；在加载期就拒绝，别把错误留到运行时。
            if (!HasMaterialDomainEntrySource(recipe.domain))
            {
                error.Record("domain", GetMaterialDomainName(spec.domain),
                             "该领域尚无固定主着色器入口（未实现）");
                return false;
            }

            return true;
        }

        template <typename Array>
        void ApplyStringOverride(const hgl::serialize::Presence<Array> &source,
                                 std::vector<std::string> &target,
                                 bool &target_present)
        {
            if (!source.present)
                return;

            // 出现过 ⇒ 整表替换（空数组 = 清空），与「不写这一行」语义完全不同。
            target_present = true;
            target.clear();

            for (int i = 0; i < source.value.GetCount(); ++i)
            {
                AnsiString *slot = source.value[i];
                if (slot)
                    target.emplace_back(slot->c_str());
            }
        }

        // `[fragment]`：片元阶段的两类可选意图 ——
        //   surface_module_name：no_material_source 领域的表面形态选择
        //   samplers / defines：采样器预设与编译宏的整表替换
        //
        // 后者必须能从数据表达：位图字形（text_gpu_bitmap）与 SDF 字形领域相同，
        // 差别只在「最近邻采样 + 不解 SDF」。只允许 C++ 侧写这两个覆盖项，等于
        // 让同一种形态在数据里无法复现（RuntimeFormProbe / 回归门禁都只能走代码路径）。
        bool ApplyFragment(const MaterialRecipeSpec &spec,
                           MaterialRecipe &recipe,
                           ParseErrorContext &error)
        {
            ApplyStringOverride(spec.fragment.samplers,
                                recipe.sampler_names, recipe.has_sampler_names);
            ApplyStringOverride(spec.fragment.defines,
                                recipe.compile_defines, recipe.has_compile_defines);

            const AnsiString &name = spec.fragment.surface_module_name;
            if (name.IsEmpty())
                return true;

            // 只有 `no_material_source` 领域（当前仅 Sky）存在「同领域多表面形态」。
            // 其他领域写了这个名字只会被入口源静默忽略 —— 那是拼写错误或无意义配置。
            const MaterialDomainContract *contract = GetMaterialDomainContract(recipe.domain);
            if (!contract || !contract->no_material_source)
            {
                error.RecordIn("fragment", "surface_module_name", name.c_str(),
                               "只有 no_material_source 领域可以声明表面模块");
                return false;
            }

            // 拿真实入口源解析当裁判，而不是在这里维护第二份名字白名单。
            DomainEntrySource probe{};
            if (!ResolveDomainEntrySource(recipe.domain,
                                          ShaderProgramPurpose::ForwardColor,
                                          false, probe, name.c_str()))
            {
                error.RecordIn("fragment", "surface_module_name", name.c_str(),
                               "该领域没有对应的表面形态着色器");
                return false;
            }

            recipe.surface_module_name = name.c_str();
            return true;
        }

        bool ApplyRenderState(const toml::value &root,
                              const MaterialRecipeSpec &spec,
                              MaterialRecipe &recipe,
                              ParseErrorContext &error)
        {
            const RecipeRenderStateSpec &state = spec.render_state;
            MaterialRenderStateOverrides &overrides = recipe.render_state_overrides;

            if (state.alpha_test.present)
            {
                overrides.has_alpha_test = true;
                overrides.alpha_test = state.alpha_test.value;
            }

            if (state.alpha_cutoff.present)
            {
                const float cutoff = state.alpha_cutoff.value;
                if (!std::isfinite(cutoff))
                {
                    error.RecordIn("render_state", "alpha_cutoff", nullptr,
                                   "数值超出有限 float 范围");
                    return false;
                }
                if (cutoff < 0.0f || cutoff > 1.0f)
                {
                    error.RecordIn("render_state", "alpha_cutoff", nullptr,
                                   "必须在 [0, 1] 范围内");
                    return false;
                }

                overrides.has_alpha_cutoff = true;
                overrides.alpha_cutoff = cutoff;
            }

            if (state.dither.present)
            {
                overrides.has_dither = true;
                overrides.dither = state.dither.value;
            }

            if (!state.pipeline.present)
                return true;

            // 「表在、但一个键都没写」是书写错误：局部覆盖必须有覆盖项。
            const toml::value *pipeline_node = FindTomlNode(root, "render_state.pipeline");
            if (pipeline_node && pipeline_node->is_table()
             && pipeline_node->as_table().empty())
            {
                error.RecordIn("render_state.pipeline", nullptr, nullptr,
                               "pipeline 表不能为空");
                return false;
            }

            // 每个键都是**可选**的：只写与默认不同的项，未写的字段保留契约默认值；
            // `present` ⇒ 掩码对应位置 1，这是「显式写成默认值」与「没写」的唯一区分。
            const RecipePipelineSpec &pipeline = state.pipeline.value;
            MaterialPipelineConfig &config = overrides.pipeline_config;
            MaterialPipelineFieldMask &mask = overrides.pipeline_config_mask;

            overrides.has_pipeline_config = true;
            mask = 0;

            if (pipeline.cull_mode.present)
            {
                config.cull_mode = pipeline.cull_mode.value;
                mask |= static_cast<MaterialPipelineFieldMask>(MaterialPipelineField::CullMode);
            }
            if (pipeline.depth_test.present)
            {
                config.depth_test = pipeline.depth_test.value;
                mask |= static_cast<MaterialPipelineFieldMask>(MaterialPipelineField::DepthTest);
            }
            if (pipeline.depth_write.present)
            {
                config.depth_write = pipeline.depth_write.value;
                mask |= static_cast<MaterialPipelineFieldMask>(MaterialPipelineField::DepthWrite);
            }
            if (pipeline.depth_compare_op.present)
            {
                config.depth_compare_op = pipeline.depth_compare_op.value;
                mask |= static_cast<MaterialPipelineFieldMask>(MaterialPipelineField::DepthCompareOp);
            }
            if (pipeline.alpha_blend.present)
            {
                config.alpha_blend = pipeline.alpha_blend.value;
                mask |= static_cast<MaterialPipelineFieldMask>(MaterialPipelineField::AlphaBlend);
            }
            if (pipeline.blend_src.present)
            {
                config.blend_src = pipeline.blend_src.value;
                mask |= static_cast<MaterialPipelineFieldMask>(MaterialPipelineField::BlendSrc);
            }
            if (pipeline.blend_dst.present)
            {
                config.blend_dst = pipeline.blend_dst.value;
                mask |= static_cast<MaterialPipelineFieldMask>(MaterialPipelineField::BlendDst);
            }
            if (pipeline.dynamic_line_width.present)
            {
                config.dynamic_line_width = pipeline.dynamic_line_width.value;
                mask |= static_cast<MaterialPipelineFieldMask>(MaterialPipelineField::DynamicLineWidth);
            }
            if (pipeline.overlay.present)
            {
                config.overlay = pipeline.overlay.value;
                mask |= static_cast<MaterialPipelineFieldMask>(MaterialPipelineField::Overlay);
            }
            if (pipeline.wireframe.present)
            {
                config.wireframe = pipeline.wireframe.value;
                mask |= static_cast<MaterialPipelineFieldMask>(MaterialPipelineField::Wireframe);
            }
            if (pipeline.line_width.present)
            {
                const float width = pipeline.line_width.value;
                if (!std::isfinite(width))
                {
                    error.RecordIn("render_state.pipeline", "line_width", nullptr,
                                   "数值超出有限 float 范围");
                    return false;
                }
                if (width <= 0.0f)
                {
                    error.RecordIn("render_state.pipeline", "line_width", nullptr,
                                   "必须大于 0");
                    return false;
                }

                config.line_width = width;
                mask |= static_cast<MaterialPipelineFieldMask>(MaterialPipelineField::LineWidth);
            }

            return true;
        }

        bool ApplyTextures(const MaterialRecipeSpec &spec,
                           MaterialRecipe &recipe,
                           ParseErrorContext &error)
        {
            const hgl::ManagedArray<RecipeTextureSpec> &textures = spec.textures;
            const int count = textures.GetCount();
            for (int i = 0; i < count; ++i)
            {
                const RecipeTextureSpec *item = textures[i];
                if (!item)
                    continue;

                if (!IsValidMaterialTextureName(item->texture_name.c_str()))
                {
                    error.RecordIn("textures", "name", item->texture_name.c_str(),
                                   "非法纹理槽名称");
                    return false;
                }

                for (const RecipeTextureBinding &existing : recipe.textures)
                {
                    if (existing.texture_name == item->texture_name.c_str())
                    {
                        error.RecordIn("textures", "name", item->texture_name.c_str(),
                                       "纹理槽重复");
                        return false;
                    }
                }

                RecipeTextureBinding binding{};
                binding.texture_name = item->texture_name.c_str();
                binding.resource_id = item->resource_id.c_str();
                binding.required = item->required;
                binding.array_layer = item->layer;
                binding.auto_mipmaps = item->auto_mipmaps;
                recipe.textures.emplace_back(std::move(binding));
            }

            return true;
        }

        // 规约 → 运行时配方。字段取值已在 `ReadSpec` 里定型，这里只做**跨字段**
        // 业务校验与「局部覆盖」的落地（`has_*` + 位掩码）。
        bool BuildRecipe(const toml::value &root,
                         const MaterialRecipeSpec &spec,
                         MaterialRecipe &recipe,
                         ParseErrorContext &error)
        {
            if (spec.name.IsEmpty())
            {
                error.Record("name", "", "必须是非空字符串");
                return false;
            }

            recipe.recipe_name = spec.name.c_str();

            if (!ApplyDomain(spec, recipe, error)
             || !ApplyFragment(spec, recipe, error))
                return false;

            const RecipeTransformSpec &transform = spec.transform;
            VertexShaderNodeConfig &node_config = recipe.vertex_node_config;
            if (transform.input.present)
                node_config.input = transform.input.value;
            if (transform.position_mapping.present)
                node_config.position_mapping = transform.position_mapping.value;
            if (transform.orientation.present)
                node_config.orientation = transform.orientation.value;
            if (transform.scale.present)
                node_config.scale = transform.scale.value;
            if (transform.projection.present)
                node_config.projection = transform.projection.value;

            if (!ApplyRenderState(root, spec, recipe, error)
             || !ApplyTextures(spec, recipe, error))
                return false;

            return true;
        }
    }

    const char *GetMaterialRecipeFileParseResultName(
        const MaterialRecipeFileParseResult result) noexcept
    {
        switch (result)
        {
        case MaterialRecipeFileParseResult::Skipped: return "Skipped";
        case MaterialRecipeFileParseResult::OK: return "OK";
        case MaterialRecipeFileParseResult::InvalidValue: return "InvalidValue";
        case MaterialRecipeFileParseResult::UnknownKey: return "UnknownKey";
        }
        return "Invalid";
    }

    MaterialRecipeFileParseResult ParseMaterialRecipeFile(
        const char *content,
        const int content_size,
        MaterialRecipeFileData &out_data) noexcept
    {
        out_data = MaterialRecipeFileData{};
        if (!content || content_size <= 0)
            return MaterialRecipeFileParseResult::Skipped;

        ParseErrorContext error;
        MaterialRecipeFileParseResult result = MaterialRecipeFileParseResult::OK;
        try
        {
            const toml::value root = toml::parse_str(
                std::string(content, static_cast<size_t>(content_size)));

            if (!root.is_table())
            {
                error.Record(nullptr, nullptr, "根节点必须是表");
                result = MaterialRecipeFileParseResult::InvalidValue;
            }
            else if (!root.contains("schema")
                  || !root.at("schema").is_integer()
                  || root.at("schema").as_integer() != 1)
            {
                error.Record("schema", nullptr, "必须存在且等于 1");
                result = MaterialRecipeFileParseResult::InvalidValue;
            }
            else if (!root.contains("name"))
            {
                error.Record("name", nullptr, "缺少必需字符串");
                result = MaterialRecipeFileParseResult::InvalidValue;
            }
            else if (!root.contains("domain"))
            {
                error.Record("domain", nullptr, "缺少必需字符串");
                result = MaterialRecipeFileParseResult::InvalidValue;
            }
            else if (!ValidateKeysByMeta(root,
                                         hgl::asset::generated::Class_MaterialRecipeSpec,
                                         AnsiString(), error))
            {
                result = MaterialRecipeFileParseResult::UnknownKey;
            }
            else
            {
                MaterialRecipeSpec spec;
                if (!ReadSpec(root, spec, error)
                 || !BuildRecipe(root, spec, out_data.recipe, error))
                    result = MaterialRecipeFileParseResult::InvalidValue;
            }
        }
        catch (const toml::syntax_error &exception)
        {
            if (!exception.errors().empty())
            {
                const toml::error_info &info = exception.errors().front();
                if (!info.locations().empty())
                {
                    error.line = static_cast<int>(
                        info.locations().front().first.first_line_number());
                    error.detail = info.locations().front().second;
                }
                else
                    error.detail = info.title();
            }
            result = MaterialRecipeFileParseResult::InvalidValue;
        }
        catch (const toml::exception &exception)
        {
            error.Record(nullptr, nullptr, exception.what());
            result = MaterialRecipeFileParseResult::InvalidValue;
        }

        if (result != MaterialRecipeFileParseResult::OK)
        {
            out_data.parse_error.table = error.table;
            out_data.parse_error.key = error.key;
            out_data.parse_error.value = error.value;
            out_data.parse_error.detail = error.detail;
            out_data.parse_error.line = error.line;
        }

        return result;
    }
}
