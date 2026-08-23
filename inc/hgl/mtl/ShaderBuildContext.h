#pragma once

#include<hgl/mtl/DescriptorSetLayoutAllocator.h>
#include<hgl/mtl/ShaderResourceSchema.h>
#include<hgl/mtl/ShaderCreateInfoMap.h>
#include<hgl/mtl/ShaderLinkSpec.h>
#include<hgl/mtl/ShaderArtifactContract.h>
#include<hgl/common/PrimitiveTypeDef.h>
#include<hgl/common/ShaderStageDef.h>
#include <hgl/common/TextureSamplerTypeDef.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include<string>

namespace hgl::graph
{
    struct ShaderBufferSource;
}

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
            uint32_t ubo_range;
            uint32_t ssbo_range;

            DescriptorSetLayoutAllocator descriptor_allocator;                  ///<材质描述符分配器
            mtl::ShaderResourceSchema shader_resource_schema;                       ///<descriptor semantic contract (phase 2)

            uint32_t local_to_world_max_count;
            uint32_t local_to_world_stage_bits;
            SSBODescriptor *local_to_world_ssbo;

            ShaderCreateInfoMap shader_map;                         ///<着色器列表

            bool has_local_to_world;
            ShaderLinkSpec program_link;
            bool has_program_link = false;
            ShaderArtifactStore *artifact_store = nullptr;
            ShaderProgramArtifactMetadata program_metadata{};
            bool has_program_metadata = false;

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
            const bool HasLocalToWorld                  ()const{return has_local_to_world;}

        public:

            ShaderBuildContext(const PrimitiveType primitive_type, const uint32_t shader_stage_bits, const bool has_local_to_world);
            ~ShaderBuildContext();  // Need explicit destructor to properly clean up shader_map

            void SetDevice(const contract::PhysicalDeviceProfileLite *profile);

            bool SetLocalToWorld(const uint32_t shader_stage_flag_bits);

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

            // —— 语义化 SSBO 注册（MeshShader 方向：按用途明确区分）——
            bool AddSSBOVertex(const uint32_t flag_bits,const ShaderBufferSource &ss);      ///< 顶点数据（Position/UV/NTB/Joint）
            bool AddSSBOVertexIndex(const uint32_t flag_bits);                              ///< 顶点索引
            bool AddSSBOMtlData(const uint32_t flag_bits,const std::string &struct_name,const std::string &name,const int data_slot);   ///< 材质数据槽
            bool AddSSBOMtlIndex(const uint32_t flag_bits);                                 ///< 材质数据行表
            bool AddSSBOTextureLayer(const uint32_t flag_bits,const int binding);           ///< 纹理层表

            bool CreateShaderDirect();               ///< 直接编译各阶段的 FinalGLSL 到 SPV
        };//class ShaderBuildContext
}//namespace hgl::graph::mtl
