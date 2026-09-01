#include <hgl/mtl/DescriptorContract.h>

#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/util/hash/FNV1a.h>
#include <algorithm>
#include <cstring>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    namespace
    {
        uint64 HashText(const char *text) noexcept
        {
            if (!text || !text[0])
                return 0;

            hgl::hash::FNV1aHasher64 h;
            h << text;
            return h;
        }

        uint64 GetLogicalResourceID(
            const SerializedDescriptorEntry &entry) noexcept
        {
            const uint64 name_hash = HashText(entry.name);
            if (name_hash != 0)
                return name_hash;

            hgl::hash::FNV1aHasher64 h;
            h << entry.semantic
              << entry.semantic_layer
              << entry.set_type
              << entry.texture_slot
              << entry.material_private_data_slot
              << entry.ssbo_type;
            return h;
        }

        uint64 GetResourceSchemaID(
            const SerializedDescriptorEntry &entry) noexcept
        {
            if (entry.struct_name && entry.struct_name[0])
                return HashText(entry.struct_name);
            if (entry.glsl_type && entry.glsl_type[0])
                return HashText(entry.glsl_type);

            hgl::hash::FNV1aHasher64 h;
            h << entry.ssbo_type;
            return h;
        }

        bool AppendEntry(
            SerializedDescriptorEntry &source,
            DescriptorContract &out_contract)
        {
            // C1-T2：就地完整规范化——ID/ssbo_type 语义推导/layer 默认/policy 默认
            // 全部写入 source；DescriptorContract.entries 直接存规范化条目
            //（原 DescriptorContractEntry 包装已删）。
            source.logical_resource_id =
                GetLogicalResourceID(source);
            source.resource_schema_id =
                GetResourceSchemaID(source);

            if (source.semantic_layer
                    == DescriptorSemanticLayer::Unknown)
            {
                source.semantic_layer =
                    GetDescriptorSemanticLayer(source.semantic);
            }

            if (source.semantic
                    == DescriptorSemantic::MaterialPrivateData
             && source.ssbo_type == SSBOType::UserDefined)
                source.ssbo_type = SSBOType::PBRSurface;
            else if (source.semantic
                    == DescriptorSemantic::MaterialTextureLayerTable)
                source.ssbo_type = SSBOType::TextureLayer;
            else if (source.semantic
                    == DescriptorSemantic::MaterialPrivateDataIndex)
                source.ssbo_type =
                    SSBOType::MaterialPrivateDataIndex;
            else if (source.semantic
                    == DescriptorSemantic::LocalToWorldIndex)
                source.ssbo_type = SSBOType::LocalToWorldIndex;

            if (!source.has_requirement_policy)
            {
                source.required =
                    IsSemanticRequired(source.semantic);
                source.allow_fallback =
                    IsSemanticFallbackAllowed(source.semantic);
            }

            if (source.logical_resource_id == 0
             || source.resource_schema_id == 0)
                return false;

            out_contract.push_back(source);
            return true;
        }
    }

    bool BuildDescriptorContract(
        const SerializedDescriptorEntry *entries,
        const uint32 entry_count,
        DescriptorContract &out_contract)
    {
        out_contract = {};
        if (entry_count > 0 && !entries)
            return false;

        out_contract.reserve(entry_count);
        for (uint32 i = 0; i < entry_count; ++i)
        {
            // C1：入口 const 数组——拷贝后就地规范化（ID 写入拷贝；
            // T2 后该规范化条目即 entries 元素）
            SerializedDescriptorEntry normalized = entries[i];
            if (!AppendEntry(normalized, out_contract))
                return false;
        }
        return ValidateDescriptorContract(out_contract);
    }

    bool EnsureDescriptorContractVaryingResources(
        const MaterialVertexVaryingConfig &varying,
        DescriptorContract &in_out_contract)
    {
        const auto has_semantic =
            [&in_out_contract](const DescriptorSemantic semantic)
        {
            for (const SerializedDescriptorEntry &entry : in_out_contract)
            {
                if (entry.semantic == semantic)
                    return true;
            }
            return false;
        };

        std::vector<SerializedDescriptorEntry> generated;
        if (varying.emit_data_index_id
         && !has_semantic(DescriptorSemantic::MaterialPrivateDataIndex))
        {
            SerializedDescriptorEntry entry{};
            entry.set_type = SBS_MaterialPrivateDataIndexRows.set_type;  // P1-2c：Transform 集
            entry.stage_flags =
                uint32(hgl::graph::kMeshFragment);
            entry.name = SBS_MaterialPrivateDataIndexRows.name;
            entry.struct_name = SBS_MaterialPrivateDataIndexRows.struct_name;
            entry.semantic =
                DescriptorSemantic::MaterialPrivateDataIndex;
            entry.semantic_layer = DescriptorSemanticLayer::SSBO;
            entry.ssbo_type = SSBOType::MaterialPrivateDataIndex;
            entry.has_requirement_policy = true;
            entry.required = true;
            generated.push_back(entry);
        }

        for (SerializedDescriptorEntry &entry : generated)
        {
            if (!AppendEntry(entry, in_out_contract))
                return false;
        }
        return ValidateDescriptorContract(in_out_contract);
    }

    bool BuildDescriptorContract(
        const std::vector<SerializedDescriptorEntry> &entries,
        DescriptorContract &out_contract)
    {
        return BuildDescriptorContract(
            entries.data(),
            static_cast<uint32>(entries.size()),
            out_contract);
    }

    bool BuildEffectiveDescriptorContract(
        const DescriptorContract &base_contract,
        const std::vector<MaterialPrivateDataSlotDeclaration> *material_private_data_slot_decls,
        const uint32 material_ssbo_stage_bits,
        DescriptorContract &out_contract)
    {
        out_contract = base_contract;
        if (!material_private_data_slot_decls || material_private_data_slot_decls->empty())
            return ValidateDescriptorContract(out_contract);

        out_contract.erase(
            std::remove_if(
                out_contract.begin(),
                out_contract.end(),
                [](const SerializedDescriptorEntry &entry)
                {
                    return entry.semantic
                        == DescriptorSemantic::MaterialPrivateData;
                }),
            out_contract.end());

        for (uint32 i = 0;
             i < static_cast<uint32>(material_private_data_slot_decls->size());
             ++i)
        {
            const MaterialPrivateDataSlotDeclaration &decl = (*material_private_data_slot_decls)[i];
            SerializedDescriptorEntry fixed{};
            fixed.set_type = DescriptorSetType::Material;
            fixed.stage_flags = material_ssbo_stage_bits;
            fixed.name = decl.name.c_str();
            fixed.struct_name =
                ssbo::GetMaterialSSBOStructName(decl.ssbo_type);
            fixed.semantic =
                DescriptorSemantic::MaterialPrivateData;
            fixed.texture_slot = TextureSlot::BaseColor;
            fixed.material_private_data_slot = i;
            fixed.ssbo_type = decl.ssbo_type;
            fixed.semantic_layer = DescriptorSemanticLayer::SSBO;
            fixed.ssbo_id = MakeRecipeSSBOId(i);
            fixed.has_requirement_policy = true;
            fixed.required = true;
            fixed.allow_fallback = false;
            if (!AppendEntry(fixed, out_contract))
                return false;
        }

        return ValidateDescriptorContract(out_contract);
    }

    bool ValidateDescriptorContract(
        const DescriptorContract &contract) noexcept
    {
        // 直接在规范化条目上校验（原"转 ShaderInterfaceContract 再校验"的
        // 往返已删）。规则与原 ValidateShaderInterfaceContract 的 descriptor
        // 段 + 名字检查逐条一致。
        //
        // 校验分层：本函数 = entry 级结构守卫 + 身份碰撞快速失败（构建步
        // 早期短路，bool 无诊断；W1 用例锚定重复身份必须在此拒绝）。
        // ValidateShaderResourceSchema = 结构权威——两两对比在 schema 层
        // 以三键（name/logical_id/semantic）判定 duplicate vs conflict，
        // 输出可操作的诊断信息。
        const size_t count = contract.size();
        for (size_t i = 0; i < count; ++i)
        {
            const SerializedDescriptorEntry &entry = contract[i];
            if (!entry.name || !entry.name[0]
             || entry.logical_resource_id == 0
             || entry.semantic == DescriptorSemantic::Unknown
             || entry.semantic_layer == DescriptorSemanticLayer::Unknown
             || entry.semantic_layer > DescriptorSemanticLayer::Sampler
             || entry.set_type == DescriptorSetType::Unknown
             || entry.set_type < DescriptorSetType::Scene
             || entry.set_type > DescriptorSetType::Vertex   // Phase 5：Vertex 为最后一个集合类型
             || entry.texture_slot < TextureSlot::BEGIN_RANGE
             || entry.texture_slot > TextureSlot::END_RANGE
             || entry.ssbo_type < SSBOType::BEGIN_RANGE
             || entry.ssbo_type > SSBOType::END_RANGE
             || entry.array_count == 0
             || entry.stage_flags == 0)
                return false;

            for (size_t j = 0; j < i; ++j)
            {
                if (entry.logical_resource_id
                    == contract[j].logical_resource_id)
                    return false;
            }
        }
        return true;
    }

    bool BuildResourceSchemaFromContract(
        const DescriptorContract &contract,
        ShaderResourceSchema &out_schema)
    {
        out_schema = {};
        if (!ValidateDescriptorContract(contract))
            return false;

        out_schema.resources.reserve(contract.size());

        // C1-T2：entries 为规范化 SerializedDescriptorEntry——直读字段
        //（原经 DescriptorContractEntry.canonical 间接访问已删除）。
        for (const SerializedDescriptorEntry &entry : contract)
        {
            ShaderResourceSlot req;

            // ── Identity（AppendEntry 已就地计算）──
            req.logical_resource_id = entry.logical_resource_id;
            req.resource_schema_id = entry.resource_schema_id;
            req.semantic = entry.semantic;
            req.semantic_layer = entry.semantic_layer;
            req.set_type = entry.set_type;
            req.texture_slot = entry.texture_slot;
            req.material_private_data_slot = entry.material_private_data_slot;
            req.ssbo_type = entry.ssbo_type;
            req.ssbo_id = entry.ssbo_id;
            req.stage_flags = entry.stage_flags;
            req.required = entry.required;
            req.allow_fallback = entry.allow_fallback;

            // ── Names ──
            req.name = entry.name ? entry.name : "";
            req.struct_name = entry.struct_name ? entry.struct_name : "";
            req.glsl_type = entry.glsl_type ? entry.glsl_type : "";

            // ── Fallback names by semantic ──
            if (req.name.empty())
            {
                const char *default_name = GetDefaultDescriptorNameBySemantic(
                    req.semantic);
                if (default_name)
                    req.name = default_name;
            }
            if (req.struct_name.empty())
            {
                const char *default_struct = GetDefaultStructNameBySemantic(
                    req.semantic);
                if (default_struct)
                    req.struct_name = default_struct;
            }

            // ── SSBO id corrections (matching BuildShaderResourceSchema) ──
            if (req.semantic == DescriptorSemantic::MaterialPrivateData
             && req.ssbo_id == MakeRecipeSSBOId(0))
            {
                req.ssbo_id = MakeRecipeSSBOId(req.material_private_data_slot);
            }
            if (req.semantic
                    == DescriptorSemantic::MaterialTextureLayerTable
             && req.ssbo_id == MakeRecipeSSBOId(0))
            {
                req.ssbo_id = MakeRecipeSSBOId(
                    static_cast<uint32>(req.texture_slot));
            }
            if (req.semantic
                    == DescriptorSemantic::MaterialPrivateDataIndex
             && req.ssbo_id == MakeRecipeSSBOId(0))
            {
                req.ssbo_id = MakeRecipeSSBOId(req.material_private_data_slot);
            }

            out_schema.resources.push_back(std::move(req));
        }
        return true;
    }

    uint64 GetDescriptorContractHash(
        const DescriptorContract &contract,
        const uint64 module_manifest_hash) noexcept
    {
        if (!ValidateDescriptorContract(contract))
            return 0;

        // 序列化恒等：按 logical_resource_id 排序后逐字段哈希（排序键与
        // 字段集沿用原 ShaderInterfaceContract 序列化——保证输入顺序无关；
        // 哈希值与旧值不同属预期，缓存键一次性整体更替）。
        std::vector<const SerializedDescriptorEntry *> ordered;
        ordered.reserve(contract.size());
        for (const SerializedDescriptorEntry &entry : contract)
            ordered.push_back(&entry);
        std::sort(
            ordered.begin(),
            ordered.end(),
            [](const SerializedDescriptorEntry *lhs,
               const SerializedDescriptorEntry *rhs)
            {
                return lhs->logical_resource_id
                    < rhs->logical_resource_id;
            });

        hgl::hash::FNV1aHasher64 entries_hasher;
        entries_hasher << static_cast<uint32>(ordered.size());
        for (const SerializedDescriptorEntry *entry : ordered)
        {
            entries_hasher << entry->logical_resource_id
                           << entry->resource_schema_id
                           << entry->semantic
                           << entry->semantic_layer
                           << entry->set_type
                           << entry->texture_slot
                           << entry->ssbo_type
                           << entry->material_private_data_slot
                           << entry->stage_flags
                           << entry->array_count
                           << entry->required
                           << entry->allow_fallback;
        }

        hgl::hash::FNV1aHasher64 h;
        h << module_manifest_hash
          << static_cast<uint64>(entries_hasher);
        return h;
    }
}
