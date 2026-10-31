#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/module/ShaderProgramLinkCache.h>
#include<hgl/graph/module/ShaderStageModuleCache.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/ObjectManager.h>

namespace hgl::graph{

class GeometryVertexFormat;
class ComputePipeline;

namespace mtl
{
    struct MaterialDefinitionBuildRequest;
    struct MaterialDefinition;
    struct PassProfileRequest;
}//namespace mtl

namespace mtl
{
    class ShaderCreateInfo;
    class ShaderCreateInfoMap;
    class ShaderBuildContext;
}//namespace mtl

using ShaderProgramID = int;

// SceneGraph policy for the existing material provider shapes. Callers that
// need a route selected from material data use this before acquiring a program.
bool SelectCurrentScenePassProfileRequest(
    const mtl::MaterialDefinition &definition,
    const mtl::MaterialDefinitionBuildRequest &request,
    mtl::PassProfileRequest &out_request);

GRAPH_MODULE_CLASS(ShaderProgramManager)
{
private:

    ShaderStageModuleCache shader_module_cache;
    ShaderProgramLinkCache shader_program_cache;

    /// L1：变体键 → program link 键。
    ///
    /// 变体键（`mtl::ComputeMaterialProgramManifestKey`：材质定义 + 已解析构建请求
    /// + 设备 profile 的纯内存哈希）**完整决定** program link —— 发行形态本来就
    /// 靠这条不变量查表取 SPV，这里只是同一不变量在内存里的复用。
    ///
    /// 存键而不是存指针：program 销毁走 `Destroy()`，那样会留下悬垂指针；存键则
    /// 由 `shader_program_cache` 的删除自然失效（查不到即当 miss）。
    UnorderedMap<uint64, mtl::ShaderProgramKey> variant_program_cache;

    AutoIdObjectManager<ShaderProgramID, ShaderProgram> rm_material;  ///<材质合集

private:

    VkDescriptorSetLayout bindless_layout_ = VK_NULL_HANDLE;   ///< 全局 Bindless Texture Set 布局（Set 1）
    VkPipelineLayout shared_pipeline_layout_ = VK_NULL_HANDLE;  ///<全材质共享 pipeline layout 单例（惰建，由本模块拥有，Release() 时销毁）

    ShaderProgramManager(GraphicsContext *);
    ~ShaderProgramManager()=default;

    friend class GraphModuleManager;

private: // Helper methods with integrated DebugUtils

    ShaderProgram *AcquireShaderProgram(const mtl::ShaderProgramKey &, const mtl::ShaderBuildContext *);
    ShaderProgram *TryGetCachedShaderProgram(
        const mtl::ShaderProgramKey &key);
    /// L1 变体键命中：变体键 → program link 键 → 内存缓存里的 ShaderProgram。
    ShaderProgram *TryGetCachedByVariantKey(const uint64 variant_key) const;
    /// 登记变体键 → program link 键（幂等；同键重复登记直接覆盖）。
    void RecordVariantProgramKey(const uint64 variant_key,
                                 const mtl::ShaderProgramKey &program_key);
    VkPipelineLayout GetOrCreateGlobalPipelineLayout();
    /// 装配 ShaderProgram：ShaderModule + stage 列表 + 共享 pipeline layout。
    /// **不创建 VkPipeline**（那是渲染侧 RenderPass::CreatePipeline 的职责）。
    bool BuildRuntimeShaderProgram(ShaderProgram *mtl,
                                   const AnsiString &mtl_name,
                                   const mtl::ShaderBuildContext *ctx,
                                   const mtl::ShaderCreateInfoMap &sci_map);

public: //Add

    ShaderProgramID Add(ShaderProgram *mtl) { return rm_material.Add(mtl); }

    /** 设置全局 Bindless Texture Set 布局（必须在创建任何材质前调用）*/
    void SetBindlessLayout(VkDescriptorSetLayout layout) { bindless_layout_ = layout; }

    /** 设置全局 Scene UBO Set 布局（必须在创建任何材质前调用，P1）*/

public: //Get

    ShaderProgram *GetShaderProgram(const ShaderProgramID &id) { return rm_material.Get(id); }

public: //Release

    void Release(ShaderProgram *mtl) { rm_material.Release(mtl); }

    void Destroy(ShaderProgram *mtl)
    {
        if (!mtl)
            return;

        shader_program_cache.Remove(mtl->GetProgramKey());

        rm_material.Release(mtl, true);
    }

public: // Override Release from GraphModule - cleanup all resources

    void Release() override;

public: //Shader

    const ShaderModule *CreateShaderModule(const AnsiString &shader_module_name, const mtl::ShaderCreateInfo *);
    const ShaderModule *CreateShaderModule(const mtl::ShaderStageKey &, const mtl::ShaderCreateInfo *);
    const ShaderModule *CreateShaderModuleFromSPV(const AnsiString &shader_module_name,
                                                  const VkShaderStageFlagBits stage,
                                                  const uint32_t *spv_data,
                                                  const size_t spv_size);
    const ShaderModule *CreateShaderModuleFromSPV(const mtl::ShaderStageKey &,
                                                  const uint32_t *spv_data,
                                                  const size_t spv_size);

public: //Compute

    /** 编译 compute GLSL 源码并创建 ShaderModule（按 name 缓存，与 graphics 模块同一缓存）。
     *  push_constant_size 是 CPU 侧预期布局大小；开发形态从 SPV 反射实际范围并要求二者一致，
     *  再把反射值登记进缓存清单。它不参与 ShaderModule 创建。生成器不在编译
     *  （ULRE_ENABLE_SHADER_PROGRAM_BUILD=0）时硬失败。 */
    const ShaderModule *CreateComputeShaderModule(const AnsiString &shader_module_name,
                                                  const AnsiString &glsl_source,
                                                  const uint32_t push_constant_size = 0);

    /**
     * 直接源码创建 compute 管线：GLSL 源码 → 编译 → ShaderModule → ComputePipeline
     * 专用 layout：Set0=全局 Scene 集 / Set1=全局 Bindless 集（与图形管线同构，都带 DB 位）
     * 用户数据一律走 BDA：push constant 里下发 buffer 设备地址，shader 侧用
     *   layout(buffer_reference, ...) 解引用——不再有第三集用户描述符集
     *  push constant：COMPUTE stage、offset 0、大小由 push_constant_size 指定（≤128B，spec 保证值）
     * 返回的 ComputePipeline 由调用方 delete（析构时连带销毁专用 layout，不影响共享全局 layout）
     *
     * 仅在生成器在编时有意义：它现编 GLSL，并把结果顺手烘焙进 shader-cache，
     * 使同一份工程代码在开发形态跑一次就留下发行形态要用的 compute 产物。
     */
    ComputePipeline *CreateComputePipeline(const AnsiString &name,
                                           const AnsiString &glsl_source,
                                           const uint32_t push_constant_size = 0);

    /**
     * 预烘焙 SPV 创建 compute 管线（发行形态主路径）：
     *   shader-cache/compute_manifest.pmf 按 name 查表 → 读 stage SPV → ShaderModule
     *   → ComputePipeline。
     *
     * 未命中一律返回 nullptr，并在日志里说清是「清单缺项」还是「SPV 缺失/损坏」。
     * 清单记录的 push_constant_size 与本参数不一致时同样拒绝 —— 那代表 layout 与
     * 烘焙侧约定脱节，继续建管线只会得到运行期越界访问。
     */
    ComputePipeline *CreateComputePipelineFromCache(const AnsiString &name,
                                                    const uint32_t push_constant_size = 0);

private:
    /** 拿 ShaderModule 之后的公共尾段：layout → VkPipeline。两条 compute 路径（现编/查表）
     *  只在「怎么拿到 ShaderModule」上不同，其余必须完全一致 —— 否则两条路建出来的
     *  管线布局会分叉。 */
    ComputePipeline *BuildComputePipeline(const AnsiString &name,
                                          const ShaderModule *sm,
                                          const uint32_t push_constant_size);

public: //ShaderProgram

    ShaderProgram *AcquireShaderProgram(
        const mtl::MaterialDefinitionBuildRequest &request);

};//class ShaderProgramManager

}//namespace hgl::graph
