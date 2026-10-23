#include <hgl/mtl/MaterialStageInterface.h>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    namespace
    {
        bool SetFailure(
            MaterialStageInterfaceDiagnostic &diagnostic,
            const MaterialStageInterfaceError error,
            const InterStageSemantic semantic =
                InterStageSemantic::Unknown) noexcept
        {
            diagnostic.error = error;
            diagnostic.semantic = semantic;
            return false;
        }

        // ── inter-stage 条目校验（本地化）───────────────────────────────
        // 原经 ShaderInterfaceContract 聚合体校验的往返已删，规则逐条保留：
        // 逐条（语义/location/插值/形状）+ 两两（语义不重复、location 区间不冲突）。
        bool IsValidInterStageShape(
            const ShaderSemanticScalarType scalar_type,
            const uint8 component_count,
            const uint8 location_width) noexcept
        {
            return scalar_type != ShaderSemanticScalarType::Unknown
                && component_count > 0
                && component_count <= 4
                && location_width > 0;
        }

        bool HasInterStageLocationConflict(
            const uint32 first_location,
            const uint32 first_width,
            const uint32 second_location,
            const uint32 second_width) noexcept
        {
            return first_location < second_location + second_width
                && second_location < first_location + first_width;
        }

        bool ValidateInterStageEntries(
            const ValueArray<InterStageSemanticContractEntry> &entries) noexcept
        {
            for (int i = 0; i < entries.GetCount(); ++i)
            {
                const InterStageSemanticContractEntry &entry = entries[i];
                if (entry.semantic == InterStageSemantic::Unknown
                 || entry.location == InvalidShaderSemanticLocation
                 || entry.interpolation < InterStageInterpolation::Smooth
                 || entry.interpolation > InterStageInterpolation::NoPerspective
                 || !IsValidInterStageShape(
                        entry.scalar_type,
                        entry.component_count,
                        entry.location_width))
                    return false;

                for (int j = 0; j < i; ++j)
                {
                    const InterStageSemanticContractEntry &other = entries[j];
                    if (entry.semantic == other.semantic
                     || HasInterStageLocationConflict(
                            entry.location,
                            entry.location_width,
                            other.location,
                            other.location_width))
                        return false;
                }
            }
            return true;
        }
    }

    const char *GetMaterialStageInterfaceErrorName(
        const MaterialStageInterfaceError error) noexcept
    {
#define HGL_ERROR(name) case MaterialStageInterfaceError::name: return #name;
        switch (error)
        {
            HGL_MATERIAL_STAGE_INTERFACE_ERROR_LIST
        }
#undef HGL_ERROR
        return "Unknown";
    }

    InterStageSemanticMask GetMaterialInterStageSemanticMask(
        const MaterialVertexVaryingConfig &varying) noexcept
    {
        // 唯一实现见 MaterialStageInterface.h 的定义表（正反两向同源）。
        return BuildMaterialInterStageSemanticMaskFromVarying(varying);
    }

    bool BuildMaterialStageInterface(
        const MaterialVertexVaryingConfig &varying,
        ValueArray<InterStageSemanticContractEntry> &out_entries,
        MaterialStageInterfaceDiagnostic &out_diagnostic) noexcept
    {
        out_entries.Clear();
        out_diagnostic = {};

        const InterStageSemanticMask mask =
            GetMaterialInterStageSemanticMask(varying);
        for (uint32 value = 1;
             value < static_cast<uint32>(InterStageSemantic::RANGE_SIZE);
             ++value)
        {
            const InterStageSemantic semantic =
                static_cast<InterStageSemantic>(value);
            if (!(mask & GetInterStageSemanticMask(semantic)))
                continue;

            const InterStageSemanticInfo *info =
                GetInterStageSemanticInfo(semantic);
            if (!info
             || info->stable_location == InvalidShaderSemanticLocation
             || info->location_width == 0)
            {
                return SetFailure(
                    out_diagnostic,
                    MaterialStageInterfaceError::MissingSemanticMetadata,
                    semantic);
            }

            out_entries.Add(
                {
                    semantic,
                    info->value_shape.scalar_type,
                    info->interpolation,
                    info->value_shape.component_count,
                    info->location_width,
                    info->stable_location
                });
        }

        if (!ValidateInterStageEntries(out_entries))
            return SetFailure(
                out_diagnostic,
                MaterialStageInterfaceError::InvalidContract);
        return true;
    }

    const InterStageSemanticContractEntry *FindMaterialStageInterfaceEntry(
        const ValueArray<InterStageSemanticContractEntry> &entries,
        const InterStageSemantic semantic) noexcept
    {
        for (int i = 0; i < entries.GetCount(); ++i)
        {
            if (entries[i].semantic == semantic)
                return &entries[i];
        }
        return nullptr;
    }

    bool BuildGLSLInterStageDeclaration(
        const InterStageSemanticContractEntry &entry,
        const char *direction,
        AnsiString &out_declaration)
    {
        out_declaration.Clear();
        const InterStageSemanticInfo *info =
            GetInterStageSemanticInfo(entry.semantic);
        const char *type_name =
            GetGLSLTypeName(entry.scalar_type, entry.component_count);
        if (!info
         || !info->shader_symbol
         || !direction
         || !direction[0]
         || !type_name)
            return false;

        out_declaration =
            AnsiString("layout(location=")
            + AnsiString::numberOf(entry.location)
            + AnsiString(") ");
        // per-primitive 语义（DataIndexID/StyleID）：mesh 侧 out 是 perprimitiveEXT
        // 数组，FS 侧 in 必须同加 perprimitiveEXT（VUID-08746 接口装饰匹配；
        // 扩展 GL_EXT_mesh_shader 由 MaterialShaderCompiler 统一注入 FS）。
        // glslang 要求 uint 必须 flat 且顺序为 flat 在前（perprimitiveEXT flat 报
        // unexpected FLAT——qualifier 组合顺序坑，实测 flat perprimitiveEXT 合法）。
        if (IsPerPrimitiveInterStageSemantic(entry.semantic))
            out_declaration += "flat perprimitiveEXT in ";
        else
        {
            if (entry.interpolation == InterStageInterpolation::Flat)
                out_declaration += "flat ";
            else if (entry.interpolation
                == InterStageInterpolation::NoPerspective)
                out_declaration += "noperspective ";
            out_declaration += direction;
        }
        out_declaration += " ";
        out_declaration += type_name;
        out_declaration += " ";
        out_declaration += info->shader_symbol;
        out_declaration += ";";
        return true;
    }

    bool BuildGLSLMaterialSurfaceInput(
        const ValueArray<InterStageSemanticContractEntry> &entries,
        bool camera_ubo_available,
        AnsiString &out_code)
    {
        out_code =
            "    SurfaceInput si;\n"
            "    si.worldPos = vec3(0.0);\n"
            "    si.worldNormal = vec3(0.0, 0.0, 1.0);\n"
            "    si.uv0 = vec2(0.0);\n"
            "    si.uv1 = vec2(0.0);\n"
            "    si.vertexColor = vec4(1.0);\n"
            "    si.viewDir = vec3(0.0, 0.0, 1.0);\n"
            "    si.screenPos = gl_FragCoord.xy;\n"
            "    si.luminance = 1.0;\n"
            "    si.styleID = 0u;\n";

        if (FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::WorldPosition))
        {
            out_code += "    si.worldPos = fragWorldPos;\n";
            // viewDir 需要真实相机位置（相机绕目标轨道运动时不在原点）。
            // camera UBO 仅在启用 scene lighting 的材质里声明；
            // 未启用时 viewDir 不参与光照，退回原点假设即可。
            out_code += camera_ubo_available
                ? "    si.viewDir = normalize(camera.camera_world_pos - fragWorldPos);\n"
                : "    si.viewDir = normalize(-fragWorldPos);\n";
        }
        if (FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::WorldNormal))
            out_code +=
                "    si.worldNormal = normalize(fragWorldNormal);\n";

        // 面法线降级（`HGL_MATERIAL_FACE_NORMAL_FALLBACK`）：
        // 材质要法线光照、但几何顶点格式没有 Normal 语义 ⇒ 用世界位置的屏幕空间导数
        // 叉积求**三角形面法线**（与 error_checkerboard_3d_source.glsl 同一约定）。
        // 开关由 GenericMaterialBuilder::PrepareResolvedDefinition 决定，这里用
        // GLSL 预处理包住 ⇒ 不降级时本段零成本、且不引用任何可能不存在的 varying。
        // 退化三角形（面积为 0）保持预置值 (0,0,1)，避免 NaN。
        out_code +=
            "#ifdef HGL_MATERIAL_FACE_NORMAL_FALLBACK\n"
            "    {\n"
            "        vec3 face_normal_vector =\n"
            "            cross(dFdx(si.worldPos), dFdy(si.worldPos));\n"
            "        const float face_normal_length_squared =\n"
            "            dot(face_normal_vector, face_normal_vector);\n"
            "        if (face_normal_length_squared > 1e-12)\n"
            "        {\n"
            "            face_normal_vector *= inversesqrt(face_normal_length_squared);\n"
            "            si.worldNormal = gl_FrontFacing\n"
            "                ? face_normal_vector : -face_normal_vector;\n"
            "        }\n"
            "    }\n"
            "#endif\n";
        if (FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::UV0))
            out_code += "    si.uv0 = fragUV0;\n";
        if (FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::Color))
            out_code += "    si.vertexColor = fragVertexColor;\n";
        if (FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::Luminance))
            out_code += "    si.luminance = fragLuminance;\n";

        out_code += "    const uint materialDataIndex = ";
        out_code += FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::DataIndexID)
            ? "fragDataIndexID;\n" : "0u;\n";
        out_code += "    si.styleID = ";
        out_code += FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::StyleID)
            ? "fragStyleID;\n" : "0u;\n";
        return true;
    }

}
