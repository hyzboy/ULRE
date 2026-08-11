#pragma once

#include<hgl/shadergen/DescriptorSetLayoutAllocator.h>
#include<hgl/mtl/ShaderResourceSchema.h>
#include<hgl/shadergen/ShaderCreateInfoMap.h>
#include<hgl/shadergen/ShaderProgramLinkSpec.h>
#include<hgl/shadergen/ShaderArtifactContract.h>
#include<hgl/common/PrimitiveTypeDef.h>
#include<hgl/common/ShaderStageDef.h>
#include <hgl/common/TextureSamplerTypeDef.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include<string>

namespace hgl::graph
{
    struct ShaderBufferSource;

    namespace shadergen
    {
        using namespace hgl::graph::mtl;
        using hgl::graph::ShaderStage;
        using hgl::graph::DescriptorSetType;

        namespace contract
        {
            struct PhysicalDeviceProfileLite;
        }
        class ShaderArtifactStore;
        class ShaderCreateInfoVertex;
        class ShaderCreateInfo;

        class ShaderProgramBuildSpec
        {
        protected:

            PrimitiveType primitive_type = PrimitiveType::Triangles;
            uint32_t shader_stage_flag_bits = 0;
            uint32_t ubo_range;
            uint32_t ssbo_range;

            DescriptorSetLayoutAllocator descriptor_db;                    ///<材质描述符管理器
            mtl::ShaderResourceSchema material_resource_layout;                       ///<descriptor semantic contract (phase 2)

            uint32_t local_to_world_max_count;
            uint32_t local_to_world_stage_bits;
            SSBODescriptor *local_to_world_ssbo;

            ShaderCreateInfoMap shader_map;                         ///<着色器列表

            bool has_local_to_world;
            ShaderProgramLinkSpec program_link;
            bool has_program_link = false;
            ShaderArtifactStore *artifact_store = nullptr;
            ShaderProgramArtifactMetadata program_metadata{};
            bool has_program_metadata = false;

        public:

            const PrimitiveType GetPrimitiveType()const{return primitive_type;}

            const   uint32      GetShaderStage  ()const{return shader_stage_flag_bits;}

                    bool        hasShader       (const ShaderStage ss)const{return shader_stage_flag_bits&(uint32)ss;}

                    bool        hasVertex       ()const{return hasShader(ShaderStage::Vertex);}
        //          bool        hasTessCtrl     ()const{return hasShader(ShaderStage::TessControl);}
        //          bool        hasTessEval     ()const{return hasShader(ShaderStage::TessEval);}
                    bool        hasFragment     ()const{return hasShader(ShaderStage::Fragment);}
        //          bool        hasCompute      ()const{return hasShader(ShaderStage::Compute);}

            ShaderCreateInfo *         GetStageShader(const ShaderStage ss){return shader_map[ss];}
            const ShaderCreateInfo *   GetStageShader(const ShaderStage ss)const{return shader_map[ss];}

            ShaderCreateInfoVertex *           GetVertexShader(){return reinterpret_cast<ShaderCreateInfoVertex *>(GetStageShader(ShaderStage::Vertex));}
            const ShaderCreateInfoVertex *     GetVertexShader()const{return reinterpret_cast<const ShaderCreateInfoVertex *>(GetStageShader(ShaderStage::Vertex));}

            const ShaderCreateInfoMap &GetShaderMap()const{return shader_map;}

        public:

            const DescriptorSetLayoutAllocator &GetDescriptorInfo()const{return descriptor_db;}
            const mtl::ShaderResourceSchema &GetShaderResourceSchema()const{return material_resource_layout;}

            void SetShaderResourceSchema(const mtl::ShaderResourceSchema &contract){material_resource_layout=contract;}

            void SetProgramLink(const ShaderProgramLinkSpec &link)
            {
                program_link = link;
                has_program_link = link.IsValid();
            }

            bool HasProgramLink() const noexcept { return has_program_link; }
            const ShaderProgramLinkSpec &GetProgramLink() const noexcept { return program_link; }
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
            const bool HasLocalToWorld                  ()const{return has_local_to_world;}

        public:

            ShaderProgramBuildSpec(const PrimitiveType primitive_type, const uint32_t shader_stage_bits, const bool has_local_to_world);
            ~ShaderProgramBuildSpec();  // Need explicit destructor to properly clean up shader_map

            void SetDevice(const contract::PhysicalDeviceProfileLite *profile);

            bool SetLocalToWorld(const uint32_t shader_stage_flag_bits);
            //bool SetWorldPosition(const uint32_t shader_stage_flag_bits);

            bool AddStruct(const std::string &ubo_typename,const std::string &codes);
            bool AddStruct(const char *ubo_typename,const char *codes)
            {
                return AddStruct(std::string(ubo_typename?ubo_typename:""),std::string(codes?codes:""));
            }

            bool AddUBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name);
            bool AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const std::string &struct_name,const std::string &name);
            bool AddUBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const char *struct_name,const char *name)
            {
                return AddUBO(flag_bits,set_type,std::string(struct_name?struct_name:""),std::string(name?name:""));
            }
            bool AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const char *struct_name,const char *name)
            {
                return AddUBO(flag_bits,set_type,std::string(struct_name?struct_name:""),std::string(name?name:""));
            }

            bool AddUBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss);

            bool AddSSBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name);
            bool AddSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const std::string &struct_name,const std::string &name);
            bool AddSSBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name,const int preferred_binding);
            bool AddSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const std::string &struct_name,const std::string &name,const int preferred_binding);
            bool AddSSBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const char *struct_name,const char *name)
            {
                return AddSSBO(flag_bits,set_type,std::string(struct_name?struct_name:""),std::string(name?name:""));
            }
            bool AddSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const char *struct_name,const char *name)
            {
                return AddSSBO(flag_bits,set_type,std::string(struct_name?struct_name:""),std::string(name?name:""));
            }
            bool AddSSBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const char *struct_name,const char *name,const int preferred_binding)
            {
                return AddSSBO(flag_bits,set_type,std::string(struct_name?struct_name:""),std::string(name?name:""),preferred_binding);
            }
            bool AddSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const char *struct_name,const char *name,const int preferred_binding)
            {
                return AddSSBO(flag_bits,set_type,std::string(struct_name?struct_name:""),std::string(name?name:""),preferred_binding);
            }

            bool AddSSBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss);

            bool AddTexture(const ShaderStage flag_bits,const DescriptorSetType set_type,const TextureType &tt,const std::string &name);
            bool AddTextureSampler(const ShaderStage flag_bits,const DescriptorSetType set_type,const SamplerType &st,const std::string &name);
            bool AddTexture(const ShaderStage flag_bits,const DescriptorSetType set_type,const TextureType &tt,const char *name)
            {
                return AddTexture(flag_bits,set_type,tt,std::string(name?name:""));
            }
            bool AddTextureSampler(const ShaderStage flag_bits,const DescriptorSetType set_type,const SamplerType &st,const char *name)
            {
                return AddTextureSampler(flag_bits,set_type,st,std::string(name?name:""));
            }

            bool CreateShaderDirect();               ///< 直接编译各阶段的 FinalGLSL 到 SPV
        };//class ShaderProgramBuildSpec
    }//namespace shadergen
}//namespace hgl::graph
