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
        const MaterialRuntimeRowID kRow = 7u;
        const uint32_t kTemplateID = 101u;

        SinglePrimitiveTemplateKey key;
        key.geometry_id = kGeoID;
        key.runtime_row = kRow;
        key.can_cast_shadow = true;
        key.is_opaque = true;

        // 初始无缓存
        if (mat_table->FindSinglePrimitiveTemplate(key) != 0u)
            return false;

        // 注册绑定并关联单图元模板
        TemplateRenderBinding binding;
        binding.template_id = kTemplateID;
        TemplateShadingGroupDesc grp;
        grp.runtime_row = kRow;
        grp.casts_shadow = true;
        grp.slot_geometry.Add(kGeoID);
        grp.slot_count.Add(1);
        binding.groups.Add(grp);

        if (!mat_table->RegisterBinding(binding))
            return false;

        mat_table->RegisterSinglePrimitiveTemplate(key, kTemplateID);

        // 查缓存必须命中
        if (mat_table->FindSinglePrimitiveTemplate(key) != kTemplateID)
            return false;

        // 注销绑定必须自动清除单图元缓存
        if (!mat_table->UnregisterBinding(kTemplateID))
            return false;

        if (mat_table->FindSinglePrimitiveTemplate(key) != 0u)
            return false;

        return true;
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

    // ── 2b. 实例级材质覆盖测试 ──────────────────────────────────────

    bool TestInstanceMaterialOverride()
    {
        ECSContext context("TestOverrideContext");
        Entity *entity_default = context.CreateEntity();
        Entity *entity_override = context.CreateEntity();
        if (!entity_default || !entity_override)
            return false;

        const uint32_t kTemplateID = 77u;
        const MaterialRuntimeRowID kOverrideRow = 55u;

        auto *ref_def = entity_default->AddComponent<GPUModelRefComponent>();
        auto *ref_ovr = entity_override->AddComponent<GPUModelRefComponent>();
        if (!ref_def || !ref_ovr)
            return false;

        ref_def->template_id = kTemplateID;
        ref_ovr->template_id = kTemplateID;

        // 默认无覆盖
        if (ref_def->HasMaterialOverride())
            return false;
        if (ref_def->override_material_row != INVALID_MATERIAL_RUNTIME_ROW_ID)
            return false;

        // 实体 B 覆盖材质
        ref_ovr->SetOverrideMaterialRow(kOverrideRow);
        if (!ref_ovr->HasMaterialOverride())
            return false;
        if (ref_ovr->override_material_row != kOverrideRow)
            return false;

        ref_ovr->ClearOverrideMaterialRow();
        if (ref_ovr->HasMaterialOverride())
            return false;

        return true;
    }

    // ── 3. 源码契约测试（验证 Step 1.4.4 与 1.4.5 的核心修改）──────────

    bool TestSourceContracts()
    {
        AnsiString collect_src, batch_src;

        if (!ReadSource(OS_TEXT("src/ecs/systems/render/collect/RenderPrimitiveCollectSystem.cpp"), collect_src)
         || !ReadSource(OS_TEXT("src/ecs/support/pipeline/PrimitiveBatchPipeline.cpp"), batch_src))
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
              "override_material_row",
              "收集系统必须支持 pure_model_ref->override_material_row 实例材质覆盖" },
            { collect_src,
              "BuildSinglePrimitiveModelTemplate",
              "收集系统前置保障必须调用 BuildSinglePrimitiveModelTemplate 自动提升为模板实体" },
            { collect_src,
              "ref->template_id = auto_template_id",
              "自动提升必须将分配的模板 ID 挂接到实体的 GPUModelRefComponent" },
            { collect_src,
              "EnsurePrimitiveModelTemplate",
              "收集预扫描必须调用 EnsurePrimitiveModelTemplate 前置确保模型模板" },
            { collect_src,
              "NoModelTemplate",
              "主收集循环必须彻底清理旧单图元物化分支，仅保留纯模板快线与 NoModelTemplate 兜底" },

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

        return ok;
    }
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestMultiMaterialModelTemplate"));

    Check(TestMaterialTableLifecycle(), "1. ModelTemplateMaterialTable 生命周期与引用计数");
    Check(TestSinglePrimitiveModelTemplateCache(), "1b. 单图元模板缓存与注销级联清理");
    Check(TestTemplateRenderItemCarrier(), "2. ModelTemplateRenderItem 载体项属性");
    Check(TestInstanceMaterialOverride(), "2b. 实例级材质覆盖属性与重置");
    Check(TestSourceContracts(), "3. 收集快线与批处理多组源码契约");

    if (g_failed > 0)
    {
        GLogError(u8"=== TestMultiMaterialModelTemplate: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestMultiMaterialModelTemplate: 全部测试通过 ===");
    return 0;
}
