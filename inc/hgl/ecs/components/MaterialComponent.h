#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/graph/module/MaterialTextureReferencePool.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/ecs/support/MaterialVariantTable.h>

namespace hgl::graph
{
    class ShaderProgram;
}

namespace hgl::ecs
{
    class MaterialComponent : public Component
    {
    public:

        // ── A3：前向/阴影程序槽 ⇒ 变体 ID ──
        // program 本体不再挂在组件上（同键实体各持一份指针无法共享/收敛）；组件只持
        // **变体 ID**，program 与 recipe 去 `support/MaterialVariantTable.h` 的表里按
        // **静态键**取（表由所属世界持有，见 `ECSContext::GetMaterialVariantTable()`）。
        // 前向槽 = ForwardColor purpose 的变体：
        MaterialVariantID forward_variant = INVALID_MATERIAL_VARIANT_ID;

        // Runtime row indices, materialized independently for this primitive.
        // They must not be sourced from a shared recipe/spec cache entry.
        uint32_t data_index_row = uint32_t(-1);  // Shared material SSBO row ID.

        // GlobalSSBOType 独立共享缓冲中的实例数据行 CPU/GPU 地址。
        void    *material_row_cpu = nullptr;
        uint64_t material_row_gpu = 0;

        // MaterialTextureReferencePool 的按 definition 配置行。
        graph::MaterialTextureConfigurationAllocation
                material_texture_configuration;
        void    *material_texture_row_cpu = nullptr;
        uint64_t material_texture_row_gpu = 0;
        uint64_t material_texture_zero_row_gpu = 0;
        uint64_t material_texture_configuration_hash = 0;

        // Dirty/lifecycle flags.
        // program_dirty — program (pipeline) must be re-resolved.
        // runtime_dirty  — bindings/resources must be re-prepared and
        //                  materialized for the current generation.
        bool program_dirty = true;
        bool runtime_dirty = true;
        bool valid = false;
        uint64_t recipe_hash = 0;

        // ── ShadowCaster 变体槽（阴影 pass 专用，与 forward 槽完全独立）──
        // 同一物体每帧先在阴影 pass 采深度、再在主帧做着色，两个 pass 的程序
        // purpose 不同（ForwardColor vs ShadowDepth）⇒ 静态键不同，表里天然是两条
        // 记录，不会乒乓驱逐。若共用一个槽，purpose 每帧 Forward↔Shadow 乒乓会让
        // InvalidateRecipeRuntime 反复 retire 纹理配置、MaterializeRecipeRows 的无行
        // 早退把 valid 打成 false，从而禁用 P1-1 全干净帧快路径（每帧每物体两次完整
        // 物化链）。
        // ShadowCaster 模板无 material/sky descriptors，不需要物化行与纹理配置，
        // 只持 program 与 CreatePipeline 消费的 normalized recipe。
        MaterialVariantID shadow_variant = INVALID_MATERIAL_VARIANT_ID;
        uint32_t shadow_tracked_material_data_generation = 0;
        graph::mtl::MaterialRecipe shadow_cached_normalized_recipe{};

        // Cached normalized recipe — avoids redundant NormalizeRecipe in CreatePipeline.
        // Its validity is tracked by recipe_hash (same value that produced it).
        graph::mtl::MaterialRecipe cached_normalized_recipe{};

        // Cached effective recipe built with the resolved program.
        // Direct resource preparation and BDA materialization consume it.
        graph::mtl::MaterialRecipe cached_effective_recipe{};
        uint64_t cached_effective_recipe_hash = 0;

        // P3: Tracks the last observed MaterialData::GetAuthoredGeneration()（材质数据层）。
        // When this matches the data layer's current generation, all cached material
        // data is valid and ResolveMaterialProgramForPrimitive can skip entirely.
        // 注：本字段与 shadow_tracked_material_data_generation 都是**每实体**跟踪副本，
        // A3 保留原样；A4 随两个程序槽一起收敛（届时并入变体记录/数据层）。
        uint32_t tracked_material_data_generation = 0;

        // Epoch of the last materialization pass in which this primitive's
        // rows were written. A mismatch with the system's current epoch means
        // this primitive was skipped (e.g. invisible), so its rows must be
        // re-materialized before rendering.
        uint64_t last_materialize_epoch = 0;

        // ── D9：阴影 pass 跳过路径的重试收敛状态（A3 已随阴影槽迁入变体记录）──
        // 计数语义 = **该阴影变体记录**的 retry_frames（见
        // `MaterialVariantRecord::retry_frames`）：首次跳过告警一次；连续超过
        // kShadowRetryFullBumpFrames 后把静态级联失效从"每帧"降频为周期性；该 caster
        // 成功产出本帧 render item 时复位。组件不再持该计数。
        // ⚠ A4 会把重试/降频状态移到**每实例侧**（共享记录不得承载每实例状态）：
        //   同键的健康兄弟每帧复位，会持续清零失败者的计数，可能掩盖 D9 的 masked
        //   失败告警/降频——只影响诊断，不影响渲染；A4 随每实例侧一并解决。

    public:

        MaterialComponent(const std::string &name = "MaterialRuntime");
        ~MaterialComponent() override = default;

    public:

        void MarkValid();
        void MarkProgramResolved();
        void MarkResourcesPending();
        void MarkFailed();
        void ClearMaterializationRows();

        void OnAttach() override;
        void OnDetach() override;
    };

    /// 槽位映射：`MaterialComponent` 就是材质**运行期**层（构造名亦为 "MaterialRuntime"）
    template<> struct ComponentTypeOf<MaterialComponent> { static constexpr ComponentType value = ComponentType::MaterialRuntime; };
}//namespace hgl::ecs
