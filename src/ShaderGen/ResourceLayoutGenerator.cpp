/// ResourceLayoutGenerator.cpp — GLSL 资源布局声明生成器实现

#include<hgl/shadergen/ResourceLayoutGenerator.h>
#include<hgl/mtl/FixedDescriptorEntry.h>
#include<hgl/mtl/FixedVertexEntry.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/common/DescriptorSetTypeDef.h>
#include<stdarg.h>
#include<stdio.h>
#include<assert.h>
#include<map>
#include <string>

namespace hgl::graph::mtl {

namespace {
    // VkFormat 转 GLSL 类型字符串
    // 仅覆盖顶点输入常用格式；未知格式退化为 "vec4"。
    const char* VkFormatToGLSLType(VkFormat fmt)
    {
        switch (fmt)
        {
        // float scalars / vectors
        case VK_FORMAT_R32_SFLOAT:              return "float";
        case VK_FORMAT_R32G32_SFLOAT:           return "vec2";
        case VK_FORMAT_R32G32B32_SFLOAT:        return "vec3";
        case VK_FORMAT_R32G32B32A32_SFLOAT:     return "vec4";
        // double scalars / vectors
        case VK_FORMAT_R64_SFLOAT:              return "double";
        case VK_FORMAT_R64G64_SFLOAT:           return "dvec2";
        case VK_FORMAT_R64G64B64_SFLOAT:        return "dvec3";
        case VK_FORMAT_R64G64B64A64_SFLOAT:     return "dvec4";
        // signed int scalars / vectors
        case VK_FORMAT_R32_SINT:                return "int";
        case VK_FORMAT_R32G32_SINT:             return "ivec2";
        case VK_FORMAT_R32G32B32_SINT:          return "ivec3";
        case VK_FORMAT_R32G32B32A32_SINT:       return "ivec4";
        // unsigned int scalars / vectors
        case VK_FORMAT_R32_UINT:                return "uint";
        case VK_FORMAT_R32G32_UINT:             return "uvec2";
        case VK_FORMAT_R32G32B32_UINT:          return "uvec3";
        case VK_FORMAT_R32G32B32A32_UINT:       return "uvec4";
        // bool scalars / vectors (packed as uint, interpreted as bool in GLSL)
        case VK_FORMAT_R8_UINT:                 return "bool";
        case VK_FORMAT_R8G8_UINT:               return "bvec2";
        case VK_FORMAT_R8G8B8_UINT:             return "bvec3";
        case VK_FORMAT_R8G8B8A8_UINT:           return "bvec4";
        default:
            fprintf(stderr, "VkFormatToGLSLType: unhandled format %d, falling back to vec4\n",
                    static_cast<int>(fmt));
            return "vec4";
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 重复绑定检测
// ─────────────────────────────────────────────────────────────────────────────

void ResourceLayoutGenerator::CheckAndMarkBinding(uint32_t set, uint32_t binding, const char* name)
{
    if (set >= 8 || binding >= 32) {
        fprintf(stderr, "ERROR: Invalid descriptor binding: set=%u, binding=%u\n", set, binding);
        return;
    }
    
    if (used_bindings.IsUsed(set, binding)) {
        // ❌ 检测到重复绑定！
        fprintf(stderr, "\n");
        fprintf(stderr, "❌❌❌ DUPLICATE DESCRIPTOR BINDING DETECTED! ❌❌❌\n");
        fprintf(stderr, "   Set=%u, Binding=%u\n", set, binding);
        fprintf(stderr, "   Resource name: %s\n", name);
        fprintf(stderr, "   This is the ROOT CAUSE of shader compilation errors!\n");
        fprintf(stderr, "\n");
        
        // 在开发阶段，崩溃以便立即发现问题
        #ifdef _DEBUG
            assert(false && "Duplicate descriptor binding!");
        #endif
        return;
    }
    
    used_bindings.MarkUsed(set, binding);
}

// ─────────────────────────────────────────────────────────────────────────────
// DescriptorKind 转 GLSL 关键字
// ─────────────────────────────────────────────────────────────────────────────

const char* ResourceLayoutGenerator::GetGLSLQualifier(uint8_t kind) const
{
    switch ((DescriptorKind)kind) {
        case DescriptorKind::UBO:               return "uniform";
        case DescriptorKind::SSBO:              return "buffer";
        case DescriptorKind::Texture:           return "";  // texture 不需要 qualifier
        case DescriptorKind::TextureSampler:    return "";
        default:                                return "uniform";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// VAType 转 GLSL 类型
// ─────────────────────────────────────────────────────────────────────────────

const char* ResourceLayoutGenerator::GetGLSLType(uint8_t va_type) const
{
    // 这里只是一个简化的映射，完整实现需要查表
    // 实际应该调用 VAType_To_GLSL
    return "vec4";  // placeholder
}

// ─────────────────────────────────────────────────────────────────────────────
// 生成描述符布局声明
// ─────────────────────────────────────────────────────────────────────────────

std::string ResourceLayoutGenerator::GenDescriptorLayout(const FixedDescriptorEntry* entries, uint32_t count)
{
    if (!entries || count == 0) {
        return std::string();
    }
    
    std::string result;
    
    // 按 set 分组，在每个 set 内部自动分配 binding
    // set -> (binding_counter, entries)
    std::map<uint32_t, uint32_t> set_binding_counters;
    
    for (uint32_t i = 0; i < count; i++) {
        const auto& entry = entries[i];
        
        // 获取 set 编号（直接使用 DescriptorSetType 枚举值）
        const uint32_t set = static_cast<uint32_t>(entry.set_type);
        
        // 获取当前 set 的下一个可用 binding 编号
        uint32_t binding = set_binding_counters[set]++;
        
        // 检测重复
        CheckAndMarkBinding(set, binding, entry.name);
        
        // 生成 GLSL 代码
        char buf[1024];
        
        switch (entry.kind) {
            case DescriptorKind::UBO: {
                // layout(set=X, binding=Y) uniform StructName { ... } instance_name;
                snprintf(buf, sizeof(buf),
                    "layout(set=%u, binding=%u) uniform %s {\n"
                    "    // UBO content defined elsewhere\n"
                    "    vec4 _placeholder;\n"
                    "} %s;\n\n",
                    set, binding,
                    entry.struct_name ? entry.struct_name : "UniformBlock",
                    entry.name);
                result += buf;
                break;
            }
            
            case DescriptorKind::SSBO: {
                // layout(set=X, binding=Y, std430) buffer StructName { ... } instance_name;
                snprintf(buf, sizeof(buf),
                    "layout(set=%u, binding=%u, std430) buffer %s {\n"
                    "    // SSBO content defined elsewhere\n"
                    "    vec4 _data[];\n"
                    "} %s;\n\n",
                    set, binding,
                    entry.struct_name ? entry.struct_name : "StorageBlock",
                    entry.name);
                result += buf;
                break;
            }
            
            case DescriptorKind::Texture: {
                // layout(set=X, binding=Y) uniform samplerXX name;
                const char* sampler_type = entry.glsl_type ? entry.glsl_type : "sampler2D";
                snprintf(buf, sizeof(buf),
                    "layout(set=%u, binding=%u) uniform %s %s;\n",
                    set, binding, sampler_type, entry.name);
                result += buf;
                break;
            }
            
            case DescriptorKind::TextureSampler: {
                // 混合采样器（texture + sampler in one binding）
                const char* sampler_type = entry.glsl_type ? entry.glsl_type : "sampler2D";
                snprintf(buf, sizeof(buf),
                    "layout(set=%u, binding=%u) uniform %s %s;\n",
                    set, binding, sampler_type, entry.name);
                result += buf;
                break;
            }
        }
    }
    
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// 生成顶点输入布局声明
// ─────────────────────────────────────────────────────────────────────────────

std::string ResourceLayoutGenerator::GenVertexInputLayout(const FixedVertexEntry* entries, uint32_t count)
{
    if (!entries || count == 0) {
        return std::string();
    }
    
    std::string result;
    
    for (uint32_t i = 0; i < count; i++) {
        const auto& entry = entries[i];
        
        // 获取 GLSL 类型字符串
        const char* glsl_type = VkFormatToGLSLType(entry.format);
        const char* input_name = GetVertexSemanticName(entry.semantic);
        
        // 生成 layout(location=N) in TYPE name;
        char buf[256];
        snprintf(buf, sizeof(buf),
            "layout(location=%u) in %s %s;\n",
            i, glsl_type, input_name);
        result += buf;
    }
    
    result += "\n";
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// 生成片段着色器输出布局
// ─────────────────────────────────────────────────────────────────────────────

std::string ResourceLayoutGenerator::GenFragmentOutputLayout()
{
    // 当前只支持单个颜色输出
    // TODO: 后续扩展为 MRT（多渲染目标）
    return std::string("layout(location=0) out vec4 FragColor;\n\n");
}

}//namespace hgl::graph::mtl
