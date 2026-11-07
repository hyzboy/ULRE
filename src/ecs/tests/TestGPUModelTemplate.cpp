/**
 * TestGPUModelTemplate —— GPU 常驻「模型模板池」的行布局、常量口径与镜像一致性（plan T5.1）。
 *
 * ── 为什么分三层 ────────────────────────────────────────────────────────────
 *
 * 模板池的真机落库（`ModelTemplateManager::RegisterModelTemplate`）要一张 VkBuffer，
 * 而 `src/ecs/tests` 下**没有图形设备**（同 `TestCSMIncrementalPass` /
 * `TestGlobalSSBORowRange` 的说明）⇒ 这里分三层把「布局真源」钉死：
 *
 * · A 层（纯数据，可完全执行）：两行结构的宽/对齐/字段偏移、默认值、标志位与
 *   LOD 级常量口径。这一层是 CPU 打包与 GLSL 读取之间的**唯一契约**，一旦漂移
 *   就是整片模型画错位置（且不会报错）。
 *
 * · B 层（GLSL 镜像逐字段对拍）：从 `ShaderLibrary/module/descriptor/ssbo/` 的
 *   生成物里解析出字段序与类型 → 累加 scalar 布局字节数，要求
 *   Σ == sizeof(C++ 行)。生成物是 codegen 产物，改了 C++ 忘了重跑 codegen，
 *   或者手改 .glsl，都会在这里当场炸（不需要设备、不需要编译着色器）。
 *
 * · C 层（源码契约）：真机才能观察的接线（池的 stride/保留行、BDA 地址回填、
 *   展开侧的 `组基址 + part.bucket_slot` 与"代理球更小不得误剔除"）用源码契约钉住。
 */

#include <hgl/graph/ssbo/InstanceCustomData.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/graph/ubo/ModelExpansionArena.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/type/ValueArray.h>

namespace
{
    using namespace hgl;
    using namespace hgl::graph;

    int g_failed = 0;

    void Check(const bool ok, const char *what)
    {
        if (!ok)
        {
            ++g_failed;
            GLogError(u8"TestGPUModelTemplate 失败: %s", what);
        }
    }

    // ── A 层：行布局 ────────────────────────────────────────────────────────

    bool TestRowLayout()
    {
        // 两步长必须均一（行号 × 128 = 字节偏移）—— 是"行号即地址"的全部前提。
        if (sizeof(ssbo::ModelTemplateRow) != 128 || sizeof(ssbo::ModelPartRow) != 128)
            return false;

        if (alignof(ssbo::ModelTemplateRow) != 16 || alignof(ssbo::ModelPartRow) != 16)
            return false;

        // 字段偏移：与 ModelTemplateRows.h 的 static_assert 同口径，但这里**运行期**
        // 再钉一遍（静态断言被谁改掉时会同时失效，运行期这一份不会）。
        const struct
        {
            bool        ok;
            const char *why;
        } kChecks[] =
        {
            { offsetof(ssbo::ModelTemplateRow, part_offset) == 0,
              "ModelTemplateRow.part_offset @0" },
            { offsetof(ssbo::ModelTemplateRow, part_count) == 4,
              "ModelTemplateRow.part_count @4" },
            { offsetof(ssbo::ModelTemplateRow, lod_proxy_template_ids) == 8,
              "ModelTemplateRow.lod_proxy_template_ids @8（3 个 u32 紧排）" },
            { offsetof(ssbo::ModelTemplateRow, lod_screen_ratios) == 20,
              "ModelTemplateRow.lod_screen_ratios @20（3 个 float 紧排）" },
            { offsetof(ssbo::ModelTemplateRow, shadow_proxy_template_id) == 32,
              "ModelTemplateRow.shadow_proxy_template_id @32" },
            { offsetof(ssbo::ModelTemplateRow, flags) == 36,
              "ModelTemplateRow.flags @36" },
            { offsetof(ssbo::ModelTemplateRow, part_bucket_count) == 40,
              "ModelTemplateRow.part_bucket_count @40" },
            { offsetof(ssbo::ModelTemplateRow, local_bound_center_radius) == 48,
              "ModelTemplateRow.local_bound_center_radius @48（16B 对齐）" },
            { offsetof(ssbo::ModelTemplateRow, local_bound_extents) == 64,
              "ModelTemplateRow.local_bound_extents @64" },

            { offsetof(ssbo::ModelPartRow, local_matrix) == 0,
              "ModelPartRow.local_matrix @0（mat4 = 64B）" },
            { offsetof(ssbo::ModelPartRow, aabb_center_radius) == 64,
              "ModelPartRow.aabb_center_radius @64" },
            { offsetof(ssbo::ModelPartRow, aabb_extents) == 80,
              "ModelPartRow.aabb_extents @80" },
            { offsetof(ssbo::ModelPartRow, geometry_id) == 96,
              "ModelPartRow.geometry_id @96" },
            { offsetof(ssbo::ModelPartRow, material_id) == 100,
              "ModelPartRow.material_id @100" },
            { offsetof(ssbo::ModelPartRow, part_flags) == 104,
              "ModelPartRow.part_flags @104" },
            { offsetof(ssbo::ModelPartRow, bucket_slot) == 108,
              "ModelPartRow.bucket_slot @108" },

            // T4.4：实例自定义数据独表（不挤进已被 128B 锁死的 EntityGPU 行）。
            { sizeof(ssbo::InstanceCustomData) == 16u,
              "InstanceCustomData 恰为 16 字节" },
            { alignof(ssbo::InstanceCustomData) == 16, "InstanceCustomData alignas(16)" },
            { offsetof(ssbo::InstanceCustomData, custom_color) == 0,
              "InstanceCustomData.custom_color @0" },
        };

        bool ok = true;
        for (const auto &c : kChecks)
        {
            if (c.ok)
                continue;

            ok = false;
            GLogError(u8"TestGPUModelTemplate 失败: %s", c.why);
        }

        return ok;
    }

    // ── A 层：默认值 / 标志位 / LOD 级常量 ──────────────────────────────────

    bool TestDefaultsAndConstants()
    {
        bool ok = true;

        // 行 0 = 无效模板哨兵；部件无槽哨兵不得与任何真实槽号（含 0）重叠。
        ok = ok && (ssbo::kInvalidModelTemplateID == 0u);
        ok = ok && (ssbo::kModelPartBucketSlotNone == 0xFFFFFFFFu);
        ok = ok && (ssbo::kInstanceCustomDataNone == 0u);

        // LOD 级命名：级 0 = 本体，级 1..3 = 三个代理槽。
        ok = ok && (ssbo::kModelLodProxyCount == 3u);
        ok = ok && (ssbo::kModelLodLevelBody == 0u);
        ok = ok && (ssbo::kModelLodLevelProxyBase == 1u);
        ok = ok && (ssbo::kModelLodLevelCount == 4u);

        // 模板标志位：互不重叠、非零。
        ok = ok && (ssbo::kModelTemplateFlagLodEnabled != 0u);
        ok = ok && (ssbo::kModelTemplateFlagShadowProxy != 0u);
        ok = ok && ((ssbo::kModelTemplateFlagLodEnabled & ssbo::kModelTemplateFlagShadowProxy) == 0u);
        ok = ok && (ssbo::kModelTemplateFlagUnused == 0u);

        // 部件标志位同上。
        ok = ok && (ssbo::kModelPartFlagCanCastShadow != 0u);
        ok = ok && (ssbo::kModelPartFlagIsOpaque != 0u);
        ok = ok && ((ssbo::kModelPartFlagCanCastShadow & ssbo::kModelPartFlagIsOpaque) == 0u);

        // 默认构造：一切都是"无模型"的恒等初值（展开侧不做任何事）。
        const ssbo::ModelTemplateRow tpl;
        ok = ok && (tpl.part_offset == 0u && tpl.part_count == 0u);
        ok = ok && (tpl.part_bucket_count == 0u);
        ok = ok && (tpl.shadow_proxy_template_id == 0u);
        ok = ok && (tpl.flags == 0u);
        ok = ok && (tpl.local_bound_center_radius.w == 0.0f);   // w=0 ⇒ 没有包围球

        for (uint32_t i = 0; i < ssbo::kModelLodProxyCount; ++i)
        {
            ok = ok && (tpl.lod_proxy_template_ids[i] == 0u);
            ok = ok && (tpl.lod_screen_ratios[i] == 0.0f);
        }

        // 部件行的 NSDMI 必须把"没有桶"设为初值（`Matrix4f local_matrix` 无初值 ⇒ 该行
        // 不能 const 默认构造，只能值初始化）。geometry_id==0 是展开侧的兜底跳过条件。
        ssbo::ModelPartRow part{};
        ok = ok && (part.geometry_id == 0u);
        ok = ok && (part.part_flags == 0u);
        ok = ok && (part.bucket_slot == ssbo::kModelPartBucketSlotNone);
        ok = ok && (ssbo::kModelPartBucketSlotNone != 0u);   // 无槽哨兵不得与真实槽号 0 撞

        // 展开竞技场：48 B 头 + 64 B 对齐段首（矩阵段/桶基址段按 16B 引用对齐）。
        ok = ok && (sizeof(ModelExpansionArena) == 48);
        ok = ok && (alignof(ModelExpansionArena) == 16);
        ok = ok && (kModelExpansionHeaderBytes >= sizeof(ModelExpansionArena));
        ok = ok && ((kModelExpansionHeaderBytes % 16u) == 0u);
        ok = ok && (offsetof(ModelExpansionArena, flags) == 0);
        ok = ok && (offsetof(ModelExpansionArena, l2w_base_row) == 4);
        ok = ok && (offsetof(ModelExpansionArena, l2w_row_capacity) == 8);
        ok = ok && (offsetof(ModelExpansionArena, bucket_base_count) == 12);
        ok = ok && (offsetof(ModelExpansionArena, bucket_base_addr) == 16);
        ok = ok && (offsetof(ModelExpansionArena, prev_l2w_base_row) == 24);
        ok = ok && (offsetof(ModelExpansionArena, custom_data_addr) == 32);

        // 桶基址表容量必须恰好容纳两个 pass 的整批实体（几何侧同一口径）。
        ok = ok && (kModelExpansionBucketBaseRowsPerPass == 4096u);
        ok = ok && (kModelExpansionMaxBucketBaseRows
                    == 2u * kModelExpansionBucketBaseRowsPerPass);

        // 逐实例个性化数据行：初值默认为白色不透明 (1,1,1,1)
        const ssbo::InstanceCustomData custom_default;
        ok = ok && (custom_default.custom_color.x == 1.0f
                 && custom_default.custom_color.y == 1.0f
                 && custom_default.custom_color.z == 1.0f
                 && custom_default.custom_color.w == 1.0f);

        const ssbo::InstanceCustomData custom_tint(math::Vector4f(0.5f, 0.2f, 0.8f, 0.5f));
        ok = ok && (custom_tint.custom_color.x == 0.5f
                 && custom_tint.custom_color.y == 0.2f
                 && custom_tint.custom_color.z == 0.8f
                 && custom_tint.custom_color.w == 0.5f);

        return ok;
    }

    bool TestCustomColorModulation()
    {
        // 模拟材质基础颜色与透明度调制逻辑（与 surface_material.glsl 完全等价）：
        // surface.baseColor = material.baseColor * si.vertexColor.rgb;
        // surface.alpha = material.alpha * si.vertexColor.a;
        // 网格阶段：fragVertexColor = GetVertexColor() * ResolveInstanceCustomColor(iid);
        const math::Vector3f mat_base_color(0.8f, 0.6f, 0.4f);
        const float mat_alpha = 0.9f;

        // 情况1：默认未设自定义数据（custom_color 恒等于 (1,1,1,1)），无损透传
        const ssbo::InstanceCustomData default_data;
        const math::Vector4f default_tint = default_data.custom_color;
        const math::Vector3f modulated_color_default(
            mat_base_color.x * default_tint.x,
            mat_base_color.y * default_tint.y,
            mat_base_color.z * default_tint.z);
        const float modulated_alpha_default = mat_alpha * default_tint.w;

        if (modulated_color_default.x != mat_base_color.x
         || modulated_color_default.y != mat_base_color.y
         || modulated_color_default.z != mat_base_color.z
         || modulated_alpha_default != mat_alpha)
        {
            GLogError(u8"TestCustomColorModulation: 默认颜色调制不符合恒等性");
            return false;
        }

        // 情况2：自定义着色与半透明调制
        const ssbo::InstanceCustomData custom_data(math::Vector4f(0.5f, 0.5f, 0.5f, 0.5f));
        const math::Vector4f custom_tint = custom_data.custom_color;
        const math::Vector3f modulated_color_custom(
            mat_base_color.x * custom_tint.x,
            mat_base_color.y * custom_tint.y,
            mat_base_color.z * custom_tint.z);
        const float modulated_alpha_custom = mat_alpha * custom_tint.w;

        const auto abs_diff = [](float a, float b) { return a > b ? a - b : b - a; };
        if (abs_diff(modulated_color_custom.x, 0.4f) > 1e-5f
         || abs_diff(modulated_color_custom.y, 0.3f) > 1e-5f
         || abs_diff(modulated_color_custom.z, 0.2f) > 1e-5f
         || abs_diff(modulated_alpha_custom, 0.45f) > 1e-5f)
        {
            GLogError(u8"TestCustomColorModulation: 自定义颜色与透明度调制数值不正确");
            return false;
        }

        return true;
    }

    // ── B 层：GLSL 镜像逐字段对拍 ───────────────────────────────────────────

    bool ReadSource(const OSString &path, AnsiString &out_text)
    {
        hgl::io::OpenFileInputStream fis(path);
        if (!fis || fis->GetSize() <= 0)
            return false;

        char chunk[4096];
        int64 got;
        while ((got = fis->Read(chunk, static_cast<int64>(sizeof(chunk)))) > 0)
            out_text.Strcat(chunk, static_cast<int>(got));

        return !out_text.IsEmpty();
    }

    /// GLSL 基本类型 → 字节数（`scalar` 布局：无隐式填充，宽度即尺寸）。
    uint32_t GlslTypeBytes(const char *type, const size_t len)
    {
        static const struct
        {
            const char *name;
            uint32_t    bytes;
        } kTypes[] =
        {
            { "uint",     4u  }, { "int",      4u }, { "float",    4u },
            { "uint64_t", 8u  }, { "int64_t",  8u },
            { "vec2",     8u  }, { "uvec2",    8u },
            { "vec4",    16u  }, { "uvec4",   16u }, { "ivec4",   16u },
            { "mat4",    64u  },
        };

        for (const auto &t : kTypes)
        {
            const size_t n = strlen(t.name);

            if (n == len && strncmp(type, t.name, n) == 0)
                return t.bytes;
        }

        return 0u;   // 未知类型 ⇒ 上层判失败
    }

    /// 从 `struct <name>` 花括号体里按行取字段并累加尺寸（`scalar` 布局下 Σ 宽度 == 行宽）。
    bool SumGlslStructBytes(const AnsiString &src, const char *struct_name,
                            uint32_t &out_bytes, uint32_t &out_fields)
    {
        const char *p = strstr(src.c_str(), struct_name);

        if (!p)
            return false;

        p = strchr(p, '{');

        if (!p)
            return false;

        ++p;

        out_bytes = 0;
        out_fields = 0;

        while (*p && *p != '}')
        {
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
                ++p;

            if (*p == '}')
                break;

            if (p[0] == '/' && p[1] == '/')
            {
                while (*p && *p != '\n')
                    ++p;
                continue;
            }

            const char *eol = p;
            while (*eol && *eol != '\n')
                ++eol;

            const char *semi = static_cast<const char *>(memchr(p, ';', size_t(eol - p)));

            if (!semi)
            {
                p = eol;
                continue;
            }

            // `类型 名字[N]`：类型 = 首个词，数组长度 = [] 内的十进制数。
            const char *type_begin = p;
            const char *type_end = p;
            while (type_end < semi && *type_end != ' ' && *type_end != '\t')
                ++type_end;

            uint32_t count = 1;
            for (const char *q = type_end; q < semi; ++q)
            {
                if (*q != '[')
                    continue;

                char digits[16] = {};
                int di = 0;

                for (const char *d = q + 1; d < semi && *d >= '0' && *d <= '9' && di < 15; ++d)
                    digits[di++] = *d;

                count = static_cast<uint32_t>(atoi(digits));
                break;
            }

            const uint32_t base = GlslTypeBytes(type_begin, size_t(type_end - type_begin));

            if (base == 0u || count == 0u)
                return false;

            out_bytes += base * count;
            ++out_fields;

            p = semi + 1;
        }

        return out_fields > 0;
    }

    bool TestGlslMirrorParity()
    {
        AnsiString tpl_src, part_src, arena_src;

        if (!ReadSource(OS_TEXT("ShaderLibrary/module/descriptor/ssbo/model_template_row.glsl"), tpl_src)
            || !ReadSource(OS_TEXT("ShaderLibrary/module/descriptor/ssbo/model_part_row.glsl"), part_src)
            || !ReadSource(OS_TEXT("ShaderLibrary/module/descriptor/ssbo/model_expansion_arena.glsl"), arena_src))
        {
            GLogError(u8"TestGPUModelTemplate: 无法读取 GLSL 镜像（需在仓库根目录运行）");
            return false;
        }

        struct
        {
            const AnsiString *src;
            const char       *struct_name;
            uint32_t          expect_bytes;
            uint32_t          expect_fields;
            const char       *why;
        } kCases[] =
        {
            { &tpl_src,   "ModelTemplateRowData",    uint32_t(sizeof(ssbo::ModelTemplateRow)), 11u,
              "ModelTemplateRowData 镜像字段总宽必须 == 128B 且字段数不变" },
            { &part_src,  "ModelPartRowData",        uint32_t(sizeof(ssbo::ModelPartRow)),     8u,
              "ModelPartRowData 镜像字段总宽必须 == 128B 且字段数不变" },
            { &arena_src, "ModelExpansionArenaData", uint32_t(sizeof(ModelExpansionArena)),    8u,
              "ModelExpansionArenaData 镜像字段总宽必须 == 48B 且字段数不变" },
        };

        bool ok = true;

        for (auto &c : kCases)
        {
            uint32_t bytes = 0, fields = 0;

            if (!SumGlslStructBytes(*c.src, c.struct_name, bytes, fields))
            {
                ok = false;
                GLogError(u8"TestGPUModelTemplate 失败: 解析不到 %s", c.struct_name);
                continue;
            }

            if (bytes != c.expect_bytes || fields != c.expect_fields)
            {
                ok = false;
                GLogError(u8"TestGPUModelTemplate 失败: %s（镜像 %uB/%u 字段，期望 %uB/%u 字段）",
                          c.why, bytes, fields, c.expect_bytes, c.expect_fields);
            }
        }

        // 生成物必须自报"由 codegen 产出"（手改后这行常被一起删掉）。
        ok = ok && (strstr(tpl_src.c_str(), "@ulre-codegen begin") != nullptr);
        ok = ok && (strstr(part_src.c_str(), "@ulre-codegen begin") != nullptr);
        ok = ok && (strstr(arena_src.c_str(), "@ulre-codegen begin") != nullptr);

        // 行 0 = 哨兵这一口径必须写进生成物说明（改注释也算改契约）。
        ok = ok && (strstr(tpl_src.c_str(), "行 0") != nullptr);

        return ok;
    }

    // ── C 层：源码契约（真机才可观察的接线） ────────────────────────────────

    bool ContainsToken(const AnsiString &src, const char *needle)
    {
        return strstr(src.c_str(), needle) != nullptr;
    }

    /// 断言 tokens 依序出现（中间允许任意空白/对齐填充）。
    /// 池配置表的列对齐会被格式化重排，逐字匹配太脆；按"记号序列"钉更稳。
    bool ContainsInOrder(const AnsiString &src, const char *const *tokens, const size_t count)
    {
        const char *p = src.c_str();

        for (size_t i = 0; i < count; ++i)
        {
            const char *hit = strstr(p, tokens[i]);

            if (!hit)
                return false;

            p = hit + strlen(tokens[i]);
        }

        return true;
    }

    bool TestSourceContracts()
    {
        AnsiString registry_src, cull_src, mgr_src, stage_head_src, interstage_writer_src, surface_src;

        if (!ReadSource(OS_TEXT("src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp"), registry_src)
            || !ReadSource(OS_TEXT("ShaderLibrary/module/descriptor/ecs/entity_gpu_cull.glsl"), cull_src)
            || !ReadSource(OS_TEXT("src/SceneGraph/asset/ModelTemplateManager.cpp"), mgr_src)
            || !ReadSource(OS_TEXT("ShaderLibrary/module/stage/stage_common_head.glsl"), stage_head_src)
            || !ReadSource(OS_TEXT("ShaderLibrary/module/stage/interstage_varying_writer.glsl"), interstage_writer_src)
            || !ReadSource(OS_TEXT("ShaderLibrary/module/provider/surface/surface_material.glsl"), surface_src))
        {
            GLogError(u8"TestGPUModelTemplate: 无法读取源文件（需在仓库根目录运行）");
            return false;
        }

        // 池：stride = 行宽（行号 × 行宽 = 字节偏移的全部前提），且保留行 0 作哨兵。
        static const char *const kTplPoolTokens[] =
            { "ModelTemplatePool", "sizeof(ssbo::ModelTemplateRow)", "4096u", "1u" };
        static const char *const kPartPoolTokens[] =
            { "ModelPartPool", "sizeof(ssbo::ModelPartRow)", "16384u", "1u" };

        // BDA：两个池地址都是**全局**（不是 per-world），展开侧才能跨世界只读。
        static const char *const kTplAddrTokens[] =
            { "addr_model_template_pool", "GetGPUBase(GlobalSSBOType::ModelTemplatePool)" };
        static const char *const kPartAddrTokens[] =
            { "addr_model_part_pool", "GetGPUBase(GlobalSSBOType::ModelPartPool)" };

        const bool tpl_pool  = ContainsInOrder(registry_src, kTplPoolTokens,  4);
        const bool part_pool = ContainsInOrder(registry_src, kPartPoolTokens, 4);
        const bool tpl_addr  = ContainsInOrder(registry_src, kTplAddrTokens,  2);
        const bool part_addr = ContainsInOrder(registry_src, kPartAddrTokens, 2);

        // 展开侧：桶号 = 组基址 + 槽号（"部件 ⇒ 素材桶"的唯一公式）。
        const bool bucket_formula = ContainsToken(cull_src,
            "const uint bucket = tag_bucket_base + part.bucket_slot");
        const bool slot_sentinel = ContainsToken(cull_src, "part.bucket_slot == 0xFFFFFFFFu");

        // 代理球更小不得导致误剔除（保守取大）。
        const bool conservative_cull = ContainsToken(cull_src, "max(cull_radius, bound_radius)");

        // LOD：槽序细→粗，从最粗槽往回取第一个满足阈值的。
        const bool lod_coarsest_first = ContainsToken(cull_src,
            "for (uint i = uint(MODEL_LOD_PROXY_COUNT); i > 0u; --i)");

        // 代理回填：本体与全部代理共享一套 bucket_slot 编号（否则代理画错几何）。
        const bool proxy_union = ContainsToken(mgr_src, "union_geoms");
        const bool proxy_renumber = ContainsToken(mgr_src, "updated.part_bucket_count = union_count");

        // T6.3/T6.4：剔除展开写出自定义颜色，网格与材质着色器消费
        const bool cull_custom_out = ContainsToken(cull_src, "instance_custom_data_out[inst_row].custom_color = custom_color;");
        const bool cull_custom_pool = ContainsToken(cull_src, "instance_custom_data_pool[entity.custom_data_index].custom_color");
        const bool stage_head_resolve = ContainsToken(stage_head_src, "ResolveInstanceCustomColor(uint iid)");
        const bool stage_head_bda = ContainsToken(stage_head_src, "InstanceCustomDataBufferRef(addr_custom).rows[iid].custom_color");
        const bool mesh_varying_color = ContainsToken(interstage_writer_src, "fragVertexColor[vid] = GetVertexColor() * ResolveInstanceCustomColor(gl_InstanceIndex);");
        const bool surface_color = ContainsToken(surface_src, "surface.baseColor = material.baseColor * si.vertexColor.rgb;");
        const bool surface_alpha = ContainsToken(surface_src, "surface.alpha = material.alpha * si.vertexColor.a;");

        const struct
        {
            bool        ok;
            const char *why;
        } kChecks[] =
        {
            { tpl_pool,  "ModelTemplatePool 池配置必须 stride=sizeof(ModelTemplateRow)、4096 行、保留行 0" },
            { part_pool, "ModelPartPool 池配置必须 stride=sizeof(ModelPartRow)、16384 行、保留行 0" },
            { tpl_addr,  "模板池 BDA 地址必须在全局地址表回填" },
            { part_addr, "部件池 BDA 地址必须在全局地址表回填" },
            { bucket_formula, "展开侧桶号必须 = 组基址 + part.bucket_slot" },
            { slot_sentinel,  "无槽部件必须按哨兵跳过（不得落到组首桶）" },
            { conservative_cull, "粗略剔除必须取 max(cull_radius, 生效模板包围球)" },
            { lod_coarsest_first, "LOD 选择必须从最粗槽往回扫描" },
            { proxy_union,    "SetModelTemplateProxies 必须做代理并集重编号" },
            { proxy_renumber, "并集重编号必须把每个参与模板的 part_bucket_count 改成并集大小" },
            { cull_custom_out, "剔除计算必须将 custom_color 写出到 instance_custom_data_out" },
            { cull_custom_pool, "剔除计算必须从 instance_custom_data_pool 采样实体自定义颜色" },
            { stage_head_resolve, "stage_common_head 必须提供 ResolveInstanceCustomColor 接口" },
            { stage_head_bda, "ResolveInstanceCustomColor 必须通过 BDA 寻址取 custom_color" },
            { mesh_varying_color, "网格阶段写入顶点色时必须调制 ResolveInstanceCustomColor" },
            { surface_color, "材质表面求值必须将 baseColor 与 si.vertexColor.rgb 调制" },
            { surface_alpha, "材质表面求值必须将 alpha 与 si.vertexColor.a 调制" },
        };

        bool ok = true;

        for (const auto &c : kChecks)
        {
            if (c.ok)
                continue;

            ok = false;
            GLogError(u8"TestGPUModelTemplate 失败: %s", c.why);
        }

        return ok;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestGPUModelTemplate"));

    Check(TestRowLayout(), "行布局（宽/对齐/字段偏移）");
    Check(TestDefaultsAndConstants(), "默认值与常量口径");
    Check(TestCustomColorModulation(), "自定义颜色与透明度调制");
    Check(TestGlslMirrorParity(), "GLSL 镜像逐字段对拍");
    Check(TestSourceContracts(), "池/展开侧/着色器源码契约");

    if (g_failed > 0)
    {
        GLogError(u8"=== TestGPUModelTemplate: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestGPUModelTemplate: 全部测试通过 ===");
    return 0;
}
