#include <hgl/common/RenderOptions.h>
#include <hgl/ecs/support/material/ModelTemplateBuilder.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/graph/asset/ModelTemplateManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
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
            ValueArray<uint32_t> slot_geometries;
            ValueArray<uint32_t> slot_counts;
            bool casts_shadow = false;
            const graph::GeometryVertexFormat *vertex_format = nullptr;
            graph::PrimitiveType primitive_type = graph::PrimitiveType::Triangles;
        };

        struct PartBuildInfo
        {
            hgl::math::Matrix4f local_matrix{1.0f};
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
        };
    }

    uint32_t BuildMultiMaterialModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                            ECSContext *context,
                                            const graph::ModelAssetContent &content,
                                            const MultiMaterialModelTemplateDesc &desc)
    {
        ValueArray<ModelTemplatePartInput> parts;
        for (const auto &node : content.nodes)
        {
            for (const int32_t pi : node.primitiveIndices)
            {
                if (pi < 0 || static_cast<size_t>(pi) >= content.assets.size())
                {
                    GLogError(u8"[BuildMultiMaterialModelTemplate] Invalid primitive index: %d", pi);
                    return 0;
                }
                const auto &asset = content.assets[static_cast<size_t>(pi)];
                const int material_index = desc.by_material_index
                    ? asset.GetMaterialIndex() : parts.GetCount();
                if (material_index < 0 || material_index >= desc.material_inputs.GetCount())
                {
                    GLogError(u8"[BuildMultiMaterialModelTemplate] Missing material input: %d", material_index);
                    return 0;
                }
                ModelTemplatePartInput part;
                part.geometry = asset.GetGeometry();
                part.primitive_type = asset.GetPrimitiveType();
                part.local_matrix = node.worldMatrix;
                part.local_bounds.Clear();
                if (node.boundsValid)
                    part.local_bounds = node.nodeBounds.aabb;
                part.material = desc.material_inputs[material_index];
                parts.Add(part);
            }
        }
        return BuildMultiMaterialModelTemplate(registry, context, parts.GetData(),
                                                static_cast<uint32_t>(parts.GetCount()));
    }

    uint32_t BuildMultiMaterialModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                            ECSContext *context,
                                            const ModelTemplatePartInput *parts,
                                            const uint32_t part_count,
                                            ModelTemplateBuildStatus *status)
    {
        using graph::ssbo::ModelTemplateRow;
        using graph::ssbo::ModelPartRow;
        using graph::ssbo::kInvalidModelTemplateID;

        if (status)
            *status = ModelTemplateBuildStatus::Failed;

        if (!registry || !context || !parts || part_count == 0u)
        {
            GLogError(u8"[BuildMultiMaterialModelTemplate] registry 或 context 为空");
            return kInvalidModelTemplateID;
        }

        MaterialRuntimeTable *runtime_table = context->RenderData().RuntimeMaterials();
        ModelTemplateMaterialTable *mat_table = context->RenderData().TemplateMaterials();
        if (!runtime_table || !mat_table)
        {
            GLogError(u8"[BuildMultiMaterialModelTemplate] Material tables unavailable");
            return kInvalidModelTemplateID;
        }
        auto *graphics = context->GetGraphicsContext();
        const auto *part_pool = registry->GetPool(GlobalSSBOType::ModelPartPool);
        if (!graphics || graphics->GetGlobalSSBOBufferRegistry() != registry
         || !part_pool || part_count >= part_pool->GetRowCapacity()
         || part_count > uint32_t(INT32_MAX))
        {
            GLogError(u8"[BuildMultiMaterialModelTemplate] Invalid resource domain or part capacity=%u", part_count);
            return kInvalidModelTemplateID;
        }

        // Validate the entire input before allocating any GPU rows.
        for (uint32_t p = 0; p < part_count; ++p)
        {
            const auto &input = parts[p];
            const auto *row = runtime_table->Get(input.material.runtime_row);
            if (input.material.runtime_row != INVALID_MATERIAL_RUNTIME_ROW_ID && !row)
            {
                GLogError(u8"[BuildMultiMaterialModelTemplate] Dead material row: part=%u row=%u",
                          p, input.material.runtime_row);
                return kInvalidModelTemplateID;
            }
            if (!input.geometry || input.geometry->GetGeometryID() == 0u
             || !row || row->data_index_row == uint32_t(-1))
            {
                if (status)
                    *status = ModelTemplateBuildStatus::Pending;
                return kInvalidModelTemplateID;
            }
            if (input.geometry->GetMeshDrawParamsRegistry() != registry
             || !registry->IsActive(GlobalSSBOType::MeshDrawParams, input.geometry->GetGeometryID()))
            {
                GLogError(u8"[BuildMultiMaterialModelTemplate] Geometry belongs to an unavailable domain: part=%u", p);
                return kInvalidModelTemplateID;
            }
        }

        PtrArray<InternalGroupInfo> groups;
        ValueArray<PartBuildInfo> part_infos;
        part_infos.Reserve(static_cast<int>(part_count));

        hgl::math::AABB merged_box;
        merged_box.Clear();
        bool any_bounds = false;

        for (uint32_t p = 0; p < part_count; ++p)
        {
            const ModelTemplatePartInput &input = parts[p];

            const bool bounds_ok = !input.local_bounds.IsEmpty();
            const hgl::math::Vector3f part_center = bounds_ok ? input.local_bounds.GetCenter()
                                                              : hgl::math::Vector3f(0.0f, 0.0f, 0.0f);
            const hgl::math::Vector3f part_extent = bounds_ok ? input.local_bounds.GetExtent()
                                                              : hgl::math::Vector3f(0.0f, 0.0f, 0.0f);
            const float part_radius = bounds_ok ? input.local_bounds.GetDiagonal() * 0.5f
                                                : graph::GlobalSSBOBufferRegistry::kAlwaysVisibleRadius;

            if (bounds_ok)
            {
                merged_box.Merge(input.local_bounds.Transformed(input.local_matrix));
                any_bounds = true;
            }

            {
                graph::Geometry *geo = input.geometry;
                const MultiMaterialPartInput &part_input = input.material;

                uint32_t mat_id = part_input.material_id;
                uint32_t tex_id = part_input.texture_id;

                if (part_input.runtime_row != INVALID_MATERIAL_RUNTIME_ROW_ID && runtime_table)
                {
                    const MaterialRuntimeRow *r_row = runtime_table->Get(part_input.runtime_row);
                    if (r_row)
                    {
                        if (r_row->data_index_row != uint32_t(-1))
                            mat_id = r_row->data_index_row;
                        if (r_row->material_texture_configuration.IsValid())
                            tex_id = r_row->material_texture_configuration.row_index;
                    }
                }

                const uint32_t geo_id = geo ? geo->GetGeometryID() : 0u;

                // 2. 匹配或划分 Shading Group
                uint32_t group_idx = static_cast<uint32_t>(groups.GetCount());
                for (int g = 0; g < groups.GetCount(); ++g)
                {
                    const InternalGroupInfo &grp = groups[g];
                    if (grp.primitive_type != input.primitive_type)
                        continue;
                    if (grp.vertex_format->GetVertexInputHash()
                        != geo->GetGeometryVertexFormat().GetVertexInputHash())
                        continue;
                    if (part_input.runtime_row != INVALID_MATERIAL_RUNTIME_ROW_ID)
                    {
                        if (grp.runtime_row == part_input.runtime_row)
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
                    new_grp.casts_shadow = part_input.can_cast_shadow;
                    new_grp.vertex_format = &geo->GetGeometryVertexFormat();
                    new_grp.primitive_type = input.primitive_type;
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
                pinfo.local_matrix = input.local_matrix;
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

                part_infos.Add(pinfo);
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
            row.local_matrix = info.local_matrix;
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
        binding.owns_gpu_rows = true;

        for (int g = 0; g < groups.GetCount(); ++g)
        {
            const InternalGroupInfo &grp = groups[g];
            TemplateShadingGroupDesc gdesc;
            gdesc.runtime_row = grp.runtime_row;
            gdesc.slot_geometry = grp.slot_geometries;
            gdesc.slot_count = grp.slot_counts;
            gdesc.casts_shadow = grp.casts_shadow;
            gdesc.vertex_format = grp.vertex_format;
            gdesc.primitive_type = grp.primitive_type;

            binding.groups.Add(gdesc);

            if (grp.runtime_row != INVALID_MATERIAL_RUNTIME_ROW_ID)
            {
                if (!binding.owned_part_rows.Contains(grp.runtime_row))
                    binding.owned_part_rows.Add(grp.runtime_row);
            }
        }

        if (!mat_table->RegisterBinding(binding))
        {
            GLogError(u8"[BuildMultiMaterialModelTemplate] Binding publication failed: template=%u", template_id);
            registry->ReleaseRowRange(GlobalSSBOType::ModelPartPool, part_first, part_count);
            registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);
            return kInvalidModelTemplateID;
        }

        if (status)
            *status = ModelTemplateBuildStatus::Ready;
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
        using graph::ssbo::kModelPartFlagCanCastShadow;
        using graph::ssbo::kModelPartFlagIsOpaque;

        if (!registry || !context || geometry_id == 0)
        {
            GLogError(u8"[BuildSinglePrimitiveModelTemplate] 参数无效: registry=%p, context=%p, geometry_id=%u",
                      registry, context, geometry_id);
            return kInvalidModelTemplateID;
        }

        ModelTemplateMaterialTable *mat_table = context->RenderData().TemplateMaterials();
        if (!mat_table)
        {
            GLogError(u8"[BuildSinglePrimitiveModelTemplate] context 未初始化 ModelTemplateMaterialTable");
            return kInvalidModelTemplateID;
        }

        // 检查缓存
        SinglePrimitiveTemplateKey cache_key;
        cache_key.geometry_id = geometry_id;
        cache_key.can_cast_shadow = desc.can_cast_shadow;
        cache_key.is_opaque = desc.is_opaque;
        cache_key.primitive_type = desc.primitive_type;

        const uint32_t existing_template_id = mat_table->FindSinglePrimitiveTemplate(cache_key);
        if (existing_template_id != kInvalidModelTemplateID)
        {
            return existing_template_id;
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
        part_row.material_id = 0;
        part_row.texture_id = 0;
        part_row.shading_group = 0;
        part_row.bucket_slot = 0;
        part_row.part_flags = 0;
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
        binding.owns_gpu_rows = true;
        binding.uses_instance_material = true;

        TemplateShadingGroupDesc grp;
        grp.casts_shadow = desc.can_cast_shadow;
        grp.primitive_type = desc.primitive_type;
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

        auto asset_desc = desc;
        asset_desc.primitive_type = asset->GetPrimitiveType();
        return BuildSinglePrimitiveModelTemplate(registry, context, asset->GetGeometry(), asset_desc);
    }

    bool DestroyMultiMaterialModelTemplate(graph::GlobalSSBOBufferRegistry *registry,
                                          ECSContext *context,
                                          uint32_t template_id)
    {
        if (template_id == 0)
            return false;

        if (!registry || !context)
        {
            GLogError(u8"[DestroyMultiMaterialModelTemplate] Missing registry/context");
            return false;
        }
        bool referenced = false;
        context->ForEachAliveEntity([&](const Entity *entity)
        {
            const auto *ref = entity->GetComponent<GPUModelRefComponent>();
            referenced |= ref && ref->GetTemplateID() == template_id;
        });
        if (referenced)
        {
            GLogError(u8"[DestroyMultiMaterialModelTemplate] Template still has live instances: %u", template_id);
            return false;
        }
        if (!graph::ModelTemplateManager::RetireModelTemplate(
                registry, template_id, context->GetRenderSubmissionSerial()
                                       + graph::MaterialTextureConfigurationRetireEpochDelay, context))
            return false;
        auto *table = context->RenderData().TemplateMaterials();
        if (table && table->HasBinding(template_id))
            return table->UnregisterBinding(template_id);
        return true;
    }
}
