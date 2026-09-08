#pragma once

#include<hgl/mtl/DescriptorSetLayoutAllocator.h>
#include<hgl/mtl/ShaderResourceSchema.h>
#include<hgl/mtl/ShaderCreateInfoMap.h>
#include<hgl/mtl/ShaderLinkSpec.h>
#include<hgl/mtl/ShaderArtifactContract.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/common/PrimitiveTypeDef.h>
#include<hgl/common/ShaderStageDef.h>
#include <hgl/common/TextureSamplerTypeDef.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include<string>
#include<vector>

namespace hgl::graph::mtl
{
        using namespace hgl::graph::mtl;
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

            DescriptorSetLayoutAllocator descriptor_allocator;                  ///<材质描述符分配器
            mtl::ShaderResourceSchema shader_resource_schema;                       ///<descriptor semantic contract (phase 2)

            ShaderCreateInfoMap shader_map;                         ///<着色器列表

            ShaderLinkSpec program_link;
            bool has_program_link = false;
            ShaderArtifactStore *artifact_store = nullptr;
            ShaderProgramArtifactMetadata program_metadata{};
            bool has_program_metadata = false;

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

            const DescriptorSetLayoutAllocator &GetDescriptorAllocator()const{return descriptor_allocator;}
            const mtl::ShaderResourceSchema &GetShaderResourceSchema()const{return shader_resource_schema;}

            void SetShaderResourceSchema(const mtl::ShaderResourceSchema &contract){shader_resource_schema=contract;}

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
            const ShaderProgramArtifactMetadata &
                GetProgramArtifactMetadata() const noexcept
            {
                return program_metadata;
            }
        public:

            ShaderBuildContext(const PrimitiveType primitive_type, const uint32_t shader_stage_bits);
            ~ShaderBuildContext();  // Need explicit destructor to properly clean up shader_map

            bool CreateShaderDirect();               ///< 直接编译各阶段的 FinalGLSL 到 SPV
        };//class ShaderBuildContext
}//namespace hgl::graph::mtl
