#pragma once

#include <hgl/shadergen/ShaderRequirement.h>
#include <hgl/mtl/MaterialResourceManifest.h>
#include <map>
#include <vector>
#include <string>
#include <string_view>

namespace hgl::graph
{
    // ─────────────────────────────────────────────────────────────────────────
    // ShaderRequirementSet
    //
    // 收集一次 shader 组装中所有片段声明的 @sfm:require 依赖，负责：
    //   1. 去重（同一语义只保留首次声明）
    //   2. 按 set 内插入顺序自动分配 binding 号
    //   3. 生成前导 include 链 GLSL 代码
    //   4. 导出给 Vulkan pipeline layout builder 的 VkDescriptorSetLayoutBinding 列表
    // ─────────────────────────────────────────────────────────────────────────
    class ShaderRequirementSet
    {
    public:
        // ── 添加接口 ─────────────────────────────────────────────────────────

        /// 手动添加一条需求（来自 row.resources 快捷路径或 C++ 代码显式声明）
        void Add(const ShaderRequirement &req);

        /// 从 GLSL 源码文本中解析顶部 @sfm:require 注解，合并到本集合
        /// 只解析顶部连续的 // 注释行，遇第一个非注释/非空行停止
        void ParseFromGLSLSource(std::string_view glsl_source);

        /// 从 ShaderLibrary 相对路径读取文件，解析 @sfm:require 注解（使用全局 library path）
        void ParseFromGLSLFile(std::string_view rel_path);

        /// 从 ShaderLibrary 相对路径读取文件，解析 @sfm:require 注解（使用指定 library path）
        void ParseFromGLSLFile(std::string_view rel_path, const std::string &library_path);

        // ── 查询接口 ─────────────────────────────────────────────────────────

        /// 是否已包含指定语义名的需求
        bool Requires(std::string_view sem_name) const;

        /// 指定 set_type 下的需求列表（按 binding 分配顺序，即插入顺序）
        const std::vector<ShaderRequirement> &GetRequirements(DescriptorSetType set_type) const;

        bool Empty() const noexcept { return total_count_ == 0; }

        // ── 输出接口 ─────────────────────────────────────────────────────────

        /// 生成所有依赖的前导 include 链（以 #include "..." 形式逐行输出）
        /// 供 CompositorAssembler 在 shader 顶部 emit
        std::string EmitIncludes() const;

        /// 导出 VkDescriptorSetLayoutBinding 数组（供 pipeline layout builder 使用）
        /// binding 号 = 在对应 set_type 桶内的插入顺序下标（0, 1, 2, ...）
        std::vector<VkDescriptorSetLayoutBinding> GetVkBindings(DescriptorSetType set_type) const;

        /// 转换为 MaterialResourceManifest（UBO/SSBO 语义集合）
        /// 调用方可再用 MergeKeepFirst / MergeOverwrite 与 def 已有声明合并
        mtl::MaterialResourceManifest ToManifest() const;

        /// 将另一个 ShaderRequirementSet 的全部需求合并进本集合（自动去重）
        void MergeFrom(const ShaderRequirementSet &other);

    private:
        // 按 set_type 分桶，桶内保持插入顺序
        std::map<DescriptorSetType, std::vector<ShaderRequirement>> buckets_;

        // 全局去重 key = "UBO:camera" / "SSBO:transform_id"
        std::vector<std::string> seen_keys_;

        size_t total_count_ = 0;

        static std::string MakeKey(mtl::DescriptorKind kind, std::string_view sem_name);

        // 从注册表查找语义名对应的 ShaderRequirement（含 set_type / glsl_include）
        static bool LookupSemantic(mtl::DescriptorKind kind,
                                   std::string_view sem_name,
                                   ShaderRequirement &out);

        static const std::vector<ShaderRequirement> s_empty_bucket_;
    };

} // namespace hgl::graph
