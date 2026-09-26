#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/graph/module/MaterialTextureReferencePool.h>
#include<hgl/mtl/MaterialRecipe.h>

namespace hgl::graph
{
    class ShaderProgram;
}

namespace hgl::ecs
{
    class MaterialComponent : public Component
    {
    public:

        // Runtime shared program, resolved by ECS.
        hgl::graph::ShaderProgram *program = nullptr;

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
        uint64_t program_build_context_hash = 0;

        // ── ShadowCaster 程序槽（阴影 pass 专用，与上面的 forward 槽完全独立）──
        // 同一物体每帧先在阴影 pass 采深度、再在主帧做着色，两个 pass 的程序
        // purpose 不同。若共用一个 program 单槽，purpose 每帧 Forward↔Shadow
        // 乒乓会让 InvalidateRecipeRuntime 反复 retire 纹理配置、
        // MaterializeRecipeRows 的无行早退把 valid 打成 false，从而禁用
        // P1-1 全干净帧快路径（每帧每物体两次完整物化链）。
        // ShadowCaster 模板无 material/sky descriptors，不需要物化行与纹理
        // 配置，只持 program 与 CreatePipeline 消费的 normalized recipe。
        hgl::graph::ShaderProgram *shadow_program = nullptr;
        uint64_t shadow_program_build_context_hash = 0;
        uint32_t shadow_tracked_material_authored_generation = 0;
        graph::mtl::MaterialRecipe shadow_cached_normalized_recipe{};

        // Cached normalized recipe — avoids redundant NormalizeRecipe in CreatePipeline.
        // Its validity is tracked by recipe_hash (same value that produced it).
        graph::mtl::MaterialRecipe cached_normalized_recipe{};

        // Cached effective recipe built with the resolved program.
        // Direct resource preparation and BDA materialization consume it.
        graph::mtl::MaterialRecipe cached_effective_recipe{};
        uint64_t cached_effective_recipe_hash = 0;

        // P3: Tracks the last observed PrimitiveComponent::material_authored_generation.
        // When this matches the primitive's current generation, all cached material
        // data is valid and ResolveMaterialProgramForPrimitive can skip entirely.
        uint32_t tracked_material_authored_generation = 0;

        // Epoch of the last materialization pass in which this primitive's
        // rows were written. A mismatch with the system's current epoch means
        // this primitive was skipped (e.g. invisible), so its rows must be
        // re-materialized before rendering.
        uint64_t last_materialize_epoch = 0;

        // ── D9：阴影 pass 跳过路径的重试收敛状态 ──
        // 阴影 pass 需要该 caster 但本帧画不了（masked 行未就绪 / 程序解析失败 /
        // 几何或管线失败）时递增；该 caster 成功产出本帧 render item（shadow_program
        // 非空）时清零。用途：① 首次跳过告警一次（不再静默，也不逐帧刷屏）；
        // ② 收敛上限——连续跳过超过阈值后把静态级联失效从"每帧"降频为周期性，
        // 否则持续失败会退化成"每帧 bump → 静态级联每帧全量重画"且全程无日志。
        uint32_t shadow_retry_frames = 0;

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
}//namespace hgl::ecs
