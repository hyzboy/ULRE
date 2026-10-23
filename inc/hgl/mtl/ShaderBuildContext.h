#pragma once

#include<hgl/mtl/ShaderCreateInfoMap.h>
#include<hgl/mtl/ShaderCreateInfo.h>
#include<hgl/mtl/ShaderLinkSpec.h>
#include<hgl/mtl/ShaderArtifactContract.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/common/PrimitiveType.h>
#include<hgl/common/ShaderStageDef.h>
#include <hgl/common/TextureSamplerTypeDef.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include<string>
#include<vector>

namespace hgl::graph::mtl
{
        using hgl::graph::ShaderStage;
        using hgl::graph::DescriptorSetType;

        namespace contract
        {
            struct PhysicalDeviceProfileLite;
        }
        class ShaderArtifactStore;
        class ShaderCreateInfo;
        class ShaderCreateInfoMap;

        class ShaderBuildContext
        {
        protected:

            PrimitiveType primitive_type = PrimitiveType::Triangles;
            uint32_t shader_stage_flag_bits = 0;

            /// 材质是否需要 per-instance 运行时数据行（`mtl_data_addrs` 行表 + 材质数据行）。
            /// 编译期直判信号；消费者 = `RenderPrimitiveCollectSystem` / `PrimitiveBatchPipeline`。
            /// 2026-10-11（S1）：原「资源条目载体」已整体删除（其生产点为 0、消费者或空转或自比），
            /// 此处只留这一个活字段。
            bool requires_recipe_runtime_rows = false;

            /// 程序是否**读 payload 行**（Fragment 里是否发射了 `MTL_ROW`）。
            ///
            /// `requires_recipe_runtime_rows` 是一个合并信号，包含两类互不相交的需求：
            ///   1. payload 行需求（`MTL_ROW` 的 `payload_index`）—— 必须有一行真实存在的
            ///      材质私有数据 SSBO 行，即 recipe 必须给出 `material_ssbo_binding`；
            ///   2. 纹理引用行需求（`MTL_TEX` 的 `texture_reference_index`）—— 只需要
            ///      `MaterialInstanceAddresses` 行表 + 纹理引用池行，行号来自 4-ID 表的
            ///      `texture_id`，**不需要** payload 绑定。
            ///
            /// 纯纹理材质（如 `UnlitTexture`）只命中第 2 类：合并信号为真但 payload 为假。
            /// 运行期校验若只读合并信号，就会把这类材质当成「缺绑定」错杀，因此本字段把
            /// 第 1 类单独记下来（编译期判定，见 MaterialShaderEmitter 的
            /// `MaterialDefinitionRequiresPayloadRow`），运行期不再反推。
            bool requires_recipe_payload_row = false;

            ShaderCreateInfoMap shader_map;                         ///<着色器列表

            ShaderLinkSpec program_link;
            bool has_program_link = false;
            ShaderArtifactStore *artifact_store = nullptr;
            ShaderProgramArtifactMetadata program_metadata{};
            bool has_program_metadata = false;

            /// 两个 stage 是否各自实际采纳了缓存 SPV。
            bool used_cached_mesh_stage = false;
            bool used_cached_fragment_stage = false;

            // ── 结构快照观察字段（ShaderStructureDump 用；不参与 shader 生成语义）──
            // 求解层产出的、但原本只在 GenericMaterialBuilder plan 里存在的状态：
            // 模块列表（manifest 依赖序）与有效 varying 配置。存到 ctx 上让
            // 结构快照/回归门无需持有 plan 也能 dump 完整求解结果。
            std::vector<std::string> resolved_module_names;          ///< 依赖序的 code module 名
            MaterialVertexVaryingConfig effective_varying;           ///< 求解后的 varying 配置
            bool has_effective_varying = false;

        public:

            const PrimitiveType GetPrimitiveType()const{return primitive_type;}

            const   uint32      GetShaderStage  ()const{return shader_stage_flag_bits;}

                    bool        has_shader      (const ShaderStage ss)const{return shader_stage_flag_bits&(uint32)ss;}

                    bool        has_mesh        ()const{return has_shader(ShaderStage::Mesh);}

                    bool        has_fragment    ()const{return has_shader(ShaderStage::Fragment);}

            ShaderCreateInfo *         GetStageShader(const ShaderStage ss){return shader_map[ss];}
            const ShaderCreateInfo *   GetStageShader(const ShaderStage ss)const{return shader_map[ss];}

            const ShaderCreateInfoMap &GetShaderMap()const{return shader_map;}

        public:

            const bool RequiresRecipeRuntimeRows()const{return requires_recipe_runtime_rows;}

            void SetRequiresRecipeRuntimeRows(const bool v){requires_recipe_runtime_rows=v;}

            /// 程序是否读 payload 行（`MTL_ROW`）——决定 recipe 是否必须给出
            /// `material_ssbo_binding`。与合并信号 `RequiresRecipeRuntimeRows()` 的差别
            /// 见同名字段注释。
            const bool RequiresRecipePayloadRow()const{return requires_recipe_payload_row;}

            void SetRequiresRecipePayloadRow(const bool v){requires_recipe_payload_row=v;}

            void SetProgramLink(const ShaderLinkSpec &link)
            {
                program_link = link;
                has_program_link = link.IsValid();
            }

            // 结构快照观察字段填充（求解层在编译收尾时调用；不参与 shader 生成语义）
            void SetResolvedModules(std::vector<std::string> module_names)
            {
                resolved_module_names = std::move(module_names);
            }

            const std::vector<std::string> &GetResolvedModules() const noexcept
            {
                return resolved_module_names;
            }

            void SetEffectiveVarying(const MaterialVertexVaryingConfig &varying)
            {
                effective_varying = varying;
                has_effective_varying = true;
            }

            const MaterialVertexVaryingConfig *GetEffectiveVarying() const noexcept
            {
                return has_effective_varying ? &effective_varying : nullptr;
            }

            bool HasProgramLink() const noexcept { return has_program_link; }
            const ShaderLinkSpec &GetProgramLink() const noexcept { return program_link; }
            void SetArtifactStore(ShaderArtifactStore *store) noexcept { artifact_store = store; }
            ShaderArtifactStore *GetArtifactStore() const noexcept { return artifact_store; }
            void SetProgramArtifactMetadata(
                const ShaderProgramArtifactMetadata &metadata)
            {
                program_metadata = metadata;
                has_program_metadata =
                    IsValidShaderProgramArtifactMetadata(metadata);
            }
            bool HasProgramArtifactMetadata() const noexcept
            {
                return has_program_metadata;
            }
            void SetUsedCachedStages(const bool mesh, const bool fragment) noexcept
            {
                used_cached_mesh_stage = mesh;
                used_cached_fragment_stage = fragment;
            }
            bool UsedCachedMeshStage() const noexcept { return used_cached_mesh_stage; }
            bool UsedCachedFragmentStage() const noexcept { return used_cached_fragment_stage; }
            const ShaderProgramArtifactMetadata &
                GetProgramArtifactMetadata() const noexcept
            {
                return program_metadata;
            }
        public:

            ShaderBuildContext(const PrimitiveType primitive_type_value, const uint32_t shader_stage_bits)
                : primitive_type(primitive_type_value), shader_stage_flag_bits(shader_stage_bits)
            {
                if (has_mesh())
                    shader_map.Add(new ShaderCreateInfo(ShaderStage::Mesh));
                if (has_fragment())
                    shader_map.Add(new ShaderCreateInfo(ShaderStage::Fragment));
            }
            ~ShaderBuildContext() = default;        ///< `shader_map` 由 `SharedPtr` 自动归还，无需手写析构

            bool CreateShaderDirect()                ///< 直接编译各阶段的 FinalGLSL 到 SPV
            {
                if (shader_map.IsEmpty())
                    return(false);

                for (auto &kv : shader_map)
                {
                    ShaderCreateInfo *sc = kv.second.get();

                    if (!sc->HasSPVData() && !sc->CompileFinalGLSLToSPV())
                        return(false);
                }

                return(true);
            }
        };//class ShaderBuildContext
}//namespace hgl::graph::mtl
