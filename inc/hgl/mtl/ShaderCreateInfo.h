#pragma once

namespace hgl::graph::mtl {}

#include <hgl/common/ShaderStageDef.h>
#include <hgl/type/ValueArray.h>
#include <hgl/type/String.h>
#include<hgl/log/Log.h>
#include<string>

namespace hgl::graph {}  // force open for forward decl
namespace hgl::graph { struct SPVData; }

namespace hgl{namespace graph::mtl
{
    using hgl::graph::SPVData;

class ShaderCreateInfo
{
    OBJECT_LOGGER

public:

    /** 释放 `SPVData` 的回调。SPV 内存归编译器插件所有，所以释放必须由生成器提供；
     *  发行形态（ULRE_ENABLE_SHADER_PROGRAM_BUILD=0）没有生成器 ⇒ 该指针为 null，
     *  此时 `spv_data` 恒为 null（只走 `cached_spv_data` 路径）。 */
    using SPVReleaser = void (*)(SPVData *);

private:

    static SPVReleaser spv_releaser;

protected:

    ShaderStage shader_stage;                      ///<着色器阶段

    std::string final_shader;

    SPVData *spv_data;
    ValueArray<uint32> cached_spv_data;

protected:

    bool CompileToSPV();
    bool CompileToSPVWithDiagnostic(AnsiString &out_diagnostic);

public:

    const ShaderStage GetShaderStage()const{return shader_stage;}

public:

    explicit ShaderCreateInfo(ShaderStage stage);
    virtual ~ShaderCreateInfo();

    /** 由生成器在安装接缝时装上；传 nullptr 即卸载。 */
    static void SetSPVReleaser(SPVReleaser releaser){spv_releaser=releaser;}

    const std::string &GetFinalGLSL()const{return final_shader;}
    void SetFinalGLSL(const std::string &glsl){final_shader=glsl;}
    void SetFinalGLSL(const char *glsl){final_shader=glsl?glsl:"";}

    bool CompileFinalGLSLToSPV();                ///< 直接编译 final_shader 到 SPV

    /** 同 `CompileFinalGLSLToSPV`，但失败时把 glslang 的原始报错留在
     *  `out_diagnostic`（成功时清空）。刻意不合并进上面那个签名：发行形态的
     *  `ShaderProgramManager` 也调用 `CompileFinalGLSLToSPV`，改签名等于改 ABI。 */
    bool CompileFinalGLSLToSPVWithDiagnostic(AnsiString &out_diagnostic);

    bool SetCachedSPVData(const void *data, size_t byte_size);
    bool HasSPVData() const;
    const uint32 *GetSPVData()const;
    const size_t GetSPVSize()const;
};//class ShaderCreateInfo
}}//namespace hgl::graph::mtl
