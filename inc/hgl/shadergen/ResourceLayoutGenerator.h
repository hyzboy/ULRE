#pragma once

/// ResourceLayoutGenerator.h — GLSL 资源布局声明生成器
///
/// **目标**：
///   - 所有 layout(...) 声明从一个地方生成，避免业务代码手写导致的重复声明冲突
///   - 自动检测重复 binding 并报错，防止运行时崩溃
///   - 支持 descriptor set、vertex input、shader output 的布局生成
///
/// **用法**：
///   ```cpp
///   ResourceLayoutGenerator gen;
///   std::string layout_code = gen.GenDescriptorLayout(entries, count);
///   // 生成:
///   // layout(set=0,binding=0) uniform CameraUBO { ... } camera;
///   // layout(set=1,binding=0) buffer MaterialInstanceData { ... } mtl;
///   ```
///
/// **验证**：
///   - 重复 binding 会触发 assert 并输出详细冲突信息
///   - 生成的 GLSL 字符串可直接插入到 shader 源码中
///
/// **当前绑定策略（与实现保持一致）**：
///   - 按 `DescriptorSetType` 分组；每个 set 内从 binding=0 开始递增分配
///   - `FixedDescriptorEntry` 当前不显式携带固定 binding，生成阶段负责稳定顺序分配
///   - 单次生成流程内使用位图检测重复 `(set,binding)` 组合
///   - 调用 `Reset()` 可在下一个 shader 生成前清空状态
///
/// **示例（伪输入 -> 输出）**：
///   输入 entries:
///     [set=Material, kind=SSBO, name=mtl]
///     [set=Material, kind=TextureSampler, name=albedo_tex]
///     [set=Global,   kind=UBO,  name=camera]
///   可能输出:
///     layout(set=<Material>, binding=0, std430) buffer ... mtl;
///     layout(set=<Material>, binding=1) uniform sampler2D albedo_tex;
///     layout(set=<Global>,   binding=0) uniform CameraUBO ... camera;

#include<hgl/type/String.h>
#include<stdint.h>
#include<string.h>
#include <string>

namespace hgl::graph::mtl {

struct FixedDescriptorEntry;
struct FixedVertexEntry;

// ─────────────────────────────────────────────────────────────────────────────
// 资源布局生成器
// ─────────────────────────────────────────────────────────────────────────────
class ResourceLayoutGenerator
{
private:
    // 重复绑定检测（set=0~7, binding=0~31，用位图存储）
    struct BindingMap {
        uint32_t sets[8];  // 每个 set 用一个 uint32 位图表示绑定使用情况
        
        BindingMap() { memset(sets, 0, sizeof(sets)); }
        
        bool IsUsed(uint32_t set, uint32_t binding) const {
            if (set >= 8 || binding >= 32) return false;
            return (sets[set] & (1u << binding)) != 0;
        }
        
        void MarkUsed(uint32_t set, uint32_t binding) {
            if (set < 8 && binding < 32)
                sets[set] |= (1u << binding);
        }
    };
    
    BindingMap used_bindings;

public:
    ResourceLayoutGenerator() = default;

    /// 生成描述符集布局声明
    ///
    /// **输入**：
    ///   - entries: FixedDescriptorEntry 数组（包含 set/binding/type/name）
    ///   - count: 数组长度
    ///
    /// **输出**：
    ///   - GLSL 代码字符串（每个 descriptor 一行 layout(...) 声明）
    ///
    /// **异常**：
    ///   - 检测到重复 binding → assert 失败 + 错误日志
    std::string GenDescriptorLayout(const FixedDescriptorEntry* entries, uint32_t count);

    /// 生成顶点输入布局声明
    ///
    /// **输入**：
    ///   - entries: FixedVertexEntry 数组
    ///   - count: 数组长度
    ///
    /// **输出**：
    ///   - GLSL 代码（layout(location=N) in vec3 vPosition; ...）
    ///
    /// **注意**：
    ///   - location 编号自动分配（0, 1, 2, ...）
    ///   - 名称从 FixedVertexEntry::name 获取
    std::string GenVertexInputLayout(const FixedVertexEntry* entries, uint32_t count);

    /// 生成片段着色器输出布局声明
    ///
    /// **输出**：
    ///   - layout(location=0) out vec4 FragColor;
    ///
    /// **说明**：
    ///   - 当前只支持单个颜色输出，后续可扩展为 MRT（多渲染目标）
    std::string GenFragmentOutputLayout();

    /// 重置状态（多次生成不同 shader 时调用）
    void Reset() {
        memset(used_bindings.sets, 0, sizeof(used_bindings.sets));
    }

private:
    /// 检查并记录 binding 使用情况
    ///
    /// **参数**：
    ///   - set: descriptor set 编号
    ///   - binding: binding 编号
    ///   - name: 资源名称（用于错误信息）
    ///
    /// **行为**：
    ///   - 如果 binding 已被使用 → assert 失败 + 打印冲突信息
    ///   - 否则记录该 binding 已使用
    void CheckAndMarkBinding(uint32_t set, uint32_t binding, const char* name);

    /// 将 DescriptorKind 转换为 GLSL 关键字
    ///
    /// **示例**：
    ///   - UBO → "uniform"
    ///   - SSBO → "buffer"
    const char* GetGLSLQualifier(uint8_t kind) const;

    /// 将 VAType 转换为 GLSL 类型字符串
    ///
    /// **示例**：
    ///   - VAT_VEC3 → "vec3"
    ///   - VAT_VEC4 → "vec4"
    const char* GetGLSLType(uint8_t va_type) const;
};

}//namespace hgl::graph::mtl
