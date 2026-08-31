#pragma once

namespace hgl::graph::mtl {}

#include <hgl/common/ShaderStageDef.h>
#include <hgl/type/ValueArray.h>
#include<hgl/log/Log.h>
#include<string>

namespace hgl::graph {}  // force open for forward decl
namespace hgl::graph { struct SPVData; }

namespace hgl{namespace graph::mtl
{
    using namespace hgl::graph::mtl;
    using hgl::graph::SPVData;

class ShaderCreateInfo
{
    OBJECT_LOGGER

protected:

    ShaderStage shader_stage;                      ///<着色器阶段

    std::string final_shader;

    SPVData *spv_data;
    ValueArray<uint32> cached_spv_data;

protected:

    bool CompileToSPV();

public:

    const ShaderStage GetShaderStage()const{return shader_stage;}

public:

    explicit ShaderCreateInfo(ShaderStage stage);
    virtual ~ShaderCreateInfo();

    const std::string &GetFinalGLSL()const{return final_shader;}
    void SetFinalGLSL(const std::string &glsl){final_shader=glsl;}
    void SetFinalGLSL(const char *glsl){final_shader=glsl?glsl:"";}

    bool CompileFinalGLSLToSPV();                ///< 直接编译 final_shader 到 SPV
    bool SetCachedSPVData(const void *data, size_t byte_size);
    const uint32 *GetSPVData()const;
    const size_t GetSPVSize()const;
};//class ShaderCreateInfo
}}//namespace hgl::graph::mtl
