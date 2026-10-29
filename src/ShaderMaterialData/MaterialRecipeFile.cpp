#include <hgl/mtl/MaterialRecipeFile.h>
#include <hgl/mtl/VkEnumText.h>

#include <toml/toml.hpp>

#include <cfloat>
#include <cmath>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        struct ParseErrorContext
        {
            const char *active_table = "root";
            AnsiString table = "root";
            AnsiString key;
            AnsiString value;
            AnsiString detail;
            int line = 0;
            bool recorded = false;
            bool unknown_key = false;

            void Record(const char *in_key,
                        const char *in_value,
                        const char *in_detail)
            {
                if (recorded)
                    return;

                table = active_table ? active_table : "root";
                key = in_key ? in_key : "";
                value = in_value ? in_value : "";
                detail = in_detail ? in_detail : "";
                recorded = true;
            }
        };

        class ParseTableScope
        {
            ParseErrorContext &context;
            const char *previous;

        public:
            ParseTableScope(ParseErrorContext &in_context, const char *name)
                : context(in_context), previous(in_context.active_table)
            {
                context.active_table = name;
            }

            ~ParseTableScope()
            {
                context.active_table = previous;
            }
        };

        bool ValidateKnownKeys(const toml::value &table,
                               const char *const *allowed,
                               const uint32_t allowed_count,
                               ParseErrorContext &error)
        {
            if (!table.is_table())
            {
                error.Record(nullptr, nullptr, "必须是表");
                return false;
            }

            for (const auto &pair : table.as_table())
            {
                bool known = false;
                for (uint32_t i = 0; i < allowed_count; ++i)
                {
                    if (pair.first == allowed[i])
                    {
                        known = true;
                        break;
                    }
                }

                if (!known)
                {
                    error.unknown_key = true;
                    error.Record(pair.first.c_str(), nullptr, "未知键");
                    return false;
                }
            }
            return true;
        }

        bool ReadString(const toml::value &table,
                        const char *key,
                    AnsiString &out,
                        ParseErrorContext &error,
                        const bool required = false)
        {
            if (!table.is_table() || !table.contains(key))
            {
                if (required)
                    error.Record(key, nullptr, "缺少必需字符串");
                return !required;
            }

            const toml::value &value = table.at(key);
            if (!value.is_string())
            {
                error.Record(key, nullptr, "必须是字符串");
                return false;
            }

            out = value.as_string();
            if (required && out.IsEmpty())
            {
                error.Record(key, "", "必须是非空字符串");
                return false;
            }
            return true;
        }

        bool ReadBool(const toml::value &table,
                      const char *key,
                      bool &out,
                      ParseErrorContext &error,
                      const bool required = false)
        {
            if (!table.is_table() || !table.contains(key))
            {
                if (required)
                    error.Record(key, nullptr, "缺少必需布尔值");
                return !required;
            }

            const toml::value &value = table.at(key);
            if (!value.is_boolean())
            {
                error.Record(key, nullptr, "必须是布尔值");
                return false;
            }

            out = value.as_boolean();
            return true;
        }

        bool ReadFloat(const toml::value &table,
                       const char *key,
                       float &out,
                       ParseErrorContext &error,
                       const bool required = false)
        {
            if (!table.is_table() || !table.contains(key))
            {
                if (required)
                    error.Record(key, nullptr, "缺少必需数值");
                return !required;
            }

            const toml::value &value = table.at(key);
            double number = 0.0;
            if (value.is_floating())
                number = value.as_floating();
            else if (value.is_integer())
                number = static_cast<double>(value.as_integer());
            else
            {
                error.Record(key, nullptr, "必须是数值");
                return false;
            }

            if (!std::isfinite(number)
             || number > static_cast<double>(FLT_MAX)
             || number < -static_cast<double>(FLT_MAX))
            {
                error.Record(key, nullptr, "数值超出有限 float 范围");
                return false;
            }

            out = static_cast<float>(number);
            return true;
        }

        bool ReadUnsigned(const toml::value &table,
                          const char *key,
                          uint32_t &out,
                          ParseErrorContext &error)
        {
            if (!table.is_table() || !table.contains(key))
                return true;

            const toml::value &value = table.at(key);
            if (!value.is_integer())
            {
                error.Record(key, nullptr, "必须是非负整数");
                return false;
            }

            const int64 number = value.as_integer();
            if (number < 0 || static_cast<uint64_t>(number) > hgl::HGL_U32_MAX)
            {
                error.Record(key, nullptr, "超出 uint32 范围");
                return false;
            }

            out = static_cast<uint32_t>(number);
            return true;
        }

        bool ParseTransform(const toml::value &root,
                            MaterialRecipe &recipe,
                            ParseErrorContext &error)
        {
            if (!root.contains("transform"))
                return true;

            ParseTableScope scope(error, "transform");
            const toml::value &table = root.at("transform");
            static const char *const keys[] = {
                "source", "mapping", "orientation", "scale", "projection"
            };
            if (!ValidateKnownKeys(table, keys, 5, error))
                return false;

            AnsiString value;
            if (table.contains("source"))
            {
                if (!ReadString(table, "source", value, error)
                 || !ParseVertexInputMode(value.c_str(), recipe.vertex_node_config.input))
                {
                    error.Record("source", value.c_str(), "未知顶点输入模式");
                    return false;
                }
            }
            if (table.contains("mapping"))
            {
                if (!ReadString(table, "mapping", value, error)
                 || !ParsePositionMappingMode(value.c_str(), recipe.vertex_node_config.position_mapping))
                {
                    error.Record("mapping", value.c_str(), "未知位置映射模式");
                    return false;
                }
            }
            if (table.contains("orientation"))
            {
                if (!ReadString(table, "orientation", value, error)
                 || !ParseOrientationMode(value.c_str(), recipe.vertex_node_config.orientation))
                {
                    error.Record("orientation", value.c_str(), "未知朝向模式");
                    return false;
                }
            }
            if (table.contains("scale"))
            {
                if (!ReadString(table, "scale", value, error)
                 || !ParseScaleMode(value.c_str(), recipe.vertex_node_config.scale))
                {
                    error.Record("scale", value.c_str(), "未知缩放模式");
                    return false;
                }
            }
            if (table.contains("projection"))
            {
                if (!ReadString(table, "projection", value, error)
                 || !ParseProjectionMode(value.c_str(), recipe.vertex_node_config.projection))
                {
                    error.Record("projection", value.c_str(), "未知投影模式");
                    return false;
                }
            }
            return true;
        }

        bool ParsePipeline(const toml::value &table,
                           MaterialRenderStateOverrides &overrides,
                           ParseErrorContext &error)
        {
            static const char *const keys[] = {
                "cull_mode", "depth_test", "depth_write", "depth_compare_op",
                "alpha_blend", "blend_src", "blend_dst", "dynamic_line_width",
                "overlay", "wireframe", "line_width"
            };
            ParseTableScope scope(error, "render_state.pipeline");
            if (!ValidateKnownKeys(table, keys, 11, error))
                return false;

            if (table.as_table().empty())
            {
                error.Record(nullptr, nullptr, "pipeline 表不能为空");
                return false;
            }

            MaterialPipelineConfig &config = overrides.pipeline_config;
            AnsiString value;
            if (!ReadString(table, "cull_mode", value, error, true)
             || !ParseVkCullMode(value.c_str(), config.cull_mode))
            {
                error.Record("cull_mode", value.c_str(), "缺少或未知剔除模式");
                return false;
            }
            if (!ReadBool(table, "depth_test", config.depth_test, error, true)
             || !ReadBool(table, "depth_write", config.depth_write, error, true))
                return false;
            if (!ReadString(table, "depth_compare_op", value, error, true)
             || !ParseVkCompareOp(value.c_str(), config.depth_compare_op))
            {
                error.Record("depth_compare_op", value.c_str(), "缺少或未知深度比较操作");
                return false;
            }
            if (!ReadBool(table, "alpha_blend", config.alpha_blend, error, true))
                return false;
            if (!ReadString(table, "blend_src", value, error, true)
             || !ParseVkBlendFactor(value.c_str(), config.blend_src))
            {
                error.Record("blend_src", value.c_str(), "缺少或未知源混合因子");
                return false;
            }
            if (!ReadString(table, "blend_dst", value, error, true)
             || !ParseVkBlendFactor(value.c_str(), config.blend_dst))
            {
                error.Record("blend_dst", value.c_str(), "缺少或未知目标混合因子");
                return false;
            }
            if (!ReadBool(table, "dynamic_line_width", config.dynamic_line_width, error, true)
             || !ReadBool(table, "overlay", config.overlay, error, true)
             || !ReadBool(table, "wireframe", config.wireframe, error, true)
             || !ReadFloat(table, "line_width", config.line_width, error, true))
                return false;

            if (config.line_width <= 0.0f)
            {
                error.Record("line_width", nullptr, "必须大于 0");
                return false;
            }

            overrides.has_pipeline_config = true;
            return true;
        }

        bool ParseRenderState(const toml::value &root,
                              MaterialRenderStateOverrides &overrides,
                              ParseErrorContext &error)
        {
            if (!root.contains("render_state"))
                return true;

            ParseTableScope scope(error, "render_state");
            const toml::value &table = root.at("render_state");
            static const char *const keys[] = {
                "alpha_test", "alpha_cutoff", "dither", "pipeline"
            };
            if (!ValidateKnownKeys(table, keys, 4, error))
                return false;

            if (table.contains("alpha_test"))
            {
                if (!ReadBool(table, "alpha_test", overrides.alpha_test, error))
                    return false;
                overrides.has_alpha_test = true;
            }
            if (table.contains("alpha_cutoff"))
            {
                if (!ReadFloat(table, "alpha_cutoff", overrides.alpha_cutoff, error))
                    return false;
                if (overrides.alpha_cutoff < 0.0f || overrides.alpha_cutoff > 1.0f)
                {
                    error.Record("alpha_cutoff", nullptr, "必须在 [0, 1] 范围内");
                    return false;
                }
                overrides.has_alpha_cutoff = true;
            }
            if (table.contains("dither"))
            {
                if (!ReadBool(table, "dither", overrides.dither, error))
                    return false;
                overrides.has_dither = true;
            }
            if (table.contains("pipeline")
             && !ParsePipeline(table.at("pipeline"), overrides, error))
                return false;

            return true;
        }

        bool ParseTextures(const toml::value &root,
                           MaterialRecipe &recipe,
                           ParseErrorContext &error)
        {
            if (!root.contains("textures"))
                return true;

            ParseTableScope scope(error, "textures");
            const toml::value &textures = root.at("textures");
            if (!textures.is_array())
            {
                error.Record(nullptr, nullptr, "必须是纹理表数组");
                return false;
            }

            const char *const keys[] = {"name", "asset", "required", "layer"};
            for (const toml::value &item : textures.as_array())
            {
                if (!ValidateKnownKeys(item, keys, 4, error))
                    return false;

                AnsiString name;
                AnsiString asset;
                bool required = true;
                uint32_t layer = 0;
                if (!ReadString(item, "name", name, error, true)
                 || !ReadString(item, "asset", asset, error, true)
                 || !ReadBool(item, "required", required, error)
                 || !ReadUnsigned(item, "layer", layer, error))
                    return false;

                if (!IsValidMaterialTextureName(name.c_str()))
                {
                    error.Record("name", name.c_str(), "非法纹理槽名称");
                    return false;
                }
                for (const RecipeTextureBinding &existing : recipe.textures)
                {
                    if (existing.texture_name == name.c_str())
                    {
                        error.Record("name", name.c_str(), "纹理槽重复");
                        return false;
                    }
                }

                RecipeTextureBinding binding{};
                binding.texture_name = name.c_str();
                binding.resource_id = asset.c_str();
                binding.required = required;
                binding.array_layer = layer;
                recipe.textures.emplace_back(std::move(binding));
            }
            return true;
        }

        bool ValidateTopLevelKeys(const toml::value &root,
                                  ParseErrorContext &error)
        {
            static const char *const keys[] = {
                "schema", "name", "material_definition", "transform",
                "render_state", "textures"
            };
            ParseTableScope scope(error, "root");
            return ValidateKnownKeys(root, keys, 6, error);
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
            else
            {
                AnsiString name;
                AnsiString definition_id;
                if (!ReadString(root, "name", name, error, true)
                 || !ReadString(root, "material_definition", definition_id, error, true))
                    result = MaterialRecipeFileParseResult::InvalidValue;
                else
                {
                    out_data.recipe.recipe_name = name.c_str();
                    out_data.recipe.mtl_def_id = definition_id.c_str();

                    if (!ParseTransform(root, out_data.recipe, error)
                     || !ParseRenderState(root, out_data.recipe.render_state_overrides, error)
                     || !ParseTextures(root, out_data.recipe, error))
                        result = error.unknown_key
                            ? MaterialRecipeFileParseResult::UnknownKey
                            : MaterialRecipeFileParseResult::InvalidValue;
                    else if (!ValidateTopLevelKeys(root, error))
                        result = MaterialRecipeFileParseResult::UnknownKey;
                }
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
