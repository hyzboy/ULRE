#include<hgl/vk/VKShaderDescriptorSet.h>
#include<hgl/log/Log.h>
#include<algorithm>
#include<cstring>
#include<unordered_set>
#include<vector>

namespace hgl::graph{
namespace
{
    bool TryGetFixedBinding(DescriptorSetType set_type, const char *name, int &out_binding)
    {
        if (!name || !*name)
            return false;

        switch (set_type)
        {
        // 注：Scene 集（camera/sky/viewport UBO）已全局化（P1），不再进入 per-material 分配器，
        //     故此处不再需要 Scene 固定绑定映射。
        case DescriptorSetType::PerObject:
            // P1-2c：PerObject 集 binding 由固定常量表 kPerObjectBinding* 确定
            //（l2w / l2w_index_rows / joint / mtl_data_index_rows）。
            if (strcmp(name, "l2w") == 0)              { out_binding = kPerObjectBindingL2W;           return true; }
            if (strcmp(name, "l2w_index_rows") == 0)   { out_binding = kPerObjectBindingL2WIndexRows;  return true; }
            if (strcmp(name, "joint") == 0)            { out_binding = kPerObjectBindingJoint;         return true; }
            if (strcmp(name, "mtl_data_index_rows") == 0) { out_binding = kPerObjectBindingDataIndexRows; return true; }
            // 顶点数据 SSBO（MeshShader 方向）
            if (strcmp(name, "VertexPosition") == 0)   { out_binding = kPerObjectBindingVertexPosition; return true; }
            if (strcmp(name, "VertexUV") == 0)         { out_binding = kPerObjectBindingVertexUV;       return true; }
            if (strcmp(name, "VertexNTB") == 0)        { out_binding = kPerObjectBindingVertexNTB;      return true; }
            if (strcmp(name, "VertexJoint") == 0)      { out_binding = kPerObjectBindingVertexJoint;    return true; }
            if (strcmp(name, "VertexIndex") == 0)      { out_binding = kPerObjectBindingVertexIndex;    return true; }
            if (strcmp(name, "VertexColor") == 0)      { out_binding = kPerObjectBindingVertexColor;   return true; }
            if (strcmp(name, "VertexLuminance") == 0)  { out_binding = kPerObjectBindingVertexLuminance; return true; }
            return false;

        case DescriptorSetType::Material:
            return false;

        default:
            return false;
        }
    }

    int DynamicBindingStart(DescriptorSetType set_type)
    {
        switch (set_type)
        {
        // 注：Scene 已全局化（P1），不再进入分配器，无动态绑定起点需求。
        // P1-2c：PerObject 集固定成员占 binding 0..7（kPerObjectBinding*），动态从 8 起。
        case DescriptorSetType::PerObject: return 8;
        case DescriptorSetType::Material:  return 0;
        default:                           return 0;
        }
    }

    void ReassignBindingsForSet(ShaderDescriptorSet &set_desc)
    {
        std::vector<ShaderDescriptor *> values;
        set_desc.descriptor_map.GetValueArray(values);

        // P1-2b：Material 集的 SSBO binding 由 ShaderBuildContext 显式传入的
        // preferred_binding（= ssbos 列表下标）确定，不再按名字字母排序分配。
        // 其余集（Transform 等）仍保留按名排序 + 固定名 + 动态分配的旧路径。
        const bool index_driven = (set_desc.set_type == DescriptorSetType::Material);

        if (!index_driven)
        {
            std::sort(values.begin(), values.end(), [](const ShaderDescriptor *a, const ShaderDescriptor *b)
            {
                const char *lhs = (a && a->name) ? a->name : "";
                const char *rhs = (b && b->name) ? b->name : "";
                return std::strcmp(lhs, rhs) < 0;
            });
        }

        std::unordered_set<int> used_bindings;
        for (auto *sd : values)
        {
            if (!sd)
                continue;

            sd->set_type = set_desc.set_type;
            sd->set = int(set_desc.set_type);
            sd->binding = -1;
        }

        // 1) Explicit preferred binding from compiler contract (highest priority).
        for (auto *sd : values)
        {
            if (!sd || sd->preferred_binding < 0)
                continue;

            if (used_bindings.find(sd->preferred_binding) != used_bindings.end())
            {
                GLogError(u8"[ShaderDescriptorSet] duplicate preferred binding %d (set=%d, name='%s')",
                          sd->preferred_binding, int(set_desc.set_type), sd->name);
                continue;
            }

            sd->binding = sd->preferred_binding;
            used_bindings.insert(sd->preferred_binding);
        }

        // 2) Legacy fixed binding by well-known descriptor names.
        for (auto *sd : values)
        {
            if (!sd || sd->binding >= 0)
                continue;

            int fixed_binding = -1;
            if (TryGetFixedBinding(set_desc.set_type, sd->name, fixed_binding))
            {
                sd->binding = fixed_binding;
                used_bindings.insert(fixed_binding);
            }
        }

        // 3) Dynamic binding for anything still unassigned.
        int candidate = DynamicBindingStart(set_desc.set_type);
        for (auto *sd : values)
        {
            if (!sd || sd->binding >= 0)
                continue;

            if (index_driven)
            {
                GLogError(u8"[ShaderDescriptorSet] material SSBO '%s' has no preferred binding index",
                          sd->name);
            }

            while (used_bindings.find(candidate) != used_bindings.end())
                ++candidate;

            sd->binding = candidate;
            used_bindings.insert(candidate);
            ++candidate;
        }
    }
}

/**
* 添加一个描述符
*/
ShaderDescriptor *ShaderDescriptorSet::AddDescriptor(uint32_t shader_stage_flag_bits,ShaderDescriptor *new_sd)
{
    ShaderDescriptor *sd;

    if(descriptor_map.Get(new_sd->name,sd))
    {
        delete new_sd;
        sd->stage_flag|=shader_stage_flag_bits;
        return(sd);
    }
    else
    {
        new_sd->set_type=set_type;
        new_sd->set=int(set_type);
        new_sd->stage_flag=shader_stage_flag_bits;

        descriptor_map.Add(new_sd->name,new_sd);
        ReassignBindingsForSet(*this);

        count++;

        return(new_sd);
    }
}
}//namespace hgl::graph
