#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""BufferGen —— UBO / SSBO 派生代码生成器

唯一真源：`inc/hgl/graph/ubo/*.h` 与 `inc/hgl/graph/ssbo/*.h` 里带
`HGL_UBO* / HGL_SSBO*` 整结构注解的 C++ 结构。

本工具读取 **Clang AST**（字段名/类型/顺序）与 **记录布局**
（`-fdump-record-layouts-complete`：偏移/尺寸/对齐），产出：

    ShaderLibrary/module/descriptor/ubo/*.glsl       结构本体（struct + buffer_reference 块）
    ShaderLibrary/module/descriptor/ssbo/*.glsl      同上
    inc/hgl/graph/generated/BufferToml.gen.h    toml11 读写
    inc/hgl/graph/generated/buffer_schema.fbs   FlatBuffers schema

C++ 结构上的注解只表达「它是什么」（整结构级），字段信息全部由 AST 推导，
因此**不需要逐字段标注**。

用法
----
    python src/Tools/BufferGen/buffer_gen.py --repo-root . --write
    python src/Tools/BufferGen/buffer_gen.py --repo-root . --check

`--check` 只比对不落盘：有差异则打印首个差异并返回 1（CI / 门禁用）。
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - py<3.11
    import tomli as tomllib  # type: ignore

SCAN_DIRS = ("inc/hgl/graph/ubo", "inc/hgl/graph/ssbo")
DEFAULT_INCLUDE_DIRS = ("inc", "CMCoreType/inc", "CMMath/inc")

ANNOT_RE = re.compile(
    r"struct\s+(?:alignas\s*\(\s*[^)]*\)\s*)?"
    r"(?P<macro>HGL_[A-Z_]+)\s+(?P<name>[A-Za-z_]\w*)"
)

ANNOT_KIND = {
    "HGL_UBO": "ubo",
    "HGL_UBO_BDA": "ubo",
    "HGL_SSBO": "ssbo",
    "HGL_SSBO_BDA": "ssbo",
    "HGL_SSBO_ROW": "ssbo",
    "HGL_GPU_STRUCT": "struct",
    "HGL_CPU_ONLY": "cpu_only",
}

# 注解宏 → (bda, access)。access 只影响 GLSL 侧 readonly 修饰。
ANNOT_META = {
    "HGL_UBO": (0, "readwrite"),
    "HGL_UBO_BDA": (1, "readonly"),
    "HGL_SSBO": (0, "readwrite"),
    "HGL_SSBO_BDA": (1, "readonly"),
    "HGL_SSBO_ROW": (1, "readwrite"),
    "HGL_GPU_STRUCT": (0, "readwrite"),
    "HGL_CPU_ONLY": (0, "readwrite"),
}

# BDA 取址在 C++ 侧只承诺 16 字节对齐（GetBufferDeviceAddressAligned16），
# 因此 GLSL 的 buffer_reference_align **不得**照抄 C++ 的 alignof（那是结构在数组里的
# 步长对齐，如 LightRow 的 64）——照抄会声明一个兑现不了的取址承诺。
DEFAULT_REF_ALIGN = 16

# ── C++ 类型 → GLSL 类型 ─────────────────────────────────────────────────────
SCALAR_TYPES = {
    "float": "float",
    "double": "double",
    "uint32_t": "uint",
    "uint32": "uint",
    "unsigned": "uint",
    "unsigned int": "uint",
    "int32_t": "int",
    "int32": "int",
    "int": "int",
    "uint64_t": "uint64_t",
    "uint64": "uint64_t",
    "unsigned long long": "uint64_t",
    "int64_t": "int64_t",
    "int64": "int64_t",
    "long long": "int64_t",
}

VECTOR_TYPES = {
    "Color3f": "vec3",
    "Color4f": "vec4",
    "Vector2f": "vec2",
    "Vector3f": "vec3",
    "Vector4f": "vec4",
    "Vector2u": "uvec2",
    "Vector3u": "uvec3",
    "Vector4u": "uvec4",
    "Vector2i": "ivec2",
    "Vector3i": "ivec3",
    "Vector4i": "ivec4",
}

MATRIX_TYPES = {
    "Matrix2f": "mat2",
    "Matrix3f": "mat3",
    "Matrix4f": "mat4",
}

# 这些类型在 GLSL 里需要额外的存储扩展，本工具**不静默降级**：遇到就报错。
UNSUPPORTED_SCALARS = {"uint16_t", "int16_t", "uint8_t", "int8_t", "bool"}


class GenError(RuntimeError):
    pass


@dataclass
class FieldInfo:
    name: str
    cpp_type: str          # 解析掉数组后的基础类型
    array_dim: str | None  # 数组长度原文（宏名保留原文）
    offset: int
    glsl_type: str
    is_nested: bool = False

    @property
    def is_pad(self) -> bool:
        return self.name.startswith("_pad") or self.name.startswith("reserved")


@dataclass
class StructInfo:
    name: str
    kind: str
    header: str            # 仓库相对路径
    modifiers: dict[str, str]
    fields: list[FieldInfo] = field(default_factory=list)
    size: int = 0
    align: int = 0
    cfg: dict = field(default_factory=dict)

    @property
    def glsl_struct(self) -> str:
        return self.cfg.get("glsl_struct") or f"{self.name}Data"

    @property
    def glsl_ref(self) -> str:
        return self.cfg.get("glsl_ref") or f"{self.name}Ref"

    @property
    def access_readonly(self) -> bool:
        return self.modifiers.get("access") == "readonly"

    @property
    def ns(self) -> str:
        """所属命名空间。缺省按独占目录推导（ubo/ → hgl::graph，ssbo/ → hgl::graph::ssbo）。"""
        if "ns" in self.cfg:
            return self.cfg["ns"]
        if "/ssbo/" in self.header:
            return "hgl::graph::ssbo"
        return "hgl::graph"

    @property
    def fq(self) -> str:
        return f"{self.ns}::{self.name}"


# 名字 → 完全限定名。字段引用嵌套结构时按它生成（生成器输出在
# hgl::graph::generated，未加限定会找不到 ssbo 命名空间里的结构）。
FQ: dict[str, str] = {}


def fq(name: str) -> str:
    return FQ.get(name, name)


# ── 注解发现（文本扫描；先定位候选，再交给 Clang 取权威信息） ────────────────
def discover(repo_root: Path) -> list[StructInfo]:
    found: list[StructInfo] = []
    for scan_dir in SCAN_DIRS:
        directory = repo_root / scan_dir
        if not directory.is_dir():
            continue
        for path in sorted(directory.glob("*.h")):
            text = path.read_text(encoding="utf-8", errors="ignore")
            for match in ANNOT_RE.finditer(text):
                macro = match.group("macro")
                kind = ANNOT_KIND.get(macro)
                if kind is None:
                    raise GenError(f"{path.name}: 未知注解宏 {macro}")
                found.append(
                    StructInfo(
                        name=match.group("name"),
                        kind=kind,
                        header=path.relative_to(repo_root).as_posix(),
                        modifiers={"macro": macro},
                    )
                )
    if not found:
        raise GenError("在 ubo/ ssbo/ 目录里没有发现任何注解结构")
    return found


def apply_config(structs: list[StructInfo], config: dict) -> list[StructInfo]:
    by_name = {s.name: s for s in structs}
    ordered: list[StructInfo] = []
    for entry in config.get("struct", []):
        name = entry["name"]
        target = by_name.get(name)
        if target is None:
            raise GenError(f"buffer_gen.toml 登记了未在源码中标注的结构：{name}")
        target.cfg = entry
        if "kind" in entry:
            target.kind = entry["kind"]
        ordered.append(target)
    missing = [s.name for s in structs if s.name not in {e["name"] for e in config.get("struct", [])}]
    if missing:
        raise GenError(
            "以下结构已在源码里标注但未在 buffer_gen.toml 登记（命名未知 ⇒ 拒绝对拍）："
            + ", ".join(missing)
        )
    return ordered


# ── Clang 调用 ──────────────────────────────────────────────────────────────
class Clang:
    def __init__(self, exe: str, include_dirs: list[str]):
        self.exe = exe
        self.include_dirs = include_dirs

    def _base(self, header: Path) -> list[str]:
        cmd = [self.exe, "-x", "c++", "-std=c++20", "-fsyntax-only", "-DGLM_ENABLE_EXPERIMENTAL"]
        for inc in self.include_dirs:
            cmd += ["-I", inc]
        cmd.append(str(header))
        return cmd

    def record_layouts(self, header: Path) -> str:
        cmd = self._base(header) + ["-Xclang", "-fdump-record-layouts-complete"]
        proc = subprocess.run(cmd, capture_output=True, text=True, errors="ignore")
        if proc.returncode != 0:
            raise GenError(f"clang 记录布局失败（{header.name}）：{proc.stderr.strip()[:800]}")
        return proc.stdout

    def record_json(self, header: Path, name: str) -> dict:
        cmd = self._base(header) + [
            "-Xclang", "-ast-dump=json",
            "-Xclang", f"-ast-dump-filter={name}",
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True, errors="ignore")
        if proc.returncode != 0:
            raise GenError(f"clang AST 失败（{header.name}:{name}）：{proc.stderr.strip()[:800]}")
        decoder = json.JSONDecoder()
        text = proc.stdout
        pos = 0
        while pos < len(text):
            while pos < len(text) and text[pos] in " \t\r\n":
                pos += 1
            if pos >= len(text):
                break
            node, pos = decoder.raw_decode(text, pos)
            record = _find_record(node, name)
            if record is not None:
                return record
        raise GenError(f"{header.name}: AST 里找不到结构 {name}")


def _find_record(node, name: str) -> dict | None:
    if not isinstance(node, dict):
        return None
    if (
        node.get("kind") == "CXXRecordDecl"
        and node.get("name") == name
        and node.get("completeDefinition")
    ):
        return node
    for child in node.get("inner", []):
        found = _find_record(child, name)
        if found is not None:
            return found
    return None


RECORD_HEAD_RE = re.compile(r"^\s*\d+ \| (?:struct|class|union) (\S+)\s*$")
RECORD_FIELD_RE = re.compile(r"^\s*(\d+) \|   (\S.*?)\s*$")
RECORD_TAIL_RE = re.compile(r"^\s*\| \[sizeof=(\d+), align=(\d+)")
RECORD_BANNER_RE = re.compile(r"^\*\*\* Dumping AST Record Layout")


def parse_layouts(text: str) -> dict[str, dict]:
    """把 `-fdump-record-layouts-complete` 的文本解析成 {结构名: {size, align, fields}}。"""
    result: dict[str, dict] = {}
    current: dict | None = None
    name: str | None = None

    for line in text.splitlines():
        if RECORD_BANNER_RE.match(line):
            current, name = None, None
            continue

        head = RECORD_HEAD_RE.match(line)
        if head:
            qualified = head.group(1)
            name = qualified.split("::")[-1]
            current = {"size": 0, "align": 0, "fields": []}
            result[name] = current
            continue

        if current is None:
            continue

        tail = RECORD_TAIL_RE.match(line)
        if tail:
            current["size"] = int(tail.group(1))
            current["align"] = int(tail.group(2))
            current, name = None, None
            continue

        fld = RECORD_FIELD_RE.match(line)
        if fld:
            offset = int(fld.group(1))
            decl = fld.group(2)
            for prefix in ("struct ", "class ", "union "):
                if decl.startswith(prefix):
                    decl = decl[len(prefix):]
            m = re.match(r"^(?P<type>.*?)\s+(?P<name>[A-Za-z_]\w*)(?P<dim>\[\w*\])?$", decl)
            if not m:
                continue
            current["fields"].append(
                {
                    "offset": offset,
                    "type": m.group("type"),
                    "name": m.group("name"),
                    "dim": m.group("dim"),
                }
            )
    return result


# ── 字段解析 ────────────────────────────────────────────────────────────────
def strip_scopes(text: str) -> str:
    text = text.strip()
    for prefix in ("const ", "class ", "struct ", "union ", "enum "):
        if text.startswith(prefix):
            text = text[len(prefix):]
    return text.strip()


def base_type_name(cpp_type: str) -> str:
    """`hgl::math::Vector4f` → `Vector4f`。"""
    text = strip_scopes(cpp_type)
    return text.split("::")[-1].strip()


def map_glsl_type(cpp_type: str, known: dict[str, str]) -> tuple[str, bool]:
    base = base_type_name(cpp_type)
    if base in known:
        return known[base], True
    if base in VECTOR_TYPES:
        return VECTOR_TYPES[base], False
    if base in MATRIX_TYPES:
        return MATRIX_TYPES[base], False
    if base in SCALAR_TYPES:
        return SCALAR_TYPES[base], False
    if base in UNSUPPORTED_SCALARS:
        raise GenError(
            f"类型 {cpp_type} 在 GLSL 需要额外存储扩展，BufferGen 不做静默降级（请显式建模为 32 位）"
        )
    raise GenError(f"未知的 GPU 字段类型：{cpp_type}（请在 buffer_gen.py 的类型表里登记）")


def collect_fields(record: dict, layout: dict, known: dict[str, str]) -> list[FieldInfo]:
    ast_fields: list[dict] = []
    for child in record.get("inner", []):
        if child.get("kind") != "FieldDecl" or child.get("isImplicit"):
            continue
        ast_fields.append(child)

    layout_fields = layout.get("fields", [])
    if len(ast_fields) != len(layout_fields):
        raise GenError(
            f"{record.get('name')}: AST 字段数 {len(ast_fields)} 与记录布局字段数 "
            f"{len(layout_fields)} 不一致（模板/位域/匿名成员不支持）"
        )

    fields: list[FieldInfo] = []
    for ast_field, layout_field in zip(ast_fields, layout_fields):
        if ast_field.get("name") != layout_field["name"]:
            raise GenError(
                f"{record.get('name')}: AST 与布局的字段顺序不一致 "
                f"({ast_field.get('name')} vs {layout_field['name']})"
            )
        qual = ast_field.get("type", {}).get("qualType", "")
        m = re.match(r"^(?P<base>.*?)\s*\[\s*(?P<dim>\w+)\s*\]$", qual)
        cpp_type = m.group("base") if m else qual
        dim = m.group("dim") if m else None
        glsl_type, nested = map_glsl_type(cpp_type, known)
        fields.append(
            FieldInfo(
                name=layout_field["name"],
                cpp_type=cpp_type,
                array_dim=dim,
                offset=layout_field["offset"],
                glsl_type=glsl_type,
                is_nested=nested,
            )
        )
    return fields


# ── GLSL 生成 ───────────────────────────────────────────────────────────────
BANNER = (
    "// ─────────────────────────────────────────────────────────────────────────────\n"
    "// @ulre-codegen begin\n"
    "//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。\n"
    "//   源：{header} → {name}\n"
    "//   布局：kind={kind} bda={bda} access={access} size={size} align={align} ref_align={ref_align}\n"
    "{note}"
    "// @ulre-codegen end\n"
    "// ─────────────────────────────────────────────────────────────────────────────\n"
)


def ref_align_of(info: StructInfo) -> int:
    return int(info.cfg.get("ref_align", DEFAULT_REF_ALIGN))


def glsl_field_line(fld: FieldInfo) -> str:
    suffix = f"[{fld.array_dim}]" if fld.array_dim else ""
    return f"    {fld.glsl_type} {fld.name}{suffix};"


def order_for_emission(structs: list[StructInfo]) -> list[StructInfo]:
    """同一个输出文件内，被引用的嵌套结构必须先声明（GLSL / C++ 都要求先定义后使用）。"""
    emitted: list[StructInfo] = []
    pending = list(structs)
    by_name = {s.name: s for s in structs}

    while pending:
        progressed = False
        for info in list(pending):
            deps = {
                base_type_name(f.cpp_type)
                for f in info.fields
                if base_type_name(f.cpp_type) in by_name
            }
            deps.discard(info.name)
            if deps <= {s.name for s in emitted}:
                emitted.append(info)
                pending.remove(info)
                progressed = True
        if not progressed:
            raise GenError(
                "输出文件内存在循环引用或缺失依赖：" + ", ".join(s.name for s in pending)
            )
    return emitted


def render_glsl_file(path: str, structs: list[StructInfo]) -> str:
    out: list[str] = []
    for index, info in enumerate(structs):
        note = info.cfg.get("note")
        note_line = f"//   说明：{note}\n" if note else ""
        out.append(
            BANNER.format(
                header=info.header,
                name=info.name,
                kind=info.kind,
                bda=info.modifiers.get("bda", "0"),
                access=info.modifiers.get("access", "readwrite"),
                align=info.align,
                size=info.size,
                ref_align=ref_align_of(info),
                note=note_line,
            )
        )

    guard_stem = path[:-5] if path.endswith(".glsl") else path
    guard = "HGL_SHADERLIB_" + re.sub(r"[^A-Za-z0-9]+", "_", guard_stem).upper().strip("_") + "_GLSL"
    out.append(f"#ifndef {guard}\n#define {guard}\n\n")

    for index, info in enumerate(structs):
        if index:
            out.append("\n")
        body = "\n".join(glsl_field_line(f) for f in info.fields)
        out.append(f"struct {info.glsl_struct}\n{{\n{body}\n}};\n")

        if info.cfg.get("emit_ref", 1) == 0:
            continue

        readonly = "readonly " if info.access_readonly else ""
        layout = (
            f"layout(buffer_reference, scalar, buffer_reference_align={ref_align_of(info)}) "
            f"{readonly}buffer {info.glsl_ref}\n"
        )
        if info.cfg.get("ref_array", 0):
            member = info.cfg.get("ref_member") or (snake(info.glsl_struct) + "s")
            out.append(f"\n{layout}{{\n    {info.glsl_struct} {member}[];\n}};\n")
        else:
            out.append(f"\n{layout}{{\n{body}\n}};\n")

    out.append(f"\n#endif // {guard}\n")
    return "".join(out)


def snake(name: str) -> str:
    text = re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()
    return text


# ── TOML 生成 ───────────────────────────────────────────────────────────────
TOML_HELPERS = r'''    namespace detail
    {
        // 所有 helper 都遵守同一条约定：**键缺失不算错误**（保留 C++ 结构的类内初始值），
        // 键存在但类型/长度不对才算错误。这正是「未给的字段用行结构的默认值」的实现方式，
        // 也是本生成器不需要解析 C++ 初始化表达式的原因。
        inline const toml::value *Find(const toml::value &table, const char *key)
        {
            if (!table.is_table())
                return nullptr;
            const auto &t = table.as_table();
            const auto it = t.find(key);
            return it == t.end() ? nullptr : &it->second;
        }

        inline bool AsFloat(const toml::value &v, float &out)
        {
            if (v.is_floating()) { out = static_cast<float>(v.as_floating()); return true; }
            if (v.is_integer())  { out = static_cast<float>(v.as_integer());  return true; }
            return false;
        }

        template <typename T>
        inline bool AsUnsigned(const toml::value &v, T &out)
        {
            if (!v.is_integer()) return false;
            const auto raw = v.as_integer();
            if (raw < 0) return false;
            out = static_cast<T>(raw);
            return true;
        }

        template <typename T>
        inline bool AsSigned(const toml::value &v, T &out)
        {
            if (!v.is_integer()) return false;
            out = static_cast<T>(v.as_integer());
            return true;
        }

        inline bool AsFloatArray(const toml::value &v, float *out, const int count)
        {
            if (!v.is_array()) return false;
            const auto &a = v.as_array();
            if (static_cast<int>(a.size()) != count) return false;
            for (int i = 0; i < count; ++i)
                if (!AsFloat(a[size_t(i)], out[i])) return false;
            return true;
        }

        template <typename T>
        inline bool AsUnsignedArray(const toml::value &v, T *out, const int count)
        {
            if (!v.is_array()) return false;
            const auto &a = v.as_array();
            if (static_cast<int>(a.size()) != count) return false;
            for (int i = 0; i < count; ++i)
                if (!AsUnsigned(a[size_t(i)], out[i])) return false;
            return true;
        }

        template <typename T>
        inline bool AsSignedArray(const toml::value &v, T *out, const int count)
        {
            if (!v.is_array()) return false;
            const auto &a = v.as_array();
            if (static_cast<int>(a.size()) != count) return false;
            for (int i = 0; i < count; ++i)
                if (!AsSigned(a[size_t(i)], out[i])) return false;
            return true;
        }

        inline toml::value FromFloat(const float v)
        {
            return toml::value(static_cast<toml::value::floating_type>(v));
        }

        template <typename T>
        inline toml::value FromInteger(const T v)
        {
            return toml::value(static_cast<toml::value::integer_type>(v));
        }

        inline toml::value FromFloatArray(const float *src, const int count)
        {
            toml::array a;
            a.reserve(size_t(count));
            for (int i = 0; i < count; ++i)
                a.push_back(FromFloat(src[i]));
            return toml::value(std::move(a));
        }

        template <typename T>
        inline toml::value FromIntegerArray(const T *src, const int count)
        {
            toml::array a;
            a.reserve(size_t(count));
            for (int i = 0; i < count; ++i)
                a.push_back(FromInteger(src[i]));
            return toml::value(std::move(a));
        }
    } // namespace detail
'''


def toml_read_field(info: StructInfo, fld: FieldInfo, indent: str) -> list[str]:
    """生成「若键存在则读取」的一段代码。"""
    if fld.is_pad:
        return []
    key = fld.name
    target = f"out.{key}"
    lines: list[str] = []
    lines.append(f'{indent}if (const toml::value *v = detail::Find(table, "{key}"))')
    lines.append(f"{indent}{{")

    glsl = fld.glsl_type
    if fld.array_dim:
        count = fld.array_dim
        if fld.is_nested:
            lines.append(f"{indent}    if (!v->is_array()) return false;")
            lines.append(f"{indent}    const auto &a = v->as_array();")
            lines.append(f"{indent}    if (a.size() != {count}) return false;")
            lines.append(f"{indent}    for (size_t i = 0; i < {count}; ++i)")
            lines.append(f"{indent}        if (!Read{base_type_name(fld.cpp_type)}(a[i], {target}[i])) return false;")
        elif glsl == "float":
            lines.append(f'{indent}    if (!detail::AsFloatArray(*v, {target}, {count})) return false;')
        elif glsl in ("uint", "uint64_t"):
            lines.append(f'{indent}    if (!detail::AsUnsignedArray(*v, {target}, {count})) return false;')
        elif glsl == "int":
            lines.append(f'{indent}    if (!detail::AsSignedArray(*v, {target}, {count})) return false;')
        else:
            raise GenError(f"{info.name}.{key}: 暂不支持 {glsl} 数组的 TOML 读写")
    elif fld.is_nested:
        lines.append(f"{indent}    if (!Read{base_type_name(fld.cpp_type)}(*v, {target})) return false;")
    elif glsl == "float":
        lines.append(f"{indent}    if (!detail::AsFloat(*v, {target})) return false;")
    elif glsl in ("vec2", "vec3", "vec4"):
        comps = {"vec2": 2, "vec3": 3, "vec4": 4}[glsl]
        lines.append(f"{indent}    if (!detail::AsFloatArray(*v, &{target}.x, {comps})) return false;")
    elif glsl == "mat4":
        lines.append(f"{indent}    if (!detail::AsFloatArray(*v, &{target}[0][0], 16)) return false;")
    elif glsl in ("uvec2", "uvec3", "uvec4"):
        comps = {"uvec2": 2, "uvec3": 3, "uvec4": 4}[glsl]
        lines.append(f"{indent}    if (!detail::AsUnsignedArray(*v, &{target}.x, {comps})) return false;")
    elif glsl in ("ivec2", "ivec3", "ivec4"):
        comps = {"ivec2": 2, "ivec3": 3, "ivec4": 4}[glsl]
        lines.append(f"{indent}    if (!detail::AsSignedArray(*v, &{target}.x, {comps})) return false;")
    elif glsl in ("uint", "uint64_t"):
        lines.append(f"{indent}    if (!detail::AsUnsigned(*v, {target})) return false;")
    elif glsl == "int":
        lines.append(f"{indent}    if (!detail::AsSigned(*v, {target})) return false;")
    elif glsl == "int64_t":
        lines.append(f"{indent}    if (!detail::AsSigned(*v, {target})) return false;")
    else:
        raise GenError(f"{info.name}.{key}: 暂不支持 {glsl} 的 TOML 读取")

    lines.append(f"{indent}}}")
    return lines


def toml_write_field(info: StructInfo, fld: FieldInfo, indent: str) -> list[str]:
    if fld.is_pad:
        return []
    key = fld.name
    src = f"in.{key}"
    glsl = fld.glsl_type
    if fld.array_dim:
        count = fld.array_dim
        if fld.is_nested:
            return [
                f"{indent}{{",
                f"{indent}    toml::array a;",
                f"{indent}    a.reserve({count});",
                f"{indent}    for (size_t i = 0; i < {count}; ++i)",
                f"{indent}        a.push_back(toml::value(Write{base_type_name(fld.cpp_type)}({src}[i])));",
                f'{indent}    t["{key}"] = toml::value(std::move(a));',
                f"{indent}}}",
            ]
        if glsl == "float":
            return [f'{indent}t["{key}"] = detail::FromFloatArray({src}, {count});']
        if glsl in ("uint", "uint64_t", "int", "int64_t"):
            return [f'{indent}t["{key}"] = detail::FromIntegerArray({src}, {count});']
        raise GenError(f"{info.name}.{key}: 暂不支持 {glsl} 数组的 TOML 写出")
    if fld.is_nested:
        return [f'{indent}t["{key}"] = toml::value(Write{base_type_name(fld.cpp_type)}({src}));']
    if glsl == "float":
        return [f'{indent}t["{key}"] = detail::FromFloat({src});']
    if glsl in ("vec2", "vec3", "vec4"):
        comps = {"vec2": 2, "vec3": 3, "vec4": 4}[glsl]
        return [f'{indent}t["{key}"] = detail::FromFloatArray(&{src}.x, {comps});']
    if glsl == "mat4":
        return [f'{indent}t["{key}"] = detail::FromFloatArray(&{src}[0][0], 16);']
    if glsl in ("uvec2", "uvec3", "uvec4"):
        comps = {"uvec2": 2, "uvec3": 3, "uvec4": 4}[glsl]
        return [f'{indent}t["{key}"] = detail::FromIntegerArray(&{src}.x, {comps});']
    if glsl in ("ivec2", "ivec3", "ivec4"):
        comps = {"ivec2": 2, "ivec3": 3, "ivec4": 4}[glsl]
        return [f'{indent}t["{key}"] = detail::FromIntegerArray(&{src}.x, {comps});']
    if glsl in ("uint", "uint64_t", "int", "int64_t"):
        return [f'{indent}t["{key}"] = detail::FromInteger({src});']
    raise GenError(f"{info.name}.{key}: 暂不支持 {glsl} 的 TOML 写出")


def render_toml_header(structs: list[StructInfo]) -> str:
    includes = sorted(
        {
            s.header[len("inc/"):] if s.header.startswith("inc/") else s.header
            for s in structs
            if s.kind != "cpu_only"
        }
    )
    names = [s for s in structs if s.kind != "cpu_only"]

    out: list[str] = []
    out.append(
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "// @ulre-codegen begin\n"
        "//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。\n"
        "//   源：inc/hgl/graph/ubo/*.h、inc/hgl/graph/ssbo/*.h 的 HGL_UBO* / HGL_SSBO* 结构\n"
        "//   说明：读写语义 = 键缺失保留 C++ 类内初始值；键存在但类型/长度不符即失败。\n"
        "//         命名以 `_pad` / `reserved` 开头的字段是对齐填充，不进 TOML。\n"
        "// @ulre-codegen end\n"
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "#pragma once\n\n"
        "#include <toml/toml.hpp>\n"
    )
    for inc in includes:
        out.append(f"#include <{inc}>\n")
    out.append("\nnamespace hgl::graph::generated\n{\n")
    out.append(TOML_HELPERS)
    out.append("\n")

    for info in names:
        out.append(f"    inline bool Read{info.name}(const toml::value &table, {info.fq} &out);\n")
    for info in names:
        out.append(f"    inline toml::value Write{info.name}(const {info.fq} &in);\n")
    out.append("\n")

    for info in names:
        out.append(f"    inline bool Read{info.name}(const toml::value &table, {info.fq} &out)\n")
        out.append("    {\n")
        out.append("        if (!table.is_table())\n            return false;\n")
        for fld in info.fields:
            for line in toml_read_field(info, fld, "        "):
                out.append(line + "\n")
        out.append("        return true;\n    }\n\n")

    for info in names:
        out.append(f"    inline toml::value Write{info.name}(const {info.fq} &in)\n")
        out.append("    {\n")
        out.append("        toml::table t;\n")
        for fld in info.fields:
            for line in toml_write_field(info, fld, "        "):
                out.append(line + "\n")
        out.append("        return toml::value(std::move(t));\n    }\n\n")

    out.append("} // namespace hgl::graph::generated\n")
    return "".join(out)


# ── FlatBuffers 生成 ────────────────────────────────────────────────────────
def fbs_type(fld: FieldInfo) -> str:
    glsl = fld.glsl_type
    table = {
        "float": "float",
        "double": "double",
        "uint": "uint",
        "uint64_t": "ulong",
        "int": "int",
        "int64_t": "long",
        "vec2": "[float:2]",
        "vec3": "[float:3]",
        "vec4": "[float:4]",
        "uvec2": "[uint:2]",
        "uvec3": "[uint:3]",
        "uvec4": "[uint:4]",
        "ivec2": "[int:2]",
        "ivec3": "[int:3]",
        "ivec4": "[int:4]",
        "mat4": "[float:16]",
    }
    if glsl in table:
        return table[glsl]
    raise GenError(f"FlatBuffers: 暂不支持 {glsl}")


def render_fbs(structs: list[StructInfo]) -> str:
    names = [s for s in structs if s.kind != "cpu_only"]
    fixed = {s.name for s in names if s.kind == "struct"}

    def fbs_field_type(fld: FieldInfo) -> str:
        if fld.is_nested or base_type_name(fld.cpp_type) in fixed:
            return base_type_name(fld.cpp_type)
        return fbs_type(fld)

    # FlatBuffers 不允许 table 里出现定长数组（必须包在 struct 里）⇒
    # 为 table 用到的每种 (元素类型, 长度) 生成一个包装 struct。
    wrappers: dict[tuple[str, str], str] = {}

    def wrapper_name(elem: str, count: str) -> str:
        key = (elem, count)
        if key not in wrappers:
            camel = {"float": "Float", "uint": "UInt", "int": "Int",
                     "ulong": "ULong", "long": "Long", "double": "Double"}.get(elem, elem)
            wrappers[key] = f"{camel}{count}"
        return wrappers[key]

    table_lines: list[tuple[StructInfo, list[str]]] = []
    for info in names:
        if info.kind == "struct":
            continue
        lines: list[str] = []
        for fld in info.fields:
            if fld.is_pad:
                continue
            if fld.array_dim:
                lines.append(f"  {fld.name}: {wrapper_name(fbs_field_type(fld), fld.array_dim)};\n")
                continue
            decl = fbs_field_type(fld)
            if decl.startswith("[") and decl.endswith("]"):
                # 向量 / 矩阵映射出来是定长数组字面量 ⇒ 同样要包进包装 struct。
                elem, count = decl[1:-1].split(":")
                decl = wrapper_name(elem, count)
            lines.append(f"  {fld.name}: {decl};\n")
        table_lines.append((info, lines))

    out: list[str] = []
    out.append(
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "// @ulre-codegen begin\n"
        "//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。\n"
        "//   源：inc/hgl/graph/ubo/*.h、inc/hgl/graph/ssbo/*.h 的 HGL_UBO* / HGL_SSBO* 结构\n"
        "//   说明：嵌套 GPU 结构（HGL_GPU_STRUCT）→ FlatBuffers `struct`（定长、可内联）；\n"
        "//         块 / 行结构 → `table`。字段名与 C++ 逐字同名（便于双向对照）。\n"
        "//         `_pad*` / `reserved*` 是对齐填充，不进 schema（序列化不需要它）。\n"
        "//         FlatBuffers 不允许 table 含定长数组 ⇒ 数组字段用本文件生成的包装 struct。\n"
        "// @ulre-codegen end\n"
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "\n"
        "namespace hgl.graph.generated;\n\n"
    )

    for info in names:
        if info.kind != "struct":
            continue
        out.append(f"// {info.header} → {info.name}（嵌套 GPU 结构）\n")
        out.append(f"struct {info.name}\n{{\n")
        for fld in info.fields:
            if fld.is_pad:
                continue
            if fld.array_dim:
                out.append(f"  {fld.name}: [{fbs_field_type(fld)}:{fld.array_dim}];\n")
            else:
                out.append(f"  {fld.name}: {fbs_field_type(fld)};\n")
        out.append("}\n\n")

    if wrappers:
        out.append("// ── table 数组字段的定长包装（元素类型 + 长度各一份） ──\n")
        for (elem, count), name in wrappers.items():
            out.append(f"struct {name}\n{{\n  v: [{elem}:{count}];\n}}\n\n")

    for info, lines in table_lines:
        out.append(f"// {info.header} → {info.name}\n")
        out.append(f"table {info.name}\n{{\n")
        out.extend(lines)
        out.append("}\n\n")

    return "".join(out)


# ── 主流程 ──────────────────────────────────────────────────────────────────
def build_structs(repo_root: Path, clang: Clang, ordered: list[StructInfo]) -> None:
    layout_cache: dict[str, dict] = {}
    known: dict[str, str] = {}

    for info in ordered:
        if info.kind in ("cpu_only",):
            continue
        if info.name in known:
            continue
        known[info.name] = info.glsl_struct

    FQ.clear()
    for info in ordered:
        FQ[info.name] = info.fq

    for info in ordered:
        if info.kind in ("cpu_only",):
            continue
        header = repo_root / info.header
        if info.header not in layout_cache:
            layout_cache[info.header] = parse_layouts(clang.record_layouts(header))
        layouts = layout_cache[info.header]
        if info.name not in layouts:
            raise GenError(f"{info.header}: 记录布局里找不到 {info.name}")
        layout = layouts[info.name]
        if layout["size"] == 0:
            raise GenError(f"{info.name}: 记录布局尺寸为 0（结构未被定义？）")
        info.size = layout["size"]
        info.align = layout["align"]
        record = clang.record_json(header, info.name)
        info.fields = collect_fields(record, layout, known)
        bda, access = ANNOT_META[info.modifiers["macro"]]
        info.modifiers.setdefault("bda", str(bda))
        info.modifiers.setdefault("access", access)


def write_or_check(path: Path, content: str, check_only: bool, report: list[str]) -> None:
    current = None
    if path.exists():
        current = path.read_text(encoding="utf-8", errors="ignore")
        if current.startswith("\ufeff"):
            current = current[1:]
    if current == content:
        return
    if check_only:
        first_diff = 0
        if current is not None:
            limit = min(len(current), len(content))
            while first_diff < limit and current[first_diff] == content[first_diff]:
                first_diff += 1
        report.append(
            f"{path}: 与生成结果不一致（首个差异偏移 {first_diff}；指纹 "
            f"on-disk={len(current) if current is not None else 'missing'} / gen={len(content)}）"
        )
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="UBO/SSBO 派生代码生成器（Clang AST 驱动）")
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--config", default="src/Tools/BufferGen/buffer_gen.toml")
    parser.add_argument("--clang", default=os.environ.get("HGL_CLANG", "clang++"))
    parser.add_argument("--include-dir", action="append", default=[])
    parser.add_argument("--check", action="store_true", help="只比对不落盘")
    parser.add_argument("--write", action="store_true", help="生成并落盘（默认行为）")
    args = parser.parse_args(argv)

    repo_root = Path(args.repo_root).resolve()
    config_path = repo_root / args.config
    if not config_path.is_file():
        raise GenError(f"找不到配置：{config_path}")
    config = tomllib.loads(config_path.read_text(encoding="utf-8"))

    include_dirs = [str((repo_root / d).resolve()) for d in DEFAULT_INCLUDE_DIRS]
    env_extra = os.environ.get("HGL_BUFFERGEN_INCLUDE_DIRS", "")
    for extra in env_extra.split(os.pathsep):
        if extra.strip():
            include_dirs.append(extra.strip())
    include_dirs.extend(args.include_dir)

    structs = apply_config(discover(repo_root), config)
    clang = Clang(args.clang, include_dirs)
    build_structs(repo_root, clang, structs)

    # 按输出文件分组（config 顺序即文件内声明顺序）
    files: dict[str, list[StructInfo]] = {}
    for info in structs:
        target = info.cfg.get("file")
        if not target:
            continue
        files.setdefault(target, []).append(info)

    report: list[str] = []
    check_only = args.check

    for rel, group in files.items():
        content = render_glsl_file(rel, order_for_emission(group))
        write_or_check(repo_root / "ShaderLibrary" / rel, content, check_only, report)

    gpu_structs = [s for s in structs if s.kind != "cpu_only"]
    write_or_check(
        repo_root / "inc/hgl/graph/generated/BufferToml.gen.h",
        render_toml_header(gpu_structs),
        check_only,
        report,
    )
    write_or_check(
        repo_root / "inc/hgl/graph/generated/buffer_schema.fbs",
        render_fbs(gpu_structs),
        check_only,
        report,
    )

    if report:
        for line in report:
            print(f"[BUFFERGEN] {line}", file=sys.stderr)
        print(
            f"[BUFFERGEN] 共 {len(report)} 个产物与源不一致；改完 C++ 结构后请重新运行本工具。",
            file=sys.stderr,
        )
        return 1

    for info in gpu_structs:
        print(
            f"[BUFFERGEN] {info.name}: kind={info.kind} size={info.size} align={info.align} "
            f"fields={len(info.fields)}"
        )
    print(f"[BUFFERGEN] OK：{len(files)} 个 GLSL 文件 + BufferToml.gen.h + buffer_schema.fbs")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except GenError as error:
        print(f"[BUFFERGEN] ERROR: {error}", file=sys.stderr)
        raise SystemExit(2)
