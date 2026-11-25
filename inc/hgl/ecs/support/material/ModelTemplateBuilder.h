#pragma once

#include <hgl/CoreType.h>
#include <hgl/type/ValueArray.h>
#include <hgl/graph/asset/ModelAsset.h>
#include <hgl/ecs/support/material/ModelTemplateMaterialTable.h>
#include <hgl/math/geometry/AABB.h>

namespace hgl::graph
{
    class GlobalSSBOBufferRegistry;
    class Geometry;
    class PrimitiveAsset;
}

namespace hgl::ecs
{
    class ECSContext;

    /**
     * 部件材质输入配置
     */
    struct MultiMaterialPartInput
    {
        MaterialRuntimeRowID runtime_row = INVALID_MATERIAL_RUNTIME_ROW_ID;
        uint32_t material_id = 0;
        uint32_t texture_id = 0;
        bool can_cast_shadow = true;
        bool is_opaque = true;
    };

    /**
     * 单图元模型模板构建描述
     */
    struct SinglePrimitiveModelTemplateDesc
    {
        MultiMaterialPartInput material_input{};
        bool can_cast_shadow = true;
        bool is_opaque = true;
    };

    /**
     * 多材质模型模板构建描述
     */
    struct MultiMaterialModelTemplateDesc
    {
        ValueArray<MultiMaterialPartInput> material_inputs;
        bool by_material_index = true; ///< true: 按 PrimitiveAsset::GetMaterialIndex() 索引；false: 按部件展开顺序索引
    };

    /**
     * @brief 构建并注册一个包含多材质绑定的 GPU 模型模板。
     *
     * 1. 遍历 content.nodes 与 primitiveIndices，打包所有部件为连续的 ModelPartRow；
     * 2. 根据 material_inputs 为每个部件映射 material_id, texture_id, shading_group, bucket_slot；
     * 3. 统计各 shading group 与素材桶总数，在 GlobalSSBOBufferRegistry 分配并写入 GPU 模板行与部件区间；
     * 4. 组装 TemplateRenderBinding 并在 ECSContext::GetModelTemplateMaterialTable() 中注册并增加材质行引用。
     *
     * @note material_inputs 必须提供已物化的 MaterialRuntimeRowID；资产实例化阶段的
     * 原始材质 SSBO 行不能代替运行期行。该接口保留用于显式多组模板构建。
     *
     * @return 模板池行号 (template_id != 0)，失败返回 0
     */
    uint32_t BuildMultiMaterialModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                            ECSContext *context,
                                            const graph::ModelAssetContent &content,
                                            const MultiMaterialModelTemplateDesc &desc);

    /**
     * @brief 构建并注册一个单图元极简 GPU 模型模板 (part_count=1, group_count=1)。
     * 具备按 (geometry_id, runtime_row, can_cast_shadow, is_opaque) 的全局去重缓存机制。
     *
     * @return 模板池行号 (template_id != 0)，失败返回 0
     */
    uint32_t BuildSinglePrimitiveModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                              ECSContext *context,
                                              uint32_t geometry_id,
                                              const hgl::math::AABB &local_aabb,
                                              const SinglePrimitiveModelTemplateDesc &desc);

    uint32_t BuildSinglePrimitiveModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                              ECSContext *context,
                                              graph::Geometry *geo,
                                              const SinglePrimitiveModelTemplateDesc &desc);

    uint32_t BuildSinglePrimitiveModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                              ECSContext *context,
                                              const graph::PrimitiveAsset *asset,
                                              const SinglePrimitiveModelTemplateDesc &desc);

    /**
     * @brief 注销多材质/单图元模型模板（同时注销材质绑定并归还 GPU 模板池行与部件区间）
     */
    bool DestroyMultiMaterialModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                          ECSContext *context,
                                          uint32_t template_id);
}
