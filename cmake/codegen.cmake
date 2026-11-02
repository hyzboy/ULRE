# ============================================================================
# codegen.cmake —— Clang AST 派生产物的构建期再生成
#
# 两个生成器，同一套策略（改真源 ⇒ 构建前自动重跑 ⇒ 生成物落回源码树）：
#
#   AssetGen        inc/hgl/asset/generated/*.gen.h
#                   ← 带 HGL_ASSET_STRUCT / HGL_ASSET_CLASS 注解的结构
#   ShaderConstGen  ShaderLibrary/contract/*.glsl
#                   inc/hgl/shader/generated/ShaderConstantsParity.gen.h
#                   ← 带 HGL_SHADER_ENUM 注解的契约枚举
#
# 产物提交在仓库里，所以**只用 MSVC 的构建永远不需要 Clang**；但一旦改了真源，
# 就必须重跑生成器，否则生成物与真源脱节。
#
# 设计约束：
#   * 可选可降级：找不到 Python 3.11+ 或 clang++ 时只打 STATUS 并跳过，
#     不破坏「只用已提交生成物」的构建；要用硬门禁请设
#     -DULRE_REQUIRE_CLANG_CODEGEN=ON。
#   * 幂等：生成器内容未变时不写盘，不会造成无谓重编译。
#   * 产物仍落在源码树（与现状一致），因此 include 写法不用改。
# ============================================================================

option(ULRE_ENABLE_CLANG_CODEGEN
       "构建期自动运行 Clang AST 代码生成（AssetGen / ShaderConstGen）" ON)
option(ULRE_REQUIRE_CLANG_CODEGEN
       "ULRE_ENABLE_CLANG_CODEGEN=ON 但缺少 Python/Clang 时直接报错" OFF)
option(ULRE_ENABLE_SHADER_CONST_CODEGEN
       "构建期自动运行 ShaderConstGen（C++ 契约枚举 → GLSL 宏 + static_assert 头）" ON)

set(ULRE_CLANGXX "" CACHE FILEPATH
    "Clang-AST 代码生成使用的 clang++（留空自动探测，也可用环境变量 HGL_CLANG）")
set(ULRE_CODEGEN_PYTHON "" CACHE FILEPATH
    "Clang-AST 代码生成使用的 Python 3.11+（留空自动探测）")
set(ULRE_CODEGEN_EXTRA_INCLUDE_DIRS "" CACHE STRING
    "追加给代码生成器的 include 目录（分号分隔）")

# ── 工具探测 ────────────────────────────────────────────────────────────────

function(_ulre_codegen_find_python out_var)
    if(ULRE_CODEGEN_PYTHON AND EXISTS "${ULRE_CODEGEN_PYTHON}")
        set(${out_var} "${ULRE_CODEGEN_PYTHON}" PARENT_SCOPE)
        return()
    endif()

    find_package(Python3 3.11 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(${out_var} "${Python3_EXECUTABLE}" PARENT_SCOPE)
    else()
        set(${out_var} "" PARENT_SCOPE)
    endif()
endfunction()

function(_ulre_codegen_find_clang out_var)
    if(ULRE_CLANGXX AND EXISTS "${ULRE_CLANGXX}")
        set(${out_var} "${ULRE_CLANGXX}" PARENT_SCOPE)
        return()
    endif()

    if(DEFINED ENV{HGL_CLANG} AND EXISTS "$ENV{HGL_CLANG}")
        set(${out_var} "$ENV{HGL_CLANG}" PARENT_SCOPE)
        return()
    endif()

    find_program(_ulre_clangxx NAMES clang++ clang++-20 clang++-19 clang++-18 clang++-17
                 HINTS "C:/Program Files/LLVM/bin" "C:/LLVM/bin" "D:/LLVM/bin")
    set(${out_var} "${_ulre_clangxx}" PARENT_SCOPE)
endfunction()

# 生成器只认得 clang 的 include 搜索路径，这里补齐第三方头（glm / Vulkan 等）。
function(_ulre_codegen_collect_includes out_var)
    set(_dirs "")

    set(_triplet "x64-windows")
    if(VCPKG_TARGET_TRIPLET)
        set(_triplet "${VCPKG_TARGET_TRIPLET}")
    endif()

    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/installed/${_triplet}/include")
        list(APPEND _dirs "$ENV{VCPKG_ROOT}/installed/${_triplet}/include")
    endif()

    # 由 vcpkg.cmake 工具链反推 vcpkg 根目录
    if(CMAKE_TOOLCHAIN_FILE MATCHES "^(.*)[/\\\\]scripts[/\\\\]buildsystems[/\\\\]vcpkg\\.cmake$")
        if(EXISTS "${CMAKE_MATCH_1}/installed/${_triplet}/include")
            list(APPEND _dirs "${CMAKE_MATCH_1}/installed/${_triplet}/include")
        endif()
    endif()

    foreach(_guess IN ITEMS "D:/vcpkg" "C:/vcpkg")
        if(EXISTS "${_guess}/installed/${_triplet}/include")
            list(APPEND _dirs "${_guess}/installed/${_triplet}/include")
        endif()
    endforeach()

    if(Vulkan_INCLUDE_DIRS)
        list(APPEND _dirs ${Vulkan_INCLUDE_DIRS})
    endif()
    if(DEFINED ENV{VULKAN_SDK} AND EXISTS "$ENV{VULKAN_SDK}/Include")
        list(APPEND _dirs "$ENV{VULKAN_SDK}/Include")
    endif()

    if(DEFINED ENV{HGL_ASSETGEN_INCLUDE_DIRS})
        list(APPEND _dirs $ENV{HGL_ASSETGEN_INCLUDE_DIRS})
    endif()
    if(ULRE_CODEGEN_EXTRA_INCLUDE_DIRS)
        list(APPEND _dirs ${ULRE_CODEGEN_EXTRA_INCLUDE_DIRS})
    endif()

    set(_existing "")
    foreach(_d IN LISTS _dirs)
        if(EXISTS "${_d}")
            file(TO_CMAKE_PATH "${_d}" _d)
            list(APPEND _existing "${_d}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _existing)

    set(${out_var} "${_existing}" PARENT_SCOPE)
endfunction()

# ── 公共入口 ────────────────────────────────────────────────────────────────

# 探测工具链并创建代码生成 target；必须在 find_package(Vulkan) 之后调用。
function(ulre_codegen_configure)
    set(ULRE_CODEGEN_AVAILABLE OFF PARENT_SCOPE)
    set(ULRE_CODEGEN_TARGET "" PARENT_SCOPE)

    if(NOT ULRE_ENABLE_CLANG_CODEGEN)
        message(STATUS "[CodeGen] 构建期再生成已禁用（ULRE_ENABLE_CLANG_CODEGEN=OFF）")
        return()
    endif()

    set(_root "${ULRE_CODEGEN_REPO_ROOT}")
    if(NOT _root)
        set(_root "${CMAKE_SOURCE_DIR}")
    endif()

    _ulre_codegen_find_python(_python)
    _ulre_codegen_find_clang(_clang)

    set(_missing "")
    if(NOT _python)
        list(APPEND _missing "Python 3.11+")
    endif()
    if(NOT _clang)
        list(APPEND _missing "clang++")
    endif()

    if(_missing)
        string(REPLACE ";" "、" _missing_text "${_missing}")
        if(ULRE_REQUIRE_CLANG_CODEGEN)
            message(FATAL_ERROR
                "[CodeGen] 未找到 ${_missing_text}。请安装它们，或用 "
                "-DULRE_ENABLE_CLANG_CODEGEN=OFF 关闭构建期再生成。")
        endif()
        message(STATUS
            "[CodeGen] 跳过构建期再生成：未找到 ${_missing_text}；"
            "仓库内已提交的 inc/hgl/asset/generated/*.gen.h 仍可正常编译。")
        return()
    endif()

    set(_tool   "${_root}/src/Tools/AssetGen/asset_gen.py")
    set(_config "${_root}/src/Tools/AssetGen/asset_gen.toml")
    if(NOT EXISTS "${_tool}" OR NOT EXISTS "${_config}")
        message(STATUS "[CodeGen] 找不到 ${_tool} 或 asset_gen.toml，跳过构建期再生成。")
        return()
    endif()

    # 把探测结果发布给 CMake（ctest 用例 shader_const_gen_check 依赖它们；
    # 探测失败时上面的 _missing 分支已 return，不会走到这里）。
    set(ULRE_CODEGEN_PYTHON_EXE "${_python}" CACHE INTERNAL "CodeGen 探测到的 Python 3.11+")
    set(ULRE_CODEGEN_CLANG_EXE  "${_clang}"  CACHE INTERNAL "CodeGen 探测到的 clang++")

    # 生成器读取的输入面：五个默认 include 根 + 生成器自身 + 配置
    # （CMAssetsManage/inc 承接 hgl/asset/AssetAnnotation.h、AssetObject.h 与
    #   整个 hgl/serialize（反射内核 + Toml/Flat/ArchiveFile 后端）；*Spec.h 的 #include 全走它）
    file(GLOB_RECURSE _inputs CONFIGURE_DEPENDS
         "${_root}/inc/*.h" "${_root}/inc/*.hpp" "${_root}/inc/*.inl"
         "${_root}/CMCoreType/inc/*.h" "${_root}/CMCore/inc/*.h" "${_root}/CMMath/inc/*.h"
         "${_root}/CMAssetsManage/inc/*.h")

    _ulre_codegen_collect_includes(_includes)
    set(_inc_args "")
    foreach(_d IN LISTS _includes)
        list(APPEND _inc_args --include-dir "${_d}")
    endforeach()

    set(_stamp_dir "${CMAKE_BINARY_DIR}/codegen")
    set(_stamp     "${_stamp_dir}/assetgen.stamp")
    file(MAKE_DIRECTORY "${_stamp_dir}")

    add_custom_command(
        OUTPUT "${_stamp}"
        COMMAND "${CMAKE_COMMAND}" -E env "PYTHONIOENCODING=utf-8"
                "${_python}" "${_tool}"
                --repo-root "${_root}"
                --config   "src/Tools/AssetGen/asset_gen.toml"
                --clang    "${_clang}"
                ${_inc_args}
                --write
        COMMAND "${CMAKE_COMMAND}" -E touch "${_stamp}"
        DEPENDS "${_tool}" "${_config}" ${_inputs}
        COMMENT "[CodeGen] AssetGen：扫描 HGL_ASSET_* 注解，刷新 inc/hgl/asset/generated/*.gen.h"
        VERBATIM)

    add_custom_target(ULRE_CodeGen_AssetGen
        DEPENDS "${_stamp}"
        SOURCES "${_tool}" "${_config}")
    set_target_properties(ULRE_CodeGen_AssetGen PROPERTIES FOLDER "CodeGen")

    set(ULRE_CODEGEN_TARGET  ULRE_CodeGen_AssetGen PARENT_SCOPE)
    set(ULRE_CODEGEN_AVAILABLE ON                    PARENT_SCOPE)

    message(STATUS "[CodeGen] AssetGen 构建期再生成已启用")
    message(STATUS "[CodeGen]   python : ${_python}")
    message(STATUS "[CodeGen]   clang++: ${_clang}")
    foreach(_d IN LISTS _includes)
        message(STATUS "[CodeGen]   include: ${_d}")
    endforeach()

    # ── ShaderConstGen：C++ 契约枚举 → GLSL 宏 + static_assert 头 ─────────────
    # 输入面 = 契约头 + 生成器自身 + 策略 toml + 被对拍的 GLSL 与 sampler.toml。
    # 生成物落在源码树（ShaderLibrary/contract/、inc/hgl/shader/generated/），
    # 与 AssetGen 同策略：提交在仓库里，MSVC-only 构建不需要 clang。
    if(ULRE_ENABLE_SHADER_CONST_CODEGEN)
        set(_const_tool "${_root}/src/Tools/ShaderConstGen/shader_const_gen.py")
        set(_const_cfg  "${_root}/src/Tools/ShaderConstGen/shader_const_gen.toml")
        set(_const_contracts "${_root}/inc/hgl/shader/contract")
        if(NOT EXISTS "${_const_tool}" OR NOT EXISTS "${_const_cfg}")
            message(STATUS "[CodeGen] 跳过 ShaderConstGen：找不到生成器或策略 toml")
        else()
            file(GLOB_RECURSE _const_inputs CONFIGURE_DEPENDS
                 "${_const_contracts}/*.h"
                 "${_root}/src/Tools/ShaderConstGen/*.py"
                 "${_root}/ShaderLibrary/*.glsl"
                 "${_root}/ShaderLibrary/contract/*.toml")

            # 注意不要强制 PYTHONIOENCODING=utf-8：生成器输出含中文，
            # 强制 UTF-8 会让 GBK 控制台（MSBuild 默认）显示成乱码。
            # clang AST 的解码在 Python 内部显式指定 UTF-8，与 stdout 编码无关。
            add_custom_command(
                OUTPUT "${_stamp_dir}/shader_const.stamp"
                COMMAND "${_python}" "${_const_tool}"
                        --repo-root "${_root}"
                        --config   "${_const_cfg}"
                        --clang    "${_clang}"
                        --write
                COMMAND "${CMAKE_COMMAND}" -E touch "${_stamp_dir}/shader_const.stamp"
                DEPENDS "${_const_tool}" "${_const_cfg}" ${_const_inputs}
                COMMENT "[CodeGen] ShaderConstGen：扫描 HGL_SHADER_ENUM 契约枚举，刷新 GLSL 宏与 parity 头"
                VERBATIM)

            add_custom_target(ULRE_CodeGen_ShaderConst
                DEPENDS "${_stamp_dir}/shader_const.stamp"
                SOURCES "${_const_tool}" "${_const_cfg}")
            set_target_properties(ULRE_CodeGen_ShaderConst PROPERTIES FOLDER "CodeGen")

            list(APPEND ULRE_CODEGEN_TARGET ULRE_CodeGen_ShaderConst)
            message(STATUS "[CodeGen] ShaderConstGen 构建期再生成已启用")
        endif()
    endif()

    set(ULRE_CODEGEN_TARGET "${ULRE_CODEGEN_TARGET}" PARENT_SCOPE)
endfunction()

# 把代码生成挂到消费生成物的 target 之前；不存在的 target 静默跳过。
function(ulre_codegen_bind)
    if(NOT ULRE_CODEGEN_AVAILABLE OR NOT ULRE_CODEGEN_TARGET)
        return()
    endif()

    foreach(_t IN LISTS ARGN)
        if(TARGET ${_t})
            add_dependencies(${_t} ${ULRE_CODEGEN_TARGET})
        endif()
    endforeach()
endfunction()
