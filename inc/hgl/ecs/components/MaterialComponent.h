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

        // Arena 行寻址（W3.3 后按 SSBOType 独立缓冲）：实例数据行的
        // CPU 映射基址（行尾句柄直写）与 GPU 设备地址（地址行表引用）。
        void    *material_row_cpu = nullptr;
        uint64_t material_row_gpu = 0;

        // 独立 MaterialTextureReferencePool 配置行。阶段 3 只保存句柄；
        // 阶段 4 负责按 MaterialDefinition layout 申请、写入和退休。
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
