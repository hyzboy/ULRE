/// ResourceLayoutGenerator.cpp — GLSL 资源布局声明生成器实现

#include<hgl/shadergen/ResourceLayoutGenerator.h>
#include<hgl/mtl/FixedDescriptorEntry.h>
#include<hgl/mtl/FixedVertexEntry.h>
#include <hgl/graph/shared/VertexAttribDef.h>
#include <hgl/graph/shared/DescriptorSetTypeDef.h>
#include<stdarg.h>
#include<stdio.h>
#include<assert.h>
#include<map>
#include <string>

namespace hgl::graph::mtl {

namespace {
    // VAType 转 GLSL 类型字符串
    const char* VAType_To_GLSL(const VAType& va_type) {
        // VertexAttribBaseType: Bool=0, Int, UInt, Float, Double
        // vec_size: 1, 2, 3, 4
        
        const uint8_t bt = static_cast<uint8_t>(va_type.basetype);
        const uint8_t vs = va_type.vec_size;
        
        if (vs < 1 || vs > 4) {
            fprintf(stderr, "ERROR: Invalid VAType vec_size: %u\n", vs);
            return "vec4";  // fallback
        }
        
        // 根据不同的基础类型生成 GLSL 类型
        switch (va_type.basetype) {
            case VertexAttribBaseType::Bool:
                if (vs == 1) return "bool";
                if (vs == 2) return "bvec2";
                if (vs == 3) return "bvec3";
                if (vs == 4) return "bvec4";
                break;
            
            case VertexAttribBaseType::Int:
                if (vs == 1) return "int";
                if (vs == 2) return "ivec2";
                if (vs == 3) return "ivec3";
                if (vs == 4) return "ivec4";
                break;
            
            case VertexAttribBaseType::UInt:
                if (vs == 1) return "uint";
                if (vs == 2) return "uvec2";
                if (vs == 3) return "uvec3";
                if (vs == 4) return "uvec4";
                break;
            
            case VertexAttribBaseType::Float:
                if (vs == 1) return "float";
                if (vs == 2) return "vec2";
                if (vs == 3) return "vec3";
                if (vs == 4) return "vec4";
                break;
            
            case VertexAttribBaseType::Double:
                if (vs == 1) return "double";
                if (vs == 2) return "dvec2";
                if (vs == 3) return "dvec3";
                if (vs == 4) return "dvec4";
                break;
            
            default:
                fprintf(stderr, "ERROR: Unknown VertexAttribBaseType: %u\n", bt);
                return "vec4";
        }
        
        return "vec4";  // fallback
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
        const char* glsl_type = VAType_To_GLSL(entry.type);
        
        // 生成 layout(location=N) in TYPE name;
        char buf[256];
        snprintf(buf, sizeof(buf),
            "layout(location=%u) in %s %s;\n",
            i, glsl_type, entry.name);
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
