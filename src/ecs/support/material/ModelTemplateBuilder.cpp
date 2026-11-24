#include <hgl/ecs/support/material/ModelTemplateBuilder.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/graph/asset/ModelTemplateManager.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/math/geometry/AABB.h>
#include <hgl/math/geometry/BoundingVolumes.h>
#include <hgl/log/Log.h>
#include <hgl/type/PtrArray.h>

namespace hgl::ecs
{
    using namespace hgl::graph;

    namespace
    {
        struct InternalGroupInfo
        {
            MaterialRuntimeRowID runtime_row = INVALID_MATERIAL_RUNTIME_ROW_ID;
            uint32_t material_id = 0;
            ValueArray<uint32_t> slot_geometries;
            ValueArray<uint32_t> slot_counts;
            bool casts_shadow = false;
        };

        struct PartBuildInfo
        {
            const graph::StaticMeshNode *node = nullptr;
            const graph::PrimitiveAsset *asset = nullptr;
            uint32_t geometry_id = 0;
            uint32_t material_id = 0;
            uint32_t texture_id = 0;
            uint32_t group_index = 0;
            uint32_t bucket_slot = graph::ssbo::kModelPartBucketSlotNone;
            bool can_cast_shadow = true;
            bool is_opaque = true;
            hgl::math::Vector3f part_center{0.0f, 0.0f, 0.0f};
            hgl::math::Vector3f part_extent{0.0f, 0.0f, 0.0f};
            float part_radius = 0.0f;
            bool bounds_ok = false;
        };
    }

    uint32_t BuildMultiMaterialModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                            ECSContext *context,
                                            const graph::ModelAssetContent &content,
                                            const MultiMaterialModelTemplateDesc &desc)
    {
        using graph::ssbo::ModelTemplateRow;
        using graph::ssbo::ModelPartRow;
        using graph::ssbo::kInvalidModelTemplateID;

        if (!registry || !context)
        {
            GLogError(u8"[BuildMultiMaterialModelTemplate] registry 或 context 为空");
            return kInvalidModelTemplateID;
        }

        const size_t node_count = content.nodes.size();
        const size_t asset_count = content.assets.size();

        // 1. 统计有效部件数并收集基础信息
        uint32_t part_count = 0;
        for (size_t n = 0; n < node_count; ++n)
        {
            const graph::StaticMeshNode &node = content.nodes[n];
            for (int32_t pi : node.primitiveIndices)
            {
                if (pi >= 0 && static_cast<size_t>(pi) < asset_count)
                    ++part_count;
            }
        }

        if (part_count == 0)
        {
            GLogWarning(u8"[BuildMultiMaterialModelTemplate] 无有效部件可打包");
            return kInvalidModelTemplateID;
        }

        MaterialRuntimeTable *runtime_table = context->GetMaterialRuntimeTable();

        PtrArray<InternalGroupInfo> groups;
        ValueArray<PartBuildInfo> part_infos;
        part_infos.Reserve(static_cast<int>(part_count));

        hgl::math::AABB merged_box;
        merged_box.Clear();
        bool any_bounds = false;

        uint32_t current_part_flat_idx = 0;

        for (size_t n = 0; n < node_count; ++n)
        {
            const graph::StaticMeshNode &node = content.nodes[n];

            const bool bounds_ok = node.boundsValid && !node.nodeBounds.aabb.IsEmpty();
            const hgl::math::Vector3f part_center = bounds_ok ? node.nodeBounds.aabb.GetCenter()
                                                              : hgl::math::Vector3f(0.0f, 0.0f, 0.0f);
            const hgl::math::Vector3f part_extent = bounds_ok ? node.nodeBounds.aabb.GetExtent()
                                                              : hgl::math::Vector3f(0.0f, 0.0f, 0.0f);
            const float part_radius = bounds_ok ? node.nodeBounds.aabb.GetDiagonal() * 0.5f
                                                : graph::GlobalSSBOBufferRegistry::kAlwaysVisibleRadius;

            if (bounds_ok)
            {
                merged_box.Merge(node.nodeBounds.aabb.Transformed(node.worldMatrix));
                any_bounds = true;
            }

            for (int32_t pi : node.primitiveIndices)
            {
                if (pi < 0 || static_cast<size_t>(pi) >= asset_count)
                    continue;

                const graph::PrimitiveAsset &asset = content.assets[static_cast<size_t>(pi)];
                graph::Geometry *geo = asset.GetGeometry();

                MultiMaterialPartInput part_input{};
                if (desc.by_material_index)
                {
                    const int32_t mat_idx = asset.GetMaterialIndex();
                    if (mat_idx >= 0 && mat_idx < desc.material_inputs.GetCount())
                        part_input = desc.material_inputs[mat_idx];
                }
                else
                {
                    if (static_cast<int>(current_part_flat_idx) < desc.material_inputs.GetCount())
                        part_input = desc.material_inputs[static_cast<int>(current_part_flat_idx)];
                }

                uint32_t mat_id = part_input.material_id;
                uint32_t tex_id = part_input.texture_id;

                if (part_input.runtime_row != INVALID_MATERIAL_RUNTIME_ROW_ID && runtime_table)
                {
                    const MaterialRuntimeRow *r_row = runtime_table->Get(part_input.runtime_row);
                    if (r_row)
                    {
                        if (r_row->data_index_row != uint32_t(-1))
                            mat_id = r_row->data_index_row;
                        if (r_row->material_texture_configuration.row_index != 0)
                            tex_id = r_row->material_texture_configuration.row_index;
                    }
                }

                const uint32_t geo_id = geo ? geo->GetGeometryID() : 0u;

                // 2. 匹配或划分 Shading Group
                uint32_t group_idx = static_cast<uint32_t>(groups.GetCount());
                for (int g = 0; g < groups.GetCount(); ++g)
                {
                    const InternalGroupInfo &grp = groups[g];
                    if (part_input.runtime_row != INVALID_MATERIAL_RUNTIME_ROW_ID)
                    {
                        if (grp.runtime_row == part_input.runtime_row)
                        {
                            group_idx = static_cast<uint32_t>(g);
                            break;
                        }
                    }
                    else
                    {
                        if (grp.material_id == mat_id && grp.runtime_row == INVALID_MATERIAL_RUNTIME_ROW_ID)
                        {
                            group_idx = static_cast<uint32_t>(g);
                            break;
                        }
                    }
                }

                if (group_idx == static_cast<uint32_t>(groups.GetCount()))
                {
                    InternalGroupInfo new_grp;
                    new_grp.runtime_row = part_input.runtime_row;
                    new_grp.material_id = mat_id;
                    new_grp.casts_shadow = part_input.can_cast_shadow;
                    groups.Add(new_grp);
                }

                InternalGroupInfo &target_grp = groups[static_cast<int>(group_idx)];
                if (part_input.can_cast_shadow)
                    target_grp.casts_shadow = true;

                // 3. 组内计算 bucket_slot
                uint32_t slot = graph::ssbo::kModelPartBucketSlotNone;
                if (geo_id != 0u)
                {
                    for (int s = 0; s < target_grp.slot_geometries.GetCount(); ++s)
                    {
                        if (target_grp.slot_geometries[s] == geo_id)
                        {
                            slot = static_cast<uint32_t>(s);
                            target_grp.slot_counts[s] += 1;
                            break;
                        }
                    }

                    if (slot == graph::ssbo::kModelPartBucketSlotNone)
                    {
                        slot = static_cast<uint32_t>(target_grp.slot_geometries.GetCount());
                        target_grp.slot_geometries.Add(geo_id);
                        target_grp.slot_counts.Add(1);
                    }
                }

                PartBuildInfo pinfo;
                pinfo.node = &node;
                pinfo.asset = &asset;
                pinfo.geometry_id = geo_id;
                pinfo.material_id = mat_id;
                pinfo.texture_id = tex_id;
                pinfo.group_index = group_idx;
                pinfo.bucket_slot = slot;
                pinfo.can_cast_shadow = part_input.can_cast_shadow;
                pinfo.is_opaque = part_input.is_opaque;
                pinfo.part_center = part_center;
                pinfo.part_extent = part_extent;
                pinfo.part_radius = part_radius;
                pinfo.bounds_ok = bounds_ok;

                part_infos.Add(pinfo);
                ++current_part_flat_idx;
            }
        }

        // 4. 统计素材桶总数
        uint32_t total_bucket_count = 0;
        for (int g = 0; g < groups.GetCount(); ++g)
        {
            total_bucket_count += static_cast<uint32_t>(groups[g].slot_geometries.GetCount());
        }

        // 5. 申请 GPU SSBO 行号
        const uint32_t template_id = registry->Acquire(GlobalSSBOType::ModelTemplatePool);
        if (template_id == ActiveRowPool::InvalidRowID)
        {
            GLogError(u8"[BuildMultiMaterialModelTemplate] ModelTemplatePool 发号失败");
            return kInvalidModelTemplateID;
        }

        const uint32_t part_first = registry->AcquireRowRange(GlobalSSBOType::ModelPartPool, part_count);
        if (part_first == ActiveRowPool::InvalidRowID)
        {
            GLogError(u8"[BuildMultiMaterialModelTemplate] ModelPartPool 发号失败，count=%u", part_count);
            registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);
            return kInvalidModelTemplateID;
        }

        // 6. 拼装 ModelPartRow 连续区间
        ValueArray<ModelPartRow> part_rows;
        part_rows.Reserve(static_cast<int>(part_count));

        for (int i = 0; i < part_infos.GetCount(); ++i)
        {
            const PartBuildInfo &info = part_infos[i];
            ModelPartRow row;
            row.local_matrix = info.node->worldMatrix;
            row.aabb_center_radius = hgl::math::Vector4f(info.part_center.x, info.part_center.y, info.part_center.z, info.part_radius);
            row.aabb_extents = hgl::math::Vector4f(info.part_extent.x, info.part_extent.y, info.part_extent.z, 0.0f);
            row.geometry_id = info.geometry_id;
            row.material_id = info.material_id;
            row.texture_id = info.texture_id;
            row.shading_group = info.group_index;
            row.bucket_slot = info.bucket_slot;

            row.part_flags = graph::ssbo::kModelPartFlagHasMaterial;
            if (info.geometry_id != 0u && info.can_cast_shadow)
                row.part_flags |= graph::ssbo::kModelPartFlagCanCastShadow;
            if (info.is_opaque)
                row.part_flags |= graph::ssbo::kModelPartFlagIsOpaque;

            part_rows.Add(row);
        }

        // 7. 写入 GPU 部件区间
        if (!registry->WriteRowRange(GlobalSSBOType::ModelPartPool, part_first, part_rows.GetData(), part_count))
        {
            GLogError(u8"[BuildMultiMaterialModelTemplate] ModelPartPool 写入失败");
            registry->ReleaseRowRange(GlobalSSBOType::ModelPartPool, part_first, part_count);
            registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);
            return kInvalidModelTemplateID;
        }

        // 8. 拼装并写入 ModelTemplateRow
        ModelTemplateRow trow;
        trow.part_offset = part_first;
        trow.part_count = part_count;
        trow.part_bucket_count = total_bucket_count;
        trow.shading_group_count = static_cast<uint32_t>(groups.GetCount() > 0 ? groups.GetCount() : 1);

        if (any_bounds)
        {
            const hgl::math::Vector3f c = merged_box.GetCenter();
            const hgl::math::Vector3f e = merged_box.GetExtent();
            trow.local_bound_center_radius = hgl::math::Vector4f(c.x, c.y, c.z, merged_box.GetDiagonal() * 0.5f);
            trow.local_bound_extents = hgl::math::Vector4f(e.x, e.y, e.z, 0.0f);
        }

        if (!registry->Write(GlobalSSBOType::ModelTemplatePool, template_id, trow))
        {
            GLogError(u8"[BuildMultiMaterialModelTemplate] ModelTemplatePool 写入失败");
            registry->ReleaseRowRange(GlobalSSBOType::ModelPartPool, part_first, part_count);
            registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);
            return kInvalidModelTemplateID;
        }

        // 9. 登记 TemplateRenderBinding 到 ModelTemplateMaterialTable
        TemplateRenderBinding binding;
        binding.template_id = template_id;

        for (int g = 0; g < groups.GetCount(); ++g)
        {
            const InternalGroupInfo &grp = groups[g];
            TemplateShadingGroupDesc gdesc;
            gdesc.runtime_row = grp.runtime_row;
            gdesc.slot_geometry = grp.slot_geometries;
            gdesc.slot_count = grp.slot_counts;
            gdesc.casts_shadow = grp.casts_shadow;

            binding.groups.Add(gdesc);

            if (grp.runtime_row != INVALID_MATERIAL_RUNTIME_ROW_ID)
            {
                if (!binding.owned_part_rows.Contains(grp.runtime_row))
                    binding.owned_part_rows.Add(grp.runtime_row);
            }
        }

        ModelTemplateMaterialTable *mat_table = context->GetModelTemplateMaterialTable();
        if (mat_table)
        {
            mat_table->RegisterBinding(binding);
        }

        return template_id;
    }

    uint32_t BuildSinglePrimitiveModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                              ECSContext *context,
                                              uint32_t geometry_id,
                                              const hgl::math::AABB &local_aabb,
                                              const SinglePrimitiveModelTemplateDesc &desc)
    {
        using graph::ssbo::ModelTemplateRow;
        using graph::ssbo::ModelPartRow;
        using graph::ssbo::kInvalidModelTemplateID;
        using graph::ssbo::kModelPartFlagHasMaterial;
        using graph::ssbo::kModelPartFlagCanCastShadow;
        using graph::ssbo::kModelPartFlagIsOpaque;

        if (!registry || !context || geometry_id == 0)
        {
            GLogError(u8"[BuildSinglePrimitiveModelTemplate] 参数无效: registry=%p, context=%p, geometry_id=%u",
                      registry, context, geometry_id);
            return kInvalidModelTemplateID;
        }

        ModelTemplateMaterialTable *mat_table = context->GetModelTemplateMaterialTable();
        if (!mat_table)
        {
            GLogError(u8"[BuildSinglePrimitiveModelTemplate] context 未初始化 ModelTemplateMaterialTable");
            return kInvalidModelTemplateID;
        }

        // 检查缓存
        SinglePrimitiveTemplateKey cache_key;
        cache_key.geometry_id = geometry_id;
        cache_key.runtime_row = desc.material_input.runtime_row;
        cache_key.can_cast_shadow = desc.can_cast_shadow;
        cache_key.is_opaque = desc.is_opaque;

        const uint32_t existing_template_id = mat_table->FindSinglePrimitiveTemplate(cache_key);
        if (existing_template_id != kInvalidModelTemplateID)
        {
            return existing_template_id;
        }

        // 自动补齐 material_id / texture_id
        uint32_t effective_mat_id = desc.material_input.material_id;
        uint32_t effective_tex_id = desc.material_input.texture_id;
        if (effective_mat_id == 0 && effective_tex_id == 0 && desc.material_input.runtime_row != INVALID_MATERIAL_RUNTIME_ROW_ID)
        {
            MaterialRuntimeTable *runtime_table = context->GetMaterialRuntimeTable();
            if (runtime_table)
            {
                if (const MaterialRuntimeRow *row = runtime_table->Get(desc.material_input.runtime_row))
                {
                    if (row->data_index_row != uint32_t(-1))
                        effective_mat_id = row->data_index_row;
                    if (row->material_texture_configuration.IsValid())
                        effective_tex_id = row->material_texture_configuration.row_index;
                }
            }
        }

        // 申请 GPU SSBO 行号
        const uint32_t template_id = registry->Acquire(GlobalSSBOType::ModelTemplatePool);
        if (template_id == ActiveRowPool::InvalidRowID)
        {
            GLogError(u8"[BuildSinglePrimitiveModelTemplate] ModelTemplatePool 发号失败");
            return kInvalidModelTemplateID;
        }

        const uint32_t part_first = registry->AcquireRowRange(GlobalSSBOType::ModelPartPool, 1);
        if (part_first == ActiveRowPool::InvalidRowID)
        {
            GLogError(u8"[BuildSinglePrimitiveModelTemplate] ModelPartPool 发号失败");
            registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);
            return kInvalidModelTemplateID;
        }

        const bool bounds_ok = !local_aabb.IsEmpty();
        const hgl::math::Vector3f part_center = bounds_ok ? local_aabb.GetCenter()
                                                          : hgl::math::Vector3f(0.0f, 0.0f, 0.0f);
        const hgl::math::Vector3f part_extent = bounds_ok ? local_aabb.GetExtent()
                                                          : hgl::math::Vector3f(0.0f, 0.0f, 0.0f);
        const float part_radius = bounds_ok ? local_aabb.GetDiagonal() * 0.5f
                                            : graph::GlobalSSBOBufferRegistry::kAlwaysVisibleRadius;

        // 拼装 ModelPartRow
        ModelPartRow part_row;
        part_row.local_matrix = hgl::math::Identity4f;
        part_row.aabb_center_radius = hgl::math::Vector4f(part_center.x, part_center.y, part_center.z, part_radius);
        part_row.aabb_extents = hgl::math::Vector4f(part_extent.x, part_extent.y, part_extent.z, 0.0f);
        part_row.geometry_id = geometry_id;
        part_row.material_id = effective_mat_id;
        part_row.texture_id = effective_tex_id;
        part_row.shading_group = 0;
        part_row.bucket_slot = 0;
        part_row.part_flags = kModelPartFlagHasMaterial;
        if (desc.can_cast_shadow)
            part_row.part_flags |= kModelPartFlagCanCastShadow;
        if (desc.is_opaque)
            part_row.part_flags |= kModelPartFlagIsOpaque;

        if (!registry->WriteRowRange(GlobalSSBOType::ModelPartPool, part_first, &part_row, 1))
        {
            GLogError(u8"[BuildSinglePrimitiveModelTemplate] ModelPartPool 写入失败");
            registry->ReleaseRowRange(GlobalSSBOType::ModelPartPool, part_first, 1);
            registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);
            return kInvalidModelTemplateID;
        }

        // 拼装 ModelTemplateRow
        ModelTemplateRow tmpl_row;
        tmpl_row.part_offset = part_first;
        tmpl_row.part_count = 1;
        tmpl_row.part_bucket_count = 1;
        tmpl_row.shading_group_count = 1;
        tmpl_row.local_bound_center_radius = hgl::math::Vector4f(part_center.x, part_center.y, part_center.z, part_radius);
        tmpl_row.local_bound_extents = hgl::math::Vector4f(part_extent.x, part_extent.y, part_extent.z, 0.0f);

        if (!registry->Write(GlobalSSBOType::ModelTemplatePool, template_id, tmpl_row))
        {
            GLogError(u8"[BuildSinglePrimitiveModelTemplate] ModelTemplatePool 写入失败");
            registry->ReleaseRowRange(GlobalSSBOType::ModelPartPool, part_first, 1);
            registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);
            return kInvalidModelTemplateID;
        }

        // 注册 TemplateRenderBinding
        TemplateRenderBinding binding;
        binding.template_id = template_id;
        if (desc.material_input.runtime_row != INVALID_MATERIAL_RUNTIME_ROW_ID)
            binding.owned_part_rows.Add(desc.material_input.runtime_row);

        TemplateShadingGroupDesc grp;
        grp.runtime_row = desc.material_input.runtime_row;
        grp.casts_shadow = desc.can_cast_shadow;
        grp.slot_geometry.Add(geometry_id);
        grp.slot_count.Add(1);
        binding.groups.Add(grp);

        if (!mat_table->RegisterBinding(binding))
        {
            GLogError(u8"[BuildSinglePrimitiveModelTemplate] RegisterBinding 失败");
            registry->ReleaseRowRange(GlobalSSBOType::ModelPartPool, part_first, 1);
            registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);
            return kInvalidModelTemplateID;
        }

        mat_table->RegisterSinglePrimitiveTemplate(cache_key, template_id);
        return template_id;
    }

    uint32_t BuildSinglePrimitiveModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                              ECSContext *context,
                                              graph::Geometry *geo,
                                              const SinglePrimitiveModelTemplateDesc &desc)
    {
        if (!geo)
            return graph::ssbo::kInvalidModelTemplateID;

        if (geo->GetGeometryID() == 0 && registry && context)
        {
            geo->EnsureMeshDrawParams(registry, context->GetGPUDevice());
        }

        const uint32_t geometry_id = geo->GetGeometryID();
        const hgl::math::AABB local_aabb = geo->GetBoundingVolumes().aabb;
        return BuildSinglePrimitiveModelTemplate(registry, context, geometry_id, local_aabb, desc);
    }

    uint32_t BuildSinglePrimitiveModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                              ECSContext *context,
                                              const graph::PrimitiveAsset *asset,
                                              const SinglePrimitiveModelTemplateDesc &desc)
    {
        if (!asset)
            return graph::ssbo::kInvalidModelTemplateID;

        return BuildSinglePrimitiveModelTemplate(registry, context, asset->GetGeometry(), desc);
    }

    bool DestroyMultiMaterialModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                          ECSContext *context,
                                          uint32_t template_id)
    {
        if (template_id == 0)
            return false;

        if (context)
        {
            ModelTemplateMaterialTable *mat_table = context->GetModelTemplateMaterialTable();
            if (mat_table)
            {
                mat_table->UnregisterBinding(template_id);
            }
        }

        if (registry)
        {
            return graph::ModelTemplateManager::UnregisterModelTemplate(registry, template_id);
        }

        return true;
    }
}
