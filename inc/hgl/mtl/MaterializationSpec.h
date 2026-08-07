#pragma once

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl
{
    // Resolve 阶段产出的“纹理绑定结果”。
    // 作用：把 Recipe 的逻辑纹理槽（TextureSlot）落到可直接驱动 GPU 的索引信息。
    struct ResolvedResource
    {
        TextureSlot slot = TextureSlot::BaseColor; // 语义槽（BaseColor/Normal/Metallic...）
        uint32_t bindless_handle = 0;              // Bindless 纹理句柄（shader 直接采样入口）
        uint32_t texture_layer = 0;                // 纹理层/间接索引表位置（实例装配时使用）
    };

    // Resolve 阶段产出的“结构体数据引用结果”。
    // 作用：把 Recipe 的结构体槽位映射到具体 SSBO 池与结构体偏移/步长。
    struct ResolvedStructRef
    {
        uint32_t data_slot = DefaultMaterialDataSlot; // DataIndex 行槽位
        SSBOType ssbo_type = SSBOType::UserDefined; // 目标 SSBO 类型（契约主字段）
        uint32_t ssbo_id = 0;                       // 目标 SSBO 资源 ID（主字段，P1.55）
        uint32_t data_index = 0;                  // 结构数据索引（默认 0；可由作者层显式指定）
        uint32_t byte_stride = 0;                   // 同类结构体的字节步长
    };

    // MaterializationSpec 是 MaterialRecipe 的“已物化 IR”：
    // - Recipe 只表达“要什么”；
    // - Spec 表达“最终怎么取到资源与数据”。
    // 它是后续批处理、shader 变体选择与缓存键计算的核心输入。
    struct MaterializationSpec
    {
        uint64_t recipe_hash = 0; // 上游 Recipe 的稳定哈希（输入身份）
        uint64_t spec_hash = 0;   // 物化后 Spec 哈希（输出身份，可作缓存键）

        bool double_sided = false; // 光栅双面开关
        bool alpha_test = false;   // 是否启用 alpha test
        float alpha_cutoff = 0.5f; // alpha test 阈值

        std::vector<ResolvedResource> resources; // 所有已解析纹理资源
        std::vector<ResolvedStructRef> struct_refs; // 所有已解析结构体数据引用
    };

    // 计算 MaterializationSpec 的稳定内容哈希。
    // 目的：作为 Shader/PSO/布局缓存键，保证同内容同 key，不同内容不同 key。
    // 注意：不纳入 data_index 等运行时分配位置信息，避免跨帧分配顺序扰动缓存键。
    inline uint64_t HashMaterializationSpec(const MaterializationSpec &spec) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();

        hash = hgl::hash::FNV1aAppendValueBytes(hash, spec.recipe_hash);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, spec.double_sided);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, spec.alpha_test);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, spec.alpha_cutoff);

        const uint32_t resource_count = static_cast<uint32_t>(spec.resources.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, resource_count);
        for (const auto &resource : spec.resources)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(hash, resource.slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, resource.bindless_handle);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, resource.texture_layer);
        }

        const uint32_t struct_count = static_cast<uint32_t>(spec.struct_refs.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, struct_count);
        for (const auto &ref : spec.struct_refs)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(hash, ref.data_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, ref.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, ref.ssbo_id);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, ref.byte_stride);
        }

        return static_cast<uint64_t>(hash);
    }

    // 由 Recipe 生成一个“未解析资源”的 Spec 初稿：
    // - 拷贝与渲染状态相关的公共字段；
    // - 计算 recipe_hash；
    // - resources/struct_refs 留空，等待后续 Resolve 阶段填充。
    inline MaterializationSpec MakeMaterializationSpecSkeleton(const MaterialRecipe &recipe)
    {
        MaterializationSpec spec;

        spec.recipe_hash = HashMaterialRecipe(recipe);
        spec.double_sided = recipe.double_sided;
        spec.alpha_test = recipe.alpha_test;
        spec.alpha_cutoff = recipe.alpha_cutoff;

        spec.spec_hash = HashMaterializationSpec(spec);
        return spec;
    }

    // 当 Resolve 阶段写入 resources/struct_refs 后，调用该函数刷新 spec_hash。
    inline void RefreshMaterializationSpecHash(MaterializationSpec &spec) noexcept
    {
        spec.spec_hash = HashMaterializationSpec(spec);
    }
}
