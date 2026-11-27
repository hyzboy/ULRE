/**
 * TestMultiMaterialModelTemplate —— Stage 1 多材质模型模板（Step 1.4）单元测试与契约验证
 *
 * 测试内容：
 * 1. ModelTemplateMaterialTable 生命周期与 MaterialRuntimeRow 引用计数维护；
 * 2. ModelTemplateRenderItem 载体项属性与管线覆盖；
 * 3. 多 Shading Group 模板的桶组与二级组表回填机制；
 * 4. 纯模板实体（无 GeometryData / MaterialData）分流契约。
 */

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/render/ModelTemplateRenderItem.h>
#include <hgl/ecs/support/material/ModelTemplateMaterialTable.h>
#include <hgl/ecs/support/material/MaterialRuntimeTable.h>
#include <hgl/ecs/support/material/MaterialRuntimePipelineTable.h>
#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/type/PtrArray.h>
#include <hgl/type/ValueArray.h>

namespace
{
    using namespace hgl;
    using namespace hgl::ecs;

    int g_failed = 0;

    void Check(const bool ok, const char *what)
    {
        if (!ok)
        {
            ++g_failed;
            GLogError(u8"TestMultiMaterialModelTemplate 失败: %s", what);
        }
    }

    bool ReadSource(const OSString &path, AnsiString &out_text)
    {
        io::OpenFileInputStream fis(path);
        if (!fis || fis->GetSize() <= 0)
            return false;

        char chunk[4096];
        int64 got;
        while ((got = fis->Read(chunk, int64(sizeof(chunk)))) > 0)
            out_text.Strcat(chunk, int(got));

        return true;
    }

    /// 收集系统实现已按职责拆到多个 TU（材质计划 / 图元模板规范化 / 主帧缓存重放）。
    /// 契约扫描必须覆盖**整组**源文件，否则拆分本身会让门禁假红。
    bool ReadCollectSources(AnsiString &out_text)
    {
        static const OSString kCollectSources[] =
        {
            OS_TEXT("src/ecs/systems/render/collect/RenderCollectSystem.cpp"),
            OS_TEXT("src/ecs/systems/render/collect/RenderCollect_PrimitiveNormalize.cpp"),
            OS_TEXT("src/ecs/systems/render/collect/RenderCollect_CacheReplay.cpp"),
            OS_TEXT("src/ecs/systems/render/collect/RenderCollectInternal.cpp"),
        };

        out_text = "";

        bool any = false;
        for (const OSString &path : kCollectSources)
        {
            AnsiString part;
            if (!ReadSource(path, part))
                continue;

            out_text += part;
            out_text += "\n";
            any = true;
        }

        return any;
    }

    bool ContainsToken(const AnsiString &src, const char *needle)
    {
        return strstr(src.c_str(), needle) != nullptr;
    }

    // ── 1. ModelTemplateMaterialTable 基础增删查与引用计数测试 ─────────

    bool TestMaterialTableLifecycle()
    {
        ECSContext context("TestContext");
        MaterialRuntimeTable *runtime_table = context.GetMaterialRuntimeTable();
        ModelTemplateMaterialTable *mat_table = context.GetModelTemplateMaterialTable();

        if (!runtime_table || !mat_table)
            return false;

        // 分配 2 个虚拟材质运行期行
        MaterialRuntimeKey key_a;
        key_a.program_build_context = 1;
        key_a.recipe_hash = 1001;
        key_a.data_index_row = 1;

        MaterialRuntimeKey key_b;
        key_b.program_build_context = 1;
        key_b.recipe_hash = 1002;
        key_b.data_index_row = 2;

        const MaterialRuntimeRowID row_a = runtime_table->Intern(key_a);
        const MaterialRuntimeRowID row_b = runtime_table->Intern(key_b);

        if (row_a == INVALID_MATERIAL_RUNTIME_ROW_ID || row_b == INVALID_MATERIAL_RUNTIME_ROW_ID)
            return false;

        const uint32_t init_ref_a = runtime_table->GetRefCount(row_a);
        const uint32_t init_ref_b = runtime_table->GetRefCount(row_b);

        // 构造一个 2 组模板绑定
        const uint32_t test_template_id = 42u;
        TemplateRenderBinding binding;
        binding.template_id = test_template_id;
        binding.owned_part_rows.Add(row_a);
        binding.owned_part_rows.Add(row_b);

        TemplateShadingGroupDesc group0;
        group0.runtime_row = row_a;
        group0.slot_geometry.Add(1001u);
        group0.slot_count.Add(2u);
        group0.casts_shadow = true;
        binding.groups.Add(group0);

        TemplateShadingGroupDesc group1;
        group1.runtime_row = row_b;
        group1.slot_geometry.Add(1002u);
        group1.slot_count.Add(3u);
        group1.casts_shadow = false;
        binding.groups.Add(group1);

        // 注册到表
        if (!mat_table->RegisterBinding(binding))
            return false;

        // 验证引用计数增加
        if (runtime_table->GetRefCount(row_a) != init_ref_a + 1
         || runtime_table->GetRefCount(row_b) != init_ref_b + 1)
            return false;

        // 查找并校验各字段
        const TemplateRenderBinding *found = mat_table->FindBinding(test_template_id);
        if (!found || found->groups.GetCount() != 2)
            return false;

        if (found->groups[0].runtime_row != row_a
         || found->groups[0].slot_count[0] != 2u
         || !found->groups[0].casts_shadow)
            return false;

        if (found->groups[1].runtime_row != row_b
         || found->groups[1].slot_count[0] != 3u
         || found->groups[1].casts_shadow)
            return false;

        // 注销绑定
        if (!mat_table->UnregisterBinding(test_template_id))
            return false;

        // 验证引用计数复原
        if (runtime_table->GetRefCount(row_a) != init_ref_a
         || runtime_table->GetRefCount(row_b) != init_ref_b)
            return false;

        // 清理行
        runtime_table->Release(row_a);
        runtime_table->Release(row_b);

        return true;
    }

    // ── 1b. 单图元模板缓存与注销级联清理测试 ────────────────────────

    bool TestSinglePrimitiveModelTemplateCache()
    {
        ECSContext context("TestSingleTmplContext");
        ModelTemplateMaterialTable *mat_table = context.GetModelTemplateMaterialTable();
        if (!mat_table)
            return false;

        const uint32_t kGeoID = 42u;
        const MaterialRuntimeRowID kRow = context.GetMaterialRuntimeTable()->Intern(MaterialRuntimeKey{1u, 7u, 0u});
        const uint32_t kTemplateID = 101u;

        SinglePrimitiveTemplateKey key;
        key.geometry_id = kGeoID;
        key.can_cast_shadow = true;
        key.is_opaque = true;

        // 初始无缓存
        if (mat_table->FindSinglePrimitiveTemplate(key) != 0u)
            return false;

        // 注册绑定并关联单图元模板
        TemplateRenderBinding binding;
        binding.template_id = kTemplateID;
        binding.owned_part_rows.Add(kRow);
        TemplateShadingGroupDesc grp;
        grp.runtime_row = kRow;
        grp.casts_shadow = true;
        grp.slot_geometry.Add(kGeoID);
        grp.slot_count.Add(1);
        binding.groups.Add(grp);

        if (!mat_table->RegisterBinding(binding))
            return false;

        mat_table->RegisterSinglePrimitiveTemplate(key, kTemplateID);

        // 查缓存必须命中：同拓扑实例共享模板，材质运行期行不进入模板身份。
        if (mat_table->FindSinglePrimitiveTemplate(key) != kTemplateID)
            return false;

        auto line_key = key;
        line_key.primitive_type = graph::PrimitiveType::Lines;
        if (mat_table->FindSinglePrimitiveTemplate(line_key) != 0u)
            return false;
        binding.template_id = kTemplateID + 1u;
        binding.groups[0].primitive_type = graph::PrimitiveType::Lines;
        if (!mat_table->RegisterBinding(binding))
            return false;
        mat_table->RegisterSinglePrimitiveTemplate(line_key, binding.template_id);
        if (mat_table->FindSinglePrimitiveTemplate(key) != kTemplateID
         || mat_table->FindSinglePrimitiveTemplate(line_key) != binding.template_id
         || mat_table->FindBinding(kTemplateID)->groups[0].primitive_type != graph::PrimitiveType::Triangles
         || mat_table->FindBinding(binding.template_id)->groups[0].primitive_type != graph::PrimitiveType::Lines)
            return false;

        // 注销绑定必须自动清除单图元缓存
        if (!mat_table->UnregisterBinding(kTemplateID))
            return false;

        if (mat_table->FindSinglePrimitiveTemplate(key) != 0u)
            return false;
        if (mat_table->FindSinglePrimitiveTemplate(line_key) != binding.template_id
         || !mat_table->UnregisterBinding(binding.template_id))
            return false;

        return true;
    }

    bool TestBindingReplacement()
    {
        ECSContext context("BindingReplacement");
        auto *runtime = context.GetMaterialRuntimeTable();
        auto *table = context.GetModelTemplateMaterialTable();
        const auto row = runtime->Intern(MaterialRuntimeKey{1u, 91u, 0u});
        TemplateRenderBinding binding;
        binding.template_id = 901u;
        binding.owns_gpu_rows = true;
        binding.owned_part_rows.Add(row);
        TemplateShadingGroupDesc group;
        group.runtime_row = row;
        group.slot_geometry.Add(1u);
        group.slot_count.Add(1u);
        binding.groups.Add(group);
        if (!table->RegisterBinding(binding))
            return false;
        runtime->Release(row); // The binding is now the sole owner.
        binding.owns_gpu_rows = false;
        if (!table->RegisterBinding(binding) || !runtime->Get(row)
         || runtime->GetRefCount(row) != 1u || !table->FindBinding(901u)->owns_gpu_rows)
            return false;
        const auto *self = table->FindBinding(binding.template_id);
        if (!self || !table->RegisterBinding(*self) || runtime->GetRefCount(row) != 1u)
            return false;

        binding.owned_part_rows.Add(INVALID_MATERIAL_RUNTIME_ROW_ID);
        if (table->RegisterBinding(binding) || runtime->GetRefCount(row) != 1u)
            return false;
        binding.owned_part_rows.Delete(binding.owned_part_rows.GetCount() - 1);
        binding.groups[0].slot_count.Clear();
        if (table->RegisterBinding(binding) || runtime->GetRefCount(row) != 1u)
            return false;
        if (!table->UnregisterBinding(901u) || runtime->Get(row))
            return false;

        const auto a = runtime->Intern(MaterialRuntimeKey{1u, 92u, 0u});
        const auto b = runtime->Intern(MaterialRuntimeKey{1u, 93u, 0u});
        const auto c = runtime->Intern(MaterialRuntimeKey{1u, 94u, 0u});
        binding.owned_part_rows.Clear();
        binding.groups.Clear();
        binding.owned_part_rows.Add(a);
        binding.owned_part_rows.Add(b);
        group.runtime_row = a;
        binding.groups.Add(group);
        group.runtime_row = b;
        binding.groups.Add(group);
        if (!table->RegisterBinding(binding))
            return false;
        runtime->Release(a);
        runtime->Release(b);
        binding.owned_part_rows[0] = c;
        binding.groups[0].runtime_row = c;
        if (!table->RegisterBinding(binding) || runtime->Get(a)
         || runtime->GetRefCount(b) != 1u || runtime->GetRefCount(c) != 2u)
            return false;
        runtime->Release(c);
        binding.owned_part_rows.Add(c);
        if (table->RegisterBinding(binding) || runtime->GetRefCount(c) != 1u)
            return false;
        return table->UnregisterBinding(901u) && !runtime->Get(b) && !runtime->Get(c);
    }

    // ── 2. ModelTemplateRenderItem 载体项测试 ──────────────────────────

    bool TestTemplateRenderItemCarrier()
    {
        ECSContext context("TestCarrierContext");
        Entity *entity = context.CreateEntity();
        if (!entity)
            return false;

        const uint32_t kTemplateID = 88u;
        const uint32_t kShadingGroup = 1u;

        TransformAccessor trans;
        graph::mtl::MaterialPipelineConfig config{};

        ModelTemplateRenderItem item(
            entity->GetEntityID(),
            trans,
            INVALID_MATERIAL_RUNTIME_ROW_ID,
            kShadingGroup,
            kTemplateID,
            nullptr,
            nullptr,
            config,
            &context);

        if (item.GetEntity() != entity)
            return false;

        if (item.GetShadingGroup() != kShadingGroup)
            return false;

        if (item.GetTemplateID() != kTemplateID)
            return false;

        return true;
    }

    bool TestGroupPipelineCache()
    {
        ECSContext context("TestGroupPipelineContext");
        Entity *entity = context.CreateEntity();
        Entity *other = context.CreateEntity();
        if (!entity || !other)
            return false;

        auto &table = context.GetMaterialRuntimePipelineTable();
        const EntityID id = entity->GetEntityID();
        const EntityID other_id = other->GetEntityID();

        // Opaque identity tokens only; no Vulkan objects are created or dereferenced.
        int tokens[7]{};
        auto *pass_a = reinterpret_cast<graph::RenderPass *>(&tokens[0]);
        auto *pass_b = reinterpret_cast<graph::RenderPass *>(&tokens[1]);
        auto *pipeline_a = reinterpret_cast<graph::Pipeline *>(&tokens[2]);
        auto *pipeline_b = reinterpret_cast<graph::Pipeline *>(&tokens[3]);
        auto *pipeline_c = reinterpret_cast<graph::Pipeline *>(&tokens[4]);
        auto *program_a = reinterpret_cast<graph::ShaderProgram *>(&tokens[5]);
        auto *program_b = reinterpret_cast<graph::ShaderProgram *>(&tokens[6]);
        graph::mtl::MaterialPipelineConfig config_a{};
        graph::mtl::MaterialPipelineConfig config_b{};
        config_b.cull_mode = VK_CULL_MODE_NONE;
        config_b.alpha_blend = true;

        table.Set(id, pass_a, pipeline_a, config_a);
        table.Set(id, pass_a, pipeline_b, config_b, 1, program_a);
        table.Set(other_id, pass_a, pipeline_c, config_a, 1);
        if (table.Get(id, pass_a, 0) != pipeline_a
            || table.Get(id, pass_a, 1) != pipeline_b
            || table.GetConfig(id, pass_a, 0) != config_a
            || table.GetConfig(id, pass_a, 1) != config_b
            || table.Get(id, pass_a, 2) != nullptr
            || table.Get(id, pass_b, 1) != nullptr)
            return false;

        TransformAccessor transform;
        ModelTemplateRenderItem item(id, transform, INVALID_MATERIAL_RUNTIME_ROW_ID,
                                     1, 42, pass_a, nullptr, config_a, &context);
        if (item.GetPipeline(pass_a) != pipeline_b
            || item.GetPipelineConfig(pass_a) != config_b)
            return false;

        table.Set(id, pass_b, pipeline_c, config_a, 1, program_b);
        if (table.Get(id, pass_a, 1) != pipeline_b
            || item.GetPipeline(pass_a) != pipeline_b
            || item.GetPipeline(pass_b) != pipeline_c
            || item.GetPipelineConfig(pass_b) != config_a
            || table.GetProgram(id, pass_a, 1) != program_a
            || item.GetShaderProgram() != program_a
            || table.Get(id, pass_a, 0) != pipeline_a)
            return false;

        table.Clear(id);
        return table.Get(id, pass_a, 0) == nullptr
            && table.Get(id, pass_b, 1) == nullptr
            && table.Get(other_id, pass_a, 1) == pipeline_c;
    }

    // ── 2b. 模型引用绑定与 GPU 事实测试（无材质覆盖污染）──────────────

    bool TestGPUModelRefBindingAndFacts()
    {
        ECSContext context("TestModelFactsContext");
        Entity *entity = context.CreateEntity();
        if (!entity)
            return false;

        const uint32_t kTemplateID = 77u;
        auto *ref = entity->AddComponent<GPUModelRefComponent>();
        if (!ref)
            return false;

        const auto topo_revision = context.GetPrimitiveTopologyLedger().GetTopologyRevision(TopologyChange::ModelTemplate);
        ref->SetTemplateBinding(kTemplateID);
        const auto changed_revision = context.GetPrimitiveTopologyLedger().GetTopologyRevision(TopologyChange::ModelTemplate);
        if (changed_revision <= topo_revision || ref->GetTemplateID() != kTemplateID)
            return false;

        // 掩码操作测试
        ref->HidePart(2u);
        if (ref->IsPartVisible(2u) || !ref->IsPartVisible(0u) || !ref->IsPartVisible(1u))
            return false;
        ref->ShowPart(2u);
        if (!ref->IsPartVisible(2u))
            return false;
        ref->HidePart(5u);
        ref->ShowAllParts();
        if (!ref->IsPartVisible(5u))
            return false;

        // GPU 派生行事实写入测试（纯净化，无 material_override 标志位）
        auto &gpu = context.GetEntityGPUTable();
        gpu.SetModelFacts(entity->GetEntityID(), kTemplateID, ref->part_visibility_mask, 12u, 34u);
        const auto *gpu_row = gpu.Row(entity->GetEntityID());
        if (!gpu_row)
            return false;

        if (gpu_row->model_template_id != kTemplateID
         || gpu_row->model_part_mask != UINT64_MAX
         || gpu_row->custom_data_index != 12u
         || gpu_row->prev_transform_id != 34u
         || gpu_row->work_flags != 0u)
            return false;

        context.DestroyEntity(entity->GetEntityID());
        return true;
    }

    // ── 2d. 实体专属材质数据行直通 4-ID 事实表测试 ────────────────────

    bool TestEntityDedicatedMaterialDataRowFact()
    {
        ECSContext context("TestDedicatedMatDataContext");
        Entity *entity = context.CreateEntity();
        if (!entity)
            return false;

        MaterialData *mat_data = context.GetOrCreateMaterialData(entity);
        if (!mat_data)
            return false;

        // 设置专属材质数据行 (data_index = 42u, 属于 PBRSurface 池)
        MaterialData::MaterialDataAuthoringResource res{};
        res.ssbo_type = graph::GlobalSSBOType::PBRSurface;
        res.ssbo_id = 1u;
        res.data_index = 42u;

        if (!mat_data->SetDataResource(res))
            return false;

        if (!mat_data->HasDataResource() || mat_data->data_resource.data_index != 42u)
            return false;

        // 验证写入实体 4-ID 事实表
        const uint32_t pass_index = 0u;
        graph::RenderItemDescriptor ids{};
        ids.transform_id = 10u;
        ids.geometry_id = 0u;
        ids.material_id = mat_data->data_resource.data_index;
        ids.texture_id = 5u;

        context.GetEntityRenderIDTable().SetRenderIDs(pass_index, entity->GetEntityID(), ids);

        const auto *read_ids = context.GetEntityRenderIDTable().Row(pass_index, entity->GetEntityID().index);
        if (!read_ids || read_ids->material_id != 42u || read_ids->texture_id != 5u)
            return false;

        context.DestroyEntity(entity->GetEntityID());
        return true;
    }

    // ── 3. 源码契约测试（验证 Step 1.4.4 与 1.4.5 的核心修改）──────────

    bool TestSourceContracts()
    {
        AnsiString collect_src, batch_src, template_src, builder_src, replay_src, shader_src;

        if (!ReadCollectSources(collect_src)
         || !ReadSource(OS_TEXT("src/ecs/support/pipeline/RenderBatchPipeline.cpp"), batch_src)
         || !ReadSource(OS_TEXT("inc/hgl/ecs/support/material/ModelTemplateMaterialTable.h"), template_src)
         || !ReadSource(OS_TEXT("src/ecs/support/material/ModelTemplateBuilder.cpp"), builder_src)
         || !ReadSource(OS_TEXT("src/ecs/systems/render/collect/RenderCollect_CacheReplay.cpp"), replay_src)
         || !ReadSource(OS_TEXT("ShaderLibrary/module/descriptor/ecs/entity_gpu_cull.glsl"), shader_src))
        {
            GLogError(u8"TestMultiMaterialModelTemplate: 无法读取源文件");
            return false;
        }

        const struct
        {
            const AnsiString &src;
            const char       *needle;
            const char       *why;
        } kChecks[] =
        {
            // Collect 系统快线契约
            { collect_src,
              "is_template_carrier",
              "PublishPrimitiveFacts 必须支持纯模板载体判定" },
            { collect_src,
              "EnsureTemplateGroupProgram",
              "收集系统必须提供 EnsureTemplateGroupProgram 确保组着色器程序就绪" },
            { collect_src,
              "GetModelTemplateMaterialTable",
              "收集系统必须查询 ModelTemplateMaterialTable 获取各 Shading Group 描述" },
            { collect_src,
              "ModelTemplateRenderItem",
              "纯模板实体必须按组创建 ModelTemplateRenderItem 载体项投入批次" },
            { collect_src,
              "grp_desc.runtime_row",
              "收集系统必须直接消费各组独立 runtime_row" },
            { collect_src,
              "BuildSinglePrimitiveModelTemplate",
              "收集系统前置保障必须调用 BuildSinglePrimitiveModelTemplate 自动提升为模板实体" },
            { collect_src,
              "ref->SetTemplateBinding(auto_template_id, true)",
              "自动提升必须将分配的模板 ID 挂接到实体的 GPUModelRefComponent" },
            { collect_src,
              "EnsurePrimitiveModelTemplate",
              "收集预扫描必须调用 EnsurePrimitiveModelTemplate 前置确保模型模板" },
            { collect_src,
              "NoModelTemplate",
              "主收集循环必须彻底清理旧单图元物化分支，仅保留纯模板快线与 NoModelTemplate 兜底" },
            { collect_src,
              "mat_data->has_data_resource",
              "4-ID 事实表必须优先消费实体专属材质数据行 (data_resource.data_index)" },

            // 批处理流水线多组支持契约
            { batch_src,
              "pass_template_group_offsets",
              "批处理必须在 pass 级别统一分配各模板的二级组表基址" },
            { batch_src,
              "gpu_cull->SetModelGroupBucketBase(pass_index, grp.group_table_offset, grp.shading_group,",
              "各组的首桶号必须回填到各自对应的 shading_group 槽位" },
            { batch_src,
              "gpu_cull->SetModelEntityGroupOffset(pass_index, owner->GetEntityID().index,",
              "必须为模板实体写入二级组表偏移" },
        };

        bool ok = true;
        for (const auto &c : kChecks)
        {
            if (ContainsToken(c.src, c.needle))
                continue;

            ok = false;
            GLogError(u8"TestMultiMaterialModelTemplate 源码契约缺失: %s", c.why);
        }

        // Simplification contracts: data rows are per-instance facts, while template identity
        // is shared across compatible geometries and rendering states.
        if (ContainsToken(builder_src, "cache_key.runtime_row"))
        {
            ok = false;
            GLogError(u8"TestMultiMaterialModelTemplate 简化契约违反: runtime_row 仍参与单图元模板键");
        }
        if (!ContainsToken(template_src, "h << k.geometry_id << (k.can_cast_shadow ? 1u : 0u)"))
        {
            ok = false;
            GLogError(u8"TestMultiMaterialModelTemplate 简化契约缺失: 单图元模板共享哈希");
        }
        if (!ContainsToken(builder_src, "binding.owns_gpu_rows = false"))
        {
            ok = false;
            GLogError(u8"TestMultiMaterialModelTemplate 简化契约缺失: 单图元模板不占有 GPU 材质行");
        }
        if (ContainsToken(shader_src, "has_part_mtl")
         || ContainsToken(shader_src, "part.material_id")
         || ContainsToken(shader_src, "part.texture_id"))
        {
            ok = false;
            GLogError(u8"TestMultiMaterialModelTemplate 简化契约违反: GPU 剔除仍回退部件行");
        }
        if (!ContainsToken(shader_src, "const uint material_id = group_route.w != 0u ? group_route.y : ids_blk.material_id")
         || !ContainsToken(shader_src, "const uint texture_id  = group_route.w != 0u ? group_route.z : ids_blk.texture_id"))
        {
            ok = false;
            GLogError(u8"TestMultiMaterialModelTemplate 简化契约缺失: GPU 材质行直通");
        }
        if (!ContainsToken(replay_src, "mat_data && mat_data->has_data_resource")
         || !ContainsToken(replay_src, "mat_data->data_resource.data_index"))
        {
            ok = false;
            GLogError(u8"TestMultiMaterialModelTemplate 简化契约缺失: 缓存重放未复用实体专属材质行");
        }

        return ok;
    }
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestMultiMaterialModelTemplate"));

    Check(TestMaterialTableLifecycle(), "1. ModelTemplateMaterialTable 生命周期与引用计数");
    Check(TestSinglePrimitiveModelTemplateCache(), "1b. 单图元模板缓存与注销级联清理");
    Check(TestBindingReplacement(), "Binding replacement preserves overlapping rows and rejects invalid input atomically");
    Check(TestTemplateRenderItemCarrier(), "2. ModelTemplateRenderItem 载体项属性");
    Check(TestGroupPipelineCache(), "2c. Group pipeline isolation, pass mismatch and entity invalidation");
    Check(TestGPUModelRefBindingAndFacts(), "2b. 模型引用绑定与 GPU 事实纯净化");
    Check(TestEntityDedicatedMaterialDataRowFact(), "2d. 实体专属材质数据行直通 4-ID 事实表");
    Check(TestSourceContracts(), "3. 收集快线与批处理多组源码契约");

    if (g_failed > 0)
    {
        GLogError(u8"=== TestMultiMaterialModelTemplate: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestMultiMaterialModelTemplate: 全部测试通过 ===");
    return 0;
}
