#include <hgl/mtl/MaterialDefinitionFile.h>
#include <hgl/mtl/MaterialDomain.h>
#include <hgl/mtl/MaterialStageInterface.h>

#include <hgl/log/Log.h>
#include <hgl/mtl/VkEnumText.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/type/Smart.h>
#include <toml/toml.hpp>

#include <cstdio>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        // ── 解析失败诊断上下文（2026-10-12）────────────────────────────────
        // 口径：材质配置写错必须能 LOG 到"表/键/值"。解析器内部失败点极多且都以
        // `return false` 收敛，故用**读值/校验 helper 统一记录**：谁先失败谁写入
        // 上下文（只记第一个 = 根因）。上下文是**纯诊断**，不参与任何控制流判定。
        // 置位用 RAII 作用域（thread_local：解析发生在加载线程；嵌套调用不会串）。
        struct ParseErrorContext
        {
            // 表路径栈（B2 2026-10-14）：失败记录里带**全路径**（如 `render_state.pipeline.cull_mode`）。
            // 原实现是"粘性单值 + 只有键校验器调用"（值解析失败时路径为空，或继承上一步的陈旧
            // 表名 = 指错位置）。改栈 + RAII（ParseTableScope）后，路径 = 出事键真正所属的表。
            static constexpr uint32_t kMaxTableDepth = 8;
            const char *table_stack[kMaxTableDepth] = {};
            uint32_t    table_depth = 0;

            std::string table;                // 快照：第一个失败点所在的表（全路径）
            std::string key;
            std::string value;
            std::string detail;
            int         line = 0;
            bool        recorded = false;

            void PushTable(const char *name) noexcept
            {
                if (name && name[0] && table_depth < kMaxTableDepth)
                    table_stack[table_depth++] = name;
            }

            void PopTable() noexcept
            {
                if (table_depth > 0)
                    --table_depth;
            }

            void BuildTablePath(std::string &out) const
            {
                out.clear();
                for (uint32_t i = 0; i < table_depth; ++i)
                {
                    if (i > 0)
                        out += '.';
                    out += table_stack[i];
                }
            }

            // 记第一个失败点（后续失败点忽略：根因优先）
            void Record(const char *in_key, const char *in_value, const char *in_detail)
            {
                if (recorded)
                    return;

                BuildTablePath(table);
                key = in_key ? in_key : "";
                value = in_value ? in_value : "";
                detail = in_detail ? in_detail : "";
                recorded = true;
            }

            void RecordLine(const int in_line, const char *in_detail)
            {
                if (recorded)
                    return;

                line = in_line;
                Record(nullptr, nullptr, in_detail);
            }
        };

        thread_local ParseErrorContext *g_parse_error_context = nullptr;

        struct ParseErrorContextScope
        {
            explicit ParseErrorContextScope(ParseErrorContext *context) noexcept
            {
                g_parse_error_context = context;
            }
            ~ParseErrorContextScope() { g_parse_error_context = nullptr; }

            ParseErrorContextScope(const ParseErrorContextScope &) = delete;
            ParseErrorContextScope &operator=(const ParseErrorContextScope &) = delete;
        };

        /// 表路径作用域（B2 2026-10-14）：进入一个 TOML 表 / 数组元素时压栈，离开时自动弹出。
        /// 值解析与键校验的失败因此都能报出**全路径**；嵌套用多个作用域表达
        ///（`ParseTableScope("render_state")` + `ParseTableScope("pipeline")` ⇒ `render_state.pipeline`）。
        /// 数组元素用索引形态：`ParseTableScope("textures", i)` ⇒ `resources.textures[2]`。
        class ParseTableScope
        {
            char indexed_label[64] = {};
            bool pushed = false;

        public:
            explicit ParseTableScope(const char *name) noexcept
            {
                Push(name);
            }

            ParseTableScope(const char *name, const uint32_t index) noexcept
            {
                std::snprintf(indexed_label, sizeof(indexed_label), "%s[%u]",
                              name ? name : "", index);
                Push(indexed_label);
            }

            ~ParseTableScope()
            {
                if (pushed && g_parse_error_context)
                    g_parse_error_context->PopTable();
            }

            ParseTableScope(const ParseTableScope &) = delete;
            ParseTableScope &operator=(const ParseTableScope &) = delete;

        private:
            void Push(const char *name) noexcept
            {
                if (g_parse_error_context && name && name[0])
                {
                    g_parse_error_context->PushTable(name);
                    pushed = true;
                }
            }
        };

        inline void RecordParseFailure(const char *in_key,
                                       const char *in_value = nullptr,
                                       const char *in_detail = nullptr)
        {
            if (g_parse_error_context)
                g_parse_error_context->Record(in_key, in_value, in_detail);
        }

        inline void RecordParseSyntaxError(const int line, const char *detail)
        {
            if (g_parse_error_context)
                g_parse_error_context->RecordLine(line, detail);
        }

        bool ReadString(const toml::value &table, const char *key, AnsiString &out)
        {
            if (!table.is_table() || !table.contains(key) || !table.at(key).is_string())
            {
                RecordParseFailure(key);
                return false;
            }
            const std::string &value = table.at(key).as_string();
            out = value.c_str();
            return true;
        }

        bool ReadString(const toml::value &table, const char *key, std::string &out)
        {
            if (!table.is_table() || !table.contains(key) || !table.at(key).is_string())
            {
                RecordParseFailure(key);
                return false;
            }
            out = table.at(key).as_string();
            return true;
        }

        bool ReadBool(const toml::value &table, const char *key, bool &out)
        {
            if (!table.is_table() || !table.contains(key) || !table.at(key).is_boolean())
            {
                RecordParseFailure(key);
                return false;
            }
            out = table.at(key).as_boolean();
            return true;
        }

        bool ReadFloat(const toml::value &table, const char *key, float &out)
        {
            if (!table.is_table() || !table.contains(key))
            {
                RecordParseFailure(key);
                return false;
            }
            const toml::value &value = table.at(key);
            if (value.is_floating())
                out = static_cast<float>(value.as_floating());
            else if (value.is_integer())
                out = static_cast<float>(value.as_integer());
            else
            {
                RecordParseFailure(key);
                return false;
            }
            return true;
        }

        bool ReadRequiredString(const toml::value &table, const char *key, std::string &out)
        {
            if (!ReadString(table, key, out))
                return false;
            if (out.empty())
            {
                RecordParseFailure(key, "", "必须是非空字符串");
                return false;
            }
            return true;
        }

        bool ParseRenderState(const toml::value &root, MaterialDefinition &definition)
        {
            if (!root.contains("render_state"))
                return true;

            const toml::value &render_state = root.at("render_state");
            if (!render_state.is_table())
                return false;

            // 本函数体的全部键都在 [render_state] 下（pipeline 子表另开一层作用域）。
            ParseTableScope render_state_table("render_state");

            if (render_state.contains("alpha_test")
             && !ReadBool(render_state, "alpha_test",
                          definition.default_render_state.alpha_test))
                return false;
            if (render_state.contains("alpha_cutoff")
             && !ReadFloat(render_state, "alpha_cutoff",
                           definition.default_render_state.alpha_cutoff))
                return false;
            if (render_state.contains("dither")
             && !ReadBool(render_state, "dither",
                          definition.default_render_state.dither))
                return false;

            if (!render_state.contains("pipeline"))
                return true;
            const toml::value &pipeline = render_state.at("pipeline");
            if (!pipeline.is_table())
                return false;

            ParseTableScope pipeline_table("pipeline");

            std::string value;
            if (pipeline.contains("cull_mode"))
            {
                if (!ReadString(pipeline, "cull_mode", value))
                    return false;
                if (!ParseVkCullMode(value.c_str(), definition.default_render_state.pipeline_config.cull_mode))
                {
                    RecordParseFailure("cull_mode", value.c_str(), "未知值（见 inc/hgl/mtl/VkEnumText.h）");
                    return false;
                }
            }
            if (pipeline.contains("depth_compare_op"))
            {
                if (!ReadString(pipeline, "depth_compare_op", value))
                    return false;
                if (!ParseVkCompareOp(value.c_str(), definition.default_render_state.pipeline_config.depth_compare_op))
                {
                    RecordParseFailure("depth_compare_op", value.c_str(), "未知值（见 inc/hgl/mtl/VkEnumText.h）");
                    return false;
                }
            }
            if (pipeline.contains("blend_src"))
            {
                if (!ReadString(pipeline, "blend_src", value))
                    return false;
                if (!ParseVkBlendFactor(value.c_str(), definition.default_render_state.pipeline_config.blend_src))
                {
                    RecordParseFailure("blend_src", value.c_str(), "未知值（见 inc/hgl/mtl/VkEnumText.h）");
                    return false;
                }
            }
            if (pipeline.contains("blend_dst"))
            {
                if (!ReadString(pipeline, "blend_dst", value))
                    return false;
                if (!ParseVkBlendFactor(value.c_str(), definition.default_render_state.pipeline_config.blend_dst))
                {
                    RecordParseFailure("blend_dst", value.c_str(), "未知值（见 inc/hgl/mtl/VkEnumText.h）");
                    return false;
                }
            }

            bool &depth_test = definition.default_render_state.pipeline_config.depth_test;
            bool &depth_write = definition.default_render_state.pipeline_config.depth_write;
            bool &alpha_blend = definition.default_render_state.pipeline_config.alpha_blend;
            bool &dynamic_line_width =
                definition.default_render_state.pipeline_config.dynamic_line_width;
            bool &overlay = definition.default_render_state.pipeline_config.overlay;
            bool &wireframe = definition.default_render_state.pipeline_config.wireframe;
            if (pipeline.contains("depth_test") && !ReadBool(pipeline, "depth_test", depth_test))
                return false;
            if (pipeline.contains("depth_write") && !ReadBool(pipeline, "depth_write", depth_write))
                return false;
            if (pipeline.contains("alpha_blend") && !ReadBool(pipeline, "alpha_blend", alpha_blend))
                return false;
            if (pipeline.contains("dynamic_line_width")
             && !ReadBool(pipeline, "dynamic_line_width", dynamic_line_width))
                return false;
            if (pipeline.contains("overlay") && !ReadBool(pipeline, "overlay", overlay))
                return false;
            if (pipeline.contains("wireframe") && !ReadBool(pipeline, "wireframe", wireframe))
                return false;
            if (pipeline.contains("line_width")
             && !ReadFloat(pipeline, "line_width",
                           definition.default_render_state.pipeline_config.line_width))
                return false;
            return true;
        }

        bool ParseTransformGraph(const toml::value &table,
                                 VertexShaderNodeConfig &out)
        {
            ParseTableScope transform_table("transform");

            std::string value;

            if (!ReadRequiredString(table, "source", value))
                return false;
            if (!ParseVertexInputMode(value.c_str(), out.input))
            {
                RecordParseFailure("source", value.c_str(), "未知顶点输入模式名");
                return false;
            }

            if (!ReadRequiredString(table, "mapping", value))
                return false;
            if (!ParsePositionMappingMode(value.c_str(), out.position_mapping))
            {
                RecordParseFailure("mapping", value.c_str(), "未知位置映射模式名");
                return false;
            }

            if (!ReadRequiredString(table, "orientation", value))
                return false;
            if (!ParseOrientationMode(value.c_str(), out.orientation))
            {
                RecordParseFailure("orientation", value.c_str(), "未知朝向模式名");
                return false;
            }

            if (!ReadRequiredString(table, "scale", value))
                return false;
            if (!ParseScaleMode(value.c_str(), out.scale))
            {
                RecordParseFailure("scale", value.c_str(), "未知缩放入参模式名");
                return false;
            }

            if (!ReadRequiredString(table, "projection", value))
                return false;
            if (!ParseProjectionMode(value.c_str(), out.projection))
            {
                RecordParseFailure("projection", value.c_str(), "未知投影模式名");
                return false;
            }

            return true;
        }

        bool ParseSemantic(const std::string &name, GlslModuleSemantic &out)
        {
            // T1：语义名字单一真源 GlslModule.h（GetGlslModuleSemanticName/
            // ParseGlslModuleSemantic）——此处不再维护平行表。
            return ParseGlslModuleSemantic(name.c_str(), out);
        }

        bool ParseGlobalSSBOType(const std::string &name, GlobalSSBOType &out)
        {
            for (uint32 i = 0; i < static_cast<uint32>(GlobalSSBOType::RANGE_SIZE); ++i)
            {
                const GlobalSSBOType type = static_cast<GlobalSSBOType>(i);
                if (name == GetGlobalSSBOTypeName(type))
                {
                    out = type;
                    return true;
                }
            }
            return false;
        }


        bool ValidateTextureDeclarationKeys(const toml::value &item)
        {
            if (!item.is_table())
                return false;

            for (const auto &pair : item.as_table())
            {
                const std::string &key = pair.first;
                if (key != "name"
                 && key != "sampler"
                 && key != "required"
                 && key != "channels")
                    return false;
            }
            return true;
        }

        bool ParseDefinition(const toml::value &root, MaterialDefinitionFileData &out)
        {
            std::string id;

            if (!ReadRequiredString(root, "id", id))
                return false;

            out.definition.definition_id = id;

            // `name` 缺省 = `id`（阶段四：资产只填领域 + 值 + 图）。
            // 现存资产里两者逐字相同，重复声明只制造「同 id 不同名」的机会；
            // 少数需要人类可读名的内建定义（如 Text2D_GPU）显式写即可。
            if (root.contains("name"))
            {
                std::string name;
                if (!ReadRequiredString(root, "name", name))
                    return false;
                out.definition.definition_name = name;
            }
            else
                out.definition.definition_name = id;

            // ── 表面领域（可选）────────────────────────────────────────────────
            // `domain = "StandardOpaque"` 触发契约校验；缺省 = Custom（不校验）。
            // 契约不满足时在这里就失败：这是「有限领域」取代「任意组合」的
            // 强制点——不许把非法形态放到运行时再兜底。
            if (root.contains("domain"))
            {
                std::string domain_name;
                if (!ReadRequiredString(root, "domain", domain_name)
                 || !ParseMaterialDomain(domain_name.c_str(), out.definition.domain))
                {
                    RecordParseFailure("domain", domain_name.c_str(),
                                       "未知表面领域名（见 MaterialDomain.h 的 GetMaterialDomainName）");
                    return false;
                }
            }
            else
                out.definition.domain = MaterialDomain::Custom;

            // ── 顶点提供策略（领域材质可缺省）────────────────────────────────────
            // 「顶点从哪来」是领域的属性（`require_allow_derived`），不是单个材质的
            // 选择；资产再声明一遍只会与契约分叉。非领域材质（Custom）没有契约可依，
            // 仍必须显式声明。
            if (root.contains("provider_policy"))
            {
                std::string policy;
                if (!ReadRequiredString(root, "provider_policy", policy))
                    return false;
                if (!ParseMaterialVertexProviderPolicy(policy.c_str(), out.definition.vertex_provider_policy))
                {
                    RecordParseFailure("provider_policy", policy.c_str(),
                                       "未知策略名（见 MaterialRecipe.h 的 GetMaterialVertexProviderPolicyName）");
                    return false;
                }
            }
            else
            {
                const MaterialDomainContract *contract =
                    GetMaterialDomainContract(out.definition.domain);
                if (!contract)
                {
                    RecordParseFailure("provider_policy", "",
                                       "非领域材质（未声明 domain）必须显式声明顶点提供策略");
                    return false;
                }
                out.definition.vertex_provider_policy =
                    contract->require_allow_derived
                        ? MaterialVertexProviderPolicy::AllowDerived
                        : MaterialVertexProviderPolicy::GeometryOnly;
            }

            if (root.contains("transform"))
            {
                if (!ParseTransformGraph(
                        root.at("transform"), out.definition.vertex_node_config))
                    return false;
            }
            else
            {
                // `[transform]` 缺省 = 采用领域规范变换（领域即材质：顶点通路由领域决定）。
                // 非领域材质（domain 缺省 = Custom）保留结构体默认值，与历史行为一致。
                GetMaterialDomainCanonicalVertexNodeConfig(
                    out.definition.domain, out.definition.vertex_node_config);
            }

            const toml::value *fragment = root.contains("fragment")
                ? &root.at("fragment") : nullptr;
            std::string value;
            if (fragment)
            {
                ParseTableScope fragment_table("fragment");

                if (fragment->contains("material_source_module_name"))
                {
                    if (!ReadRequiredString(*fragment, "material_source_module_name", value))
                        return false;
                    out.material_source_module_name_storage = value.c_str();
                    out.definition.material_source_module_name =
                        out.material_source_module_name_storage.c_str();
                }
                if (fragment->contains("surface_module_name"))
                {
                    if (!ReadRequiredString(*fragment, "surface_module_name", value))
                        return false;
                    out.surface_module_name_storage = value.c_str();
                    out.definition.surface_module_name =
                        out.surface_module_name_storage.c_str();
                }
                if (fragment->contains("ntb_module_name"))
                {
                    if (!ReadRequiredString(*fragment, "ntb_module_name", value))
                        return false;
                    out.ntb_module_name_storage = value.c_str();
                    out.definition.ntb_module_name =
                        out.ntb_module_name_storage.c_str();
                }
            }
            else
            {
                // `[fragment]` 缺省 = 模块全部由领域契约给出（阶段四：资产只填
                // 领域 + 值 + 图）。非领域材质没有契约可依，必须自己写。
                if (!GetMaterialDomainContract(out.definition.domain))
                {
                    RecordParseFailure("fragment", "",
                                       "非领域材质（未声明 domain）必须声明 [fragment]");
                    return false;
                }
            }

            // 空 `[fragment]` 表（只有 `surface_module_name` 之类）不算声明了表面来源：
            // 领域材质由契约补齐，Custom 没有契约 ⇒ 必须在这里挡住，否则会带着
            // 空模块名一路走到着色器拼装期才炸。
            if (!out.definition.material_source_module_name
             && !GetMaterialDomainContract(out.definition.domain))
            {
                RecordParseFailure("fragment", "material_source_module_name",
                                   "非领域材质（未声明 domain）必须声明表面来源模块");
                return false;
            }

            if (!ParseRenderState(root, out.definition))
                return false;

            const toml::value *vertex = root.contains("vertex") ? &root.at("vertex") : nullptr;
            if (vertex && !vertex->is_table())
            {
                RecordParseFailure("vertex", "", "必须是表");
                return false;
            }
            {
                ParseTableScope vertex_table("vertex");

                if (vertex && vertex->contains("normal_mode"))
                {
                    std::string normal_mode;
                    if (!ReadRequiredString(*vertex, "normal_mode", normal_mode))
                        return false;
                    if (!ParseMaterialVertexNormalMode(
                            normal_mode.c_str(), out.definition.vertex_normal_mode))
                    {
                        RecordParseFailure("normal_mode", normal_mode.c_str(), "未知法线模式名");
                        return false;
                    }
                }

                // `requirements` 缺省 = 由领域契约推导（`required_vertex_semantic_mask`）。
                // 它曾经是必填项，且缺失时静默 `return false`——一个只能靠猜的陷阱；
                // 现在显式声明依然接受（Custom 材质必须显式），领域材质可以不写。
                //
                // 注意：推导结果**不**与 `varyings` 取交集。领域契约的语义位是
                // **顶点 ABI**（同一领域必须同布局才能共享 layout/合批），而
                // `varyings` 只决定着色器发什么插值；两者不是一回事。把不发
                // `emit_world_normal` 的 StandardOpaque 材质的 Normal 裁掉，会让
                // 它和同领域其它材质的顶点布局分叉 —— 领域契约校验会直接拒绝。
                if (vertex && vertex->contains("requirements"))
                {
                    if (!vertex->at("requirements").is_array())
                    {
                        RecordParseFailure("requirements", "", "必须是顶点语义名数组");
                        return false;
                    }
                    for (const auto &item : vertex->at("requirements").as_array())
                    {
                        if (!item.is_string())
                        {
                            RecordParseFailure("requirements", "", "数组元素必须是顶点语义名字符串");
                            return false;
                        }
                        GlslModuleSemantic semantic;
                        if (!ParseSemantic(item.as_string(), semantic))
                        {
                            RecordParseFailure("requirements", item.as_string().c_str(),
                                               "未知顶点语义名（见 GlslModule.h 的 GetGlslModuleSemanticName）");
                            return false;
                        }
                        GlslModuleSemanticRequirement requirement;
                        requirement.source = GlslModuleCapabilitySource::ProducedSemantic;
                        requirement.semantic = semantic;
                        out.definition.vertex_semantic_requirements.Add(requirement);
                    }
                }
                else if (!GetMaterialDomainVertexSemanticRequirements(
                             out.definition.domain,
                             out.definition.vertex_semantic_requirements))
                {
                    RecordParseFailure("requirements", "",
                                       "非领域材质（未声明 domain）必须显式声明顶点语义需求");
                    return false;
                }
            }

            bool has_texture_configuration_settings = false;
            // `[resources].material_data` 是否被作者显式声明过。
            // 只有「完全没写」才按领域契约兜底；显式写了别的类型要报错，不能悄悄覆盖。
            bool declares_material_data = false;
            if (root.contains("resources"))
            {
                ParseTableScope resources_table("resources");

                const toml::value &resources = root.at("resources");
                if (!resources.is_table())
                    return false;

                // 注：原 "ubos" 键随 Scene 集（viewport）退场而删除——definition 侧
                // 不再能声明任何描述符资源（viewport/sky/shadow 走 BDA，纹理/采样器走
                // bindless）⇒ 该键现在会被 ValidateKnownKeys 直接拒绝。

                if (resources.contains("code_modules"))
                {
                    ParseTableScope code_modules_table("code_modules");
                    if (!resources.at("code_modules").is_array())
                        return false;
                    for (const auto &item : resources.at("code_modules").as_array())
                    {
                        if (!item.is_string())
                            return false;
                        // 模块名即身份（注册表唯一键），字符串直存。
                        out.definition.code_module_requirements.emplace_back(
                            item.as_string().c_str());
                    }
                }

                if (resources.contains("material_data"))
                {
                    ParseTableScope material_data_table("material_data");
                    // 单一声明：一个材质只有一个私有数据 SSBO（MaterialPrivateData）。
                    // TOML 形态：material_data = { type = "EmissiveSurface" }
                    // 名字固定为 DefaultMaterialPrivateDataName；
                    // type 必须是材质 SSBO 类型。
                    const auto &item = resources.at("material_data");
                    if (!item.is_table())
                    {
                        RecordParseFailure("material_data", nullptr, "必须是内联表");
                        return false;
                    }
                    if (!item.contains("type") || !item.at("type").is_string())
                    {
                        RecordParseFailure("material_data.type", nullptr, "必须有字符串 type");
                        return false;
                    }
                    GlobalSSBOType type;
                    if (!ParseGlobalSSBOType(item.at("type").as_string(), type)
                     || !IsGlobalSSBOType(type))
                    {
                        RecordParseFailure("material_data.type", item.at("type").as_string().c_str(),
                                           "未知材质数据类型名（见 GlobalSSBOTypes.h 的 GetGlobalSSBOTypeName）");
                        return false;
                    }
                    out.definition.material_private_data = type;
                    declares_material_data = true;
                }

                if (resources.contains("textures"))
                {
                    ParseTableScope textures_table("textures");

                    if (!resources.at("textures").is_array())
                    {
                        RecordParseFailure("textures", nullptr, "必须是数组");
                        return false;
                    }
                    uint32_t texture_index = 0;
                    for (const auto &item : resources.at("textures").as_array())
                    {
                        // 数组元素带索引：路径形如 resources.textures[2].sampler。
                        ParseTableScope texture_item_table("textures", texture_index);
                        ++texture_index;
                        // 纹理声明的形态契约：键白名单 + name/sampler 为字符串 + required 为布尔。
                        // 逐项记录到键级：形态不符曾被静默拒绝，作者看不出哪里写错。
                        if (!ValidateTextureDeclarationKeys(item))
                            return false;

                        if (!item.contains("name") || !item.at("name").is_string())
                        {
                            RecordParseFailure("name", nullptr, "必须有字符串 name");
                            return false;
                        }
                        const std::string texture_name = item.at("name").as_string();
                        if (!IsValidMaterialTextureName(texture_name))
                        {
                            RecordParseFailure("name", texture_name.c_str(), "非法纹理声明名");
                            return false;
                        }

                        // ── 槽形态来自领域契约（阶段三之二）──────────────────────
                        // `sampler / required / channels` 三项在同一个领域里必然相同，
                        // 它们是「这个槽在领域里长什么样」，不是「这份资产想怎么用」。
                        // 故资产可以整段省略：解析层按契约兜底；显式写了不同值的，
                        // 由 ValidateMaterialDefinitionDomain 判 TextureSlotMismatch。
                        //
                        // 契约里没有这个槽（Custom 领域，或槽名不在白名单）时没有
                        // 兜底来源，仍按原样强制显式声明。
                        const MaterialDomainTextureSlot *contract_slot =
                            FindMaterialDomainTextureSlot(out.definition.domain,
                                                          texture_name.c_str());

                        SamplerType sampler = SamplerType::Sampler2D;
                        if (item.contains("sampler"))
                        {
                            if (!item.at("sampler").is_string())
                            {
                                RecordParseFailure("sampler", nullptr,
                                                   "sampler 必须是字符串（采样器类型名）");
                                return false;
                            }

                            // 采样器类型名走唯一真源（大小写不敏感）；未知名 ⇒ Error ⇒ 解析失败。
                            const std::string sampler_name = item.at("sampler").as_string();
                            sampler = ParseSamplerType(sampler_name.c_str());
                            if (sampler == SamplerType::Error)
                            {
                                RecordParseFailure(
                                    "sampler", sampler_name.c_str(),
                                    "未知采样器类型名（见 inc/hgl/common/TextureSamplerTypeDef.h）");
                                return false;
                            }
                        }
                        else if (contract_slot)
                            sampler = contract_slot->sampler;
                        else
                        {
                            RecordParseFailure(
                                "sampler", nullptr,
                                "必须有字符串 sampler（该槽不在任何领域契约内，无规范值可兜底）");
                            return false;
                        }

                        bool binding_required = false;
                        if (item.contains("required"))
                        {
                            if (!item.at("required").is_boolean())
                            {
                                RecordParseFailure("required", nullptr,
                                                   "required 必须是布尔值");
                                return false;
                            }
                            binding_required = item.at("required").as_boolean();
                        }
                        else if (contract_slot)
                            binding_required = contract_slot->binding_required;
                        else
                        {
                            RecordParseFailure(
                                "required", nullptr,
                                "必须有布尔 required（该槽不在任何领域契约内，无规范值可兜底）");
                            return false;
                        }

                        for (const auto &existing :
                             out.definition.texture_declarations)
                        {
                            if (existing.name == texture_name)
                                return false;
                        }

                        uint32_t declared_channels = contract_slot ? contract_slot->channels : 0;
                        if (item.contains("channels"))
                        {
                            // 可选：纹理通道数。0/缺省 = 由纹素格式决定；
                            // 2 = 双通道法线(BC5)：只存 XY，Z 由 shader 还原
                            // （程序构建阶段据此注入 MTL_TEX_<NAME>_CHANNELS）。
                            if (!item.at("channels").is_integer())
                            {
                                RecordParseFailure("channels", nullptr, "channels 必须是整数");
                                return false;
                            }
                            const int64 declared = item.at("channels").as_integer();
                            if (declared < 1 || declared > 4)
                            {
                                RecordParseFailure("channels", nullptr,
                                                   "channels 必须在 1..4 之间");
                                return false;
                            }
                            declared_channels = static_cast<uint32_t>(declared);
                        }

                        out.definition.texture_declarations.push_back(
                            {texture_name, sampler, binding_required, declared_channels});
                    }
                }

                if (resources.contains("texture_configurations"))
                {
                    const toml::value &configurations =
                        resources.at("texture_configurations");
                    if (!configurations.is_table()
                     || !configurations.contains("max_count")
                     || !configurations.at("max_count").is_integer())
                        return false;

                    const int64 max_count =
                        configurations.at("max_count").as_integer();
                    if (max_count <= 0
                     || static_cast<uint64>(max_count)
                        > static_cast<uint64>(hgl::HGL_U32_MAX))
                        return false;

                    out.definition.texture_configuration_max_count =
                        static_cast<uint32>(max_count);
                    has_texture_configuration_settings = true;
                }

                if (resources.contains("samplers"))
                {
                    if (!resources.at("samplers").is_array())
                        return false;
                    for (const auto &item : resources.at("samplers").as_array())
                    {
                        if (!item.is_string())
                            return false;
                        const std::string name = item.as_string();
                        if (name.empty())
                            return false;
                        for (const auto &existing : out.definition.sampler_names)
                        {
                            if (existing == name)
                                return false;
                        }
                        out.definition.sampler_names.push_back(name);
                    }
                }

                if (resources.contains("defines"))
                {
                    if (!resources.at("defines").is_array())
                        return false;
                    for (const auto &item : resources.at("defines").as_array())
                    {
                        if (!item.is_string())
                            return false;
                        const std::string name = item.as_string();
                        if (name.empty())
                            return false;
                        for (const auto &existing : out.definition.compile_defines)
                        {
                            if (existing == name)
                                return false;
                        }
                        out.definition.compile_defines.push_back(name);
                    }
                }
            }

            // ── 领域规范材质私有数据类型兜底（阶段三之二）───────────────────────
            // 类型是「这个领域的 payload 行长什么样」，不是美术参数，故资产不必写。
            // 只在作者完全没写 `[resources].material_data` 时兜底；显式写了别的类型
            // 保留原值交给 ValidateMaterialDefinitionDomain 判 MaterialDataTypeMismatch
            // （悄悄覆盖会让作者以为自己写对了）。
            if (!declares_material_data)
            {
                GlobalSSBOType canonical_type;
                if (GetMaterialDomainCanonicalMaterialDataType(out.definition.domain,
                                                               canonical_type))
                    out.definition.material_private_data = canonical_type;
            }

            if (has_texture_configuration_settings
             && out.definition.texture_declarations.empty())
                return false;

            if (root.contains("vertex") && root.at("vertex").contains("varyings"))
            {
                const toml::value &varyings = root.at("vertex").at("varyings");
                if (!varyings.is_array())
                    return false;
                // T2：varying 字段表驱动（成员指针表——字段名单一真源在此表，
                // 新增 varying 只需加一行）
                struct VaryingFieldEntry
                {
                    const char *name;
                    bool MaterialVertexVaryingConfig::*field;
                };
                static const VaryingFieldEntry kVaryingFieldTable[] = {
                    { "emit_data_index_id",             &MaterialVertexVaryingConfig::emit_data_index_id },
                    { "emit_vertex_color",              &MaterialVertexVaryingConfig::emit_vertex_color },
                    { "emit_uv0",                       &MaterialVertexVaryingConfig::emit_uv0 },
                    { "emit_world_pos",                 &MaterialVertexVaryingConfig::emit_world_pos },
                    { "emit_world_normal",              &MaterialVertexVaryingConfig::emit_world_normal },
                    { "emit_luminance",                 &MaterialVertexVaryingConfig::emit_luminance },
                    { "emit_frag_direction",            &MaterialVertexVaryingConfig::emit_frag_direction },
                    { "use_transform_id_attr",          &MaterialVertexVaryingConfig::use_transform_id_attr },
                    { "emit_vertex_color_from_palette", &MaterialVertexVaryingConfig::emit_vertex_color_from_palette },
                    { "emit_style_id",                  &MaterialVertexVaryingConfig::emit_style_id },
                };
                for (const auto &item : varyings.as_array())
                {
                    if (!item.is_string())
                        return false;
                    const std::string field = item.as_string();
                    bool found = false;
                    for (const VaryingFieldEntry &entry : kVaryingFieldTable)
                    {
                        if (field == entry.name)
                        {
                            out.definition.vertex_varying.*(entry.field) = true;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        return false;
                }
            }
            else
            {
                // 资产未显式声明 [vertex] varyings 时：由领域契约自动推导规范 Varying 掩码。
                if (out.definition.domain != MaterialDomain::Custom)
                {
                    const InterStageSemanticMask default_mask =
                        GetMaterialDomainDefaultVaryingMask(out.definition.domain);
                    if (default_mask != 0)
                    {
                        BuildMaterialVertexVaryingFromSemanticMask(
                            default_mask, out.definition.vertex_varying);
                        if (out.definition.domain == MaterialDomain::VertexPalette)
                        {
                            out.definition.vertex_varying.emit_vertex_color = false;
                            out.definition.vertex_varying.emit_vertex_color_from_palette = true;
                            out.definition.vertex_varying.use_transform_id_attr = true;
                        }
                    }
                }
            }

            // ── [mesh_shader] 段（可选）──────────────────────────────
            if (root.contains("mesh_shader"))
            {
                ParseTableScope mesh_shader_table("mesh_shader");

                const toml::value &ms = root.at("mesh_shader");
                if (ms.contains("mode"))
                {
                    std::string mode_str;
                    if (!ReadRequiredString(ms, "mode", mode_str))
                        return false;
                    if (!ParseMeshShaderMode(mode_str.c_str(), out.definition.mesh_shader_mode))
                    {
                        // 未知模式名 = 拼写错误，硬失败而非静默降级
                        RecordParseFailure("mode", mode_str.c_str(),
                                           "未知 mesh shader 模式名（见 MeshShaderMode.h 的 GetMeshShaderModeName）");
                        return false;
                    }
                }
                if (ms.contains("max_invocations"))
                {
                    const auto &val = ms.at("max_invocations");
                    if (!val.is_integer())
                        return false;
                    out.definition.mesh_shader_max_invocations =
                        static_cast<uint32_t>(val.as_integer());
                }
            }

            return IsValidMaterialDefinitionFileData(out);
        }
    }

    const char *GetMaterialDefinitionFileParseResultName(
        const MaterialDefinitionFileParseResult result) noexcept
    {
        switch (result)
        {
        case MaterialDefinitionFileParseResult::Skipped: return "Skipped";
        case MaterialDefinitionFileParseResult::OK: return "OK";
        case MaterialDefinitionFileParseResult::InvalidValue: return "InvalidValue";
        case MaterialDefinitionFileParseResult::UnknownKey: return "UnknownKey";
        default: return "Invalid";
        }
    }

    // 层内键白名单校验：解析器不认识的键 → false。未知键 = 作者笔误或已删字段
    // 残留（如 usage），静默吞掉会让作者以为配置生效而实际被忽略——TOML 化
    // 材质最危险的失效模式，故显式报错。
    static bool ValidateKnownKeys(
        const toml::value &table,
        std::initializer_list<const char *> allowed) noexcept
    {
        if (!table.is_table())
            return true;
        const auto &tbl = table.as_table();
        for (const auto &pair : tbl)
        {
            const std::string &key = pair.first;
            bool known = false;
            for (const char *name : allowed)
            {
                if (key == name)
                {
                    known = true;
                    break;
                }
            }
            if (!known)
            {
                const char *detail = "未知键（拼写错误或已删字段残留）";
                if (key == "material_source_module")
                    detail = "schema 4 请使用 material_source_module_name（@ulre name）";
                else if (key == "ntb_module")
                    detail = "schema 4 请使用 ntb_module_name（@ulre name）";
                else if (key == "surface_module_include")
                    detail = "schema 4 只使用 surface_module_name，路径由注册表解析";
                else if (key == "ambient_module_name")
                    detail = "ambient provider 已上移到环境层，用 EnvironmentSystem::SetAmbientProvider()";
                RecordParseFailure(key.c_str(), nullptr, detail);
                return false;
            }
        }
        return true;
    }

    // ── 领域规范模块兜底（阶段四：资产只填领域 + 值 + 图）────────────────────────
    // 前置条件：`ValidateMaterialDefinitionDomain()` 已返回 OK。
    // 只填空缺，绝不覆盖作者显式值——校验阶段已经保证「显式值必须等于规范值」，
    // 所以这里覆盖与否结果相同，但保留显式值更利于诊断信息还原。
    static void ApplyDomainModuleDefaults(MaterialDefinitionFileData &out) noexcept
    {
        if (out.definition.domain == MaterialDomain::Custom)
            return;

        if (!out.definition.material_source_module_name
            || !*out.definition.material_source_module_name)
        {
            if (const char *source =
                    GetMaterialDomainReferenceMaterialSource(out.definition.domain))
            {
                out.material_source_module_name_storage = source;
                out.definition.material_source_module_name =
                    out.material_source_module_name_storage.c_str();
            }
        }

        if (!out.definition.ntb_module_name || !*out.definition.ntb_module_name)
        {
            if (const char *ntb = GetMaterialDomainReferenceNtb(out.definition.domain))
            {
                out.ntb_module_name_storage = ntb;
                out.definition.ntb_module_name = out.ntb_module_name_storage.c_str();
            }
        }
    }

    // 顶层 + 各子层键白名单（与 ParseDefinition/ParseRenderState 支持的键逐项对应）
    static bool ValidateMaterialDefinitionKeys(const toml::value &root) noexcept
    {
        if (!ValidateKnownKeys(root, {
                "schema", "id", "name", "provider_policy", "domain",
                "transform", "fragment", "vertex", "resources",
                "mesh_shader", "render_state"}))
            return false;

        if (root.contains("transform"))
        {
            ParseTableScope validate_transform_table("transform");
            if (!ValidateKnownKeys(root.at("transform"), {
                    "source", "mapping", "orientation", "projection", "scale"}))
                return false;
        }

        if (root.contains("fragment"))
        {
            ParseTableScope validate_fragment_table("fragment");
            if (!ValidateKnownKeys(root.at("fragment"), {
                    "material_source_module_name", "ntb_module_name",
                    "surface_module_name"}))
                return false;
        }

        if (root.contains("vertex"))
        {
            ParseTableScope validate_vertex_table("vertex");
            if (!ValidateKnownKeys(root.at("vertex"), {
                    "requirements", "normal_mode", "varyings"}))
                return false;
        }

        if (root.contains("resources"))
        {
            ParseTableScope validate_resources_table("resources");

            const toml::value &resources = root.at("resources");
            if (!resources.is_table()
             || !ValidateKnownKeys(resources, {
                    "material_data", "samplers", "textures",
                    "texture_configurations", "defines"}))
                return false;

            if (resources.contains("texture_configurations"))
            {
                ParseTableScope validate_texture_configurations_table("texture_configurations");
                if (!resources.at("texture_configurations").is_table()
                 || !ValidateKnownKeys(
                        resources.at("texture_configurations"), {"max_count"}))
                    return false;
            }
        }

        if (root.contains("mesh_shader"))
        {
            ParseTableScope validate_mesh_shader_table("mesh_shader");
            if (!ValidateKnownKeys(root.at("mesh_shader"), {
                    "mode", "max_invocations"}))
                return false;
        }

        if (root.contains("render_state"))
        {
            ParseTableScope validate_render_state_table("render_state");

            const toml::value &rs = root.at("render_state");
            if (!ValidateKnownKeys(rs, {
                    "alpha_test", "alpha_cutoff", "dither", "pipeline"}))
                return false;

            if (rs.contains("pipeline"))
            {
                ParseTableScope validate_pipeline_table("pipeline");
                if (!ValidateKnownKeys(rs.at("pipeline"), {
                        "cull_mode", "depth_compare_op", "blend_src", "blend_dst",
                        "depth_test", "depth_write", "alpha_blend",
                        "dynamic_line_width", "overlay", "wireframe", "line_width"}))
                    return false;
            }
        }

        return true;
    }

    MaterialDefinitionFileParseResult ParseMaterialDefinitionFile(
        const char *content,
        const int content_size,
        MaterialDefinitionFileData &out_data) noexcept
    {
        out_data = MaterialDefinitionFileData{};
        if (!content || content_size <= 0)
            return MaterialDefinitionFileParseResult::Skipped;

        ParseErrorContext parse_error_context;
        MaterialDefinitionFileParseResult result = MaterialDefinitionFileParseResult::OK;

        {
            ParseErrorContextScope parse_error_scope(&parse_error_context);

            try
            {
                const toml::value root = toml::parse_str(
                    std::string(content, static_cast<size_t>(content_size)));

                if (!root.is_table() || !root.contains("schema")
                 || !root.at("schema").is_integer()
                 || root.at("schema").as_integer() != 4)
                {
                    RecordParseFailure("schema", nullptr, "必须存在且等于 4");
                    result = MaterialDefinitionFileParseResult::InvalidValue;
                }
                else if (!ParseDefinition(root, out_data))
                    result = MaterialDefinitionFileParseResult::InvalidValue;
                else if (!ValidateMaterialDefinitionKeys(root))
                    result = MaterialDefinitionFileParseResult::UnknownKey;
                else
                {
                    // 表面领域契约校验：只在显式声明了 domain 时生效。
                    // 放在键白名单校验之后，保证报出的是「契约」而不是「拼错键」。
                    AnsiString domain_detail;

                    if (ValidateMaterialDefinitionDomain(
                            out_data.definition, out_data.definition.domain,
                            domain_detail) != MaterialDomainValidationResult::OK)
                    {
                        RecordParseFailure("domain",
                                           GetMaterialDomainName(out_data.definition.domain),
                                           domain_detail.c_str());
                        result = MaterialDefinitionFileParseResult::InvalidValue;
                    }
                    else
                    {
                        // ── 领域规范模块兜底（阶段四）──────────────────────────
                        // 资产只写 `domain` + 数值 + 纹理；算法模块名由领域契约给出。
                        // 顺序不能反过来：校验必须看到作者原始声明，否则「自选算法」
                        // 会被兜底值覆盖而永远判不出来。
                        ApplyDomainModuleDefaults(out_data);
                    }
                }
            }
            catch (const toml::syntax_error &error)
            {
                // TOML 语法错误：报出行号 + 库给的原文（作者最常见的失效模式之一）
                int line = 0;
                const char *detail = nullptr;

                if (!error.errors().empty())
                {
                    const toml::error_info &info = error.errors().front();
                    if (!info.locations().empty())
                    {
                        const toml::source_location &location = info.locations().front().first;
                        line = static_cast<int>(location.first_line_number());
                        detail = info.locations().front().second.c_str();
                    }
                    else
                        detail = info.title().c_str();
                }

                RecordParseSyntaxError(line, detail);
                result = MaterialDefinitionFileParseResult::InvalidValue;
            }
            catch (const toml::exception &error)
            {
                RecordParseFailure(nullptr, nullptr, error.what());
                result = MaterialDefinitionFileParseResult::InvalidValue;
            }
        }

        if (result != MaterialDefinitionFileParseResult::OK)
        {
            // 诊断细节回传（表/键/值/行号）：调用方据此把日志报到配置项级别
            const ParseErrorContext &context = parse_error_context;

            out_data.parse_error.table = context.table;
            out_data.parse_error.key = context.key;
            out_data.parse_error.value = context.value;
            out_data.parse_error.detail = context.detail;
            out_data.parse_error.line = context.line;
        }

        return result;
    }

    bool IsValidMaterialDefinitionFileData(
        const MaterialDefinitionFileData &data) noexcept
    {
        const MaterialDefinition &definition = data.definition;
        return !definition.definition_id.empty()
            && !definition.definition_name.empty()
            && !definition.vertex_semantic_requirements.IsEmpty();
    }

    bool MaterialDefinitionFileRegistry::LoadFile(const OSString &path)
    {
        hgl::io::OpenFileInputStream opener(path);
        if (!opener)
        {
            GLogError(u8"[MaterialDefinitionFileRegistry] 材质定义打不开: %ls", path.c_str());
            return false;
        }
        const int64 size = opener->GetSize();
        if (size <= 0)
        {
            GLogError(u8"[MaterialDefinitionFileRegistry] 材质定义文件为空: %ls", path.c_str());
            return false;
        }

        hgl::AutoDeleteArray<char> buffer(size_t(size) + 1);
        if (!buffer || opener->Read(buffer.data(), size) != size)
        {
            GLogError(u8"[MaterialDefinitionFileRegistry] 材质定义读取失败: %ls", path.c_str());
            return false;
        }
        buffer[size_t(size)] = 0;

        return RegisterDefinitionSource(
            path.c_str(), buffer.data(), static_cast<int>(size));
    }

    bool MaterialDefinitionFileRegistry::RegisterDefinitionSource(
        const os_char *label, const char *content, const int content_size)
    {
        if (!content || content_size <= 0)
            return false;

        MaterialDefinitionFileData *data = files.Create();
        if (!data)
            return false;

        const MaterialDefinitionFileParseResult parse_result =
            ParseMaterialDefinitionFile(content, content_size, *data);
        if (parse_result != MaterialDefinitionFileParseResult::OK)
        {
            // 口径（2026-10-11 用户）：采样器等配置写错 = **LOG 报错**（不静默、不假装能编）。
            // 该定义**不注册** ⇒ 引用它的配方落到错误材质路径
            // （MaterialErrorKind::UnknownMaterialDefinition ⇒ 保底材质 + 按颜色标错误种类）。
            const MaterialDefinitionFileData::ParseErrorDetail &error = data->parse_error;
            std::string error_detail;

            if (error.line > 0)
                error_detail += " line=" + std::to_string(error.line);
            if (!error.key.empty())
            {
                error_detail += " ";
                if (!error.table.empty())
                    error_detail += error.table + ".";
                error_detail += error.key;
            }
            else if (!error.table.empty())
                error_detail += " " + error.table;
            if (!error.value.empty())
                error_detail += " = \"" + error.value + "\"";
            if (!error.detail.empty())
                error_detail += " (" + error.detail + ")";

            GLogError(u8"[MaterialDefinitionFileRegistry] 材质定义解析失败: %ls (%s)%s",
                      label, GetMaterialDefinitionFileParseResultName(parse_result),
                      error_detail.c_str());
            files.DeleteAt(files.GetCount() - 1);
            return false;
        }

        const int new_file_index = files.GetCount() - 1;
        for (int i = 0; i < new_file_index; ++i)
        {
            if (files[i]
             && files[i]->definition.definition_id
                    == data->definition.definition_id)
            {
                GLogError(u8"[MaterialDefinitionFileRegistry] 材质定义 ID 重复: %s (source=%ls)",
                          data->definition.definition_id.c_str(), label);
                files.DeleteAt(new_file_index);
                return false;
            }
        }
        return true;
    }

    bool MaterialDefinitionFileRegistry::LoadDirectory(
        const OSString &directory, int *out_file_count, int *out_error_count)
    {
        hgl::ValueArray<hgl::filesystem::FileInfo> file_list;
        const int scan_count = hgl::filesystem::GetFileInfoList(
            file_list, directory, true, true, true);
        if (scan_count < 0)
            return false;

        int file_count = 0;
        int error_count = 0;
        for (int i = 0; i < file_list.GetCount(); ++i)
        {
            const auto &file = file_list[i];
            if (!file.is_file)
                continue;
            const OSString name(file.name);
            // 阶段四：材质资产统一为材质实例格式 `*.mi.toml`（旧 `*.material.toml`
            // 是「母材质定义文件」，它随领域契约化而废止——不再扫描）。
            if (!name.EndsWith(OS_TEXT(".mi.toml")))
                continue;
            if (LoadFile(OSString(file.fullname)))
                ++file_count;
            else
                ++error_count;
        }

        if (error_count > 0)
            GLogError(u8"[MaterialDefinitionFileRegistry] 材质库加载完成：成功 %d 份、失败 %d 份"
                      u8"（失败项不注册 ⇒ 引用者走错误材质路径）", file_count, error_count);

        if (out_file_count) *out_file_count = file_count;
        if (out_error_count) *out_error_count = error_count;
        return true;
    }

    const MaterialDefinition *MaterialDefinitionFileRegistry::FindByID(
        const char *definition_id) const
    {
        if (!definition_id || !definition_id[0])
            return nullptr;
        for (int i = 0; i < files.GetCount(); ++i)
        {
            if (files[i] && files[i]->definition.definition_id == definition_id)
                return &files[i]->definition;
        }
        return nullptr;
    }
}
