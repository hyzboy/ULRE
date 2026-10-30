#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AssetGen —— 资产对象派生代码生成器

唯一真源：`inc/hgl/**` 里带 `HGL_ASSET_STRUCT` / `HGL_ASSET_CLASS` 注解的 C++ 结构。
注解只表达「这个结构是资产规约」，字段清单、类型、顺序、偏移、类内缺省值全部由
Clang AST + 记录布局自动提取 —— **不需要逐字段标注**。`HGL_PROPERTY(...)` 是可选的
编辑器/文档元数据增强。

产出（一律提交进仓库，MSVC 构建不需要 Clang）：

    inc/hgl/asset/generated/<Group>Toml.gen.h            TOML 读写
    inc/hgl/asset/generated/<Group>Meta.gen.h            属性元数据表（GUI / 文档）
    inc/hgl/asset/generated/<group>_schema.fbs           FlatBuffers schema
    inc/hgl/asset/generated/<group>_schema_generated.h   flatc 产物（发行版二进制格式）
    inc/hgl/asset/generated/<Group>Flat.gen.h            FlatBuffers 编解码桥

用法
----
    python src/Tools/AssetGen/asset_gen.py --repo-root . --write
    python src/Tools/AssetGen/asset_gen.py --repo-root . --check

`--check` 只比对不落盘：有差异则打印首个差异并返回 1（CI / 门禁用）。
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - py<3.11
    import tomli as tomllib  # type: ignore

DEFAULT_INCLUDE_DIRS = ("inc", "CMCoreType/inc", "CMCore/inc", "CMMath/inc")

# `struct HGL_ASSET_STRUCT Name` / `class HGL_ASSET_CLASS Name`
ANNOT_RE = re.compile(
    r"\b(?P<kw>struct|class)\s+HGL_ASSET_(?P<kind>STRUCT|CLASS)\s+(?P<name>[A-Za-z_]\w*)"
)


class GenError(RuntimeError):
    pass


# ── 类型表 ──────────────────────────────────────────────────────────────────
# C++ 基础类型 → (PropertyKind, 标量类, FlatBuffers 标量)
SCALAR_TYPES: dict[str, tuple[str, str, str]] = {
    "bool":     ("Bool",   "bool",  "bool"),
    "uint8_t":  ("UInt8",  "uint",  "ubyte"),
    "uint16_t": ("UInt16", "uint",  "ushort"),
    "uint32_t": ("UInt32", "uint",  "uint"),
    "uint64_t": ("UInt64", "uint",  "ulong"),
    "uint8":    ("UInt8",  "uint",  "ubyte"),
    "uint16":   ("UInt16", "uint",  "ushort"),
    "uint32":   ("UInt32", "uint",  "uint"),
    "uint64":   ("UInt64", "uint",  "ulong"),
    "uint":     ("UInt32", "uint",  "uint"),
    "int8_t":   ("Int8",   "int",   "byte"),
    "int16_t":  ("Int16",  "int",   "short"),
    "int32_t":  ("Int32",  "int",   "int"),
    "int64_t":  ("Int64",  "int",   "long"),
    "int8":     ("Int8",   "int",   "byte"),
    "int16":    ("Int16",  "int",   "short"),
    "int32":    ("Int32",  "int",   "int"),
    "int64":    ("Int64",  "int",   "long"),
    "int":      ("Int32",  "int",   "int"),
    "float":    ("Float",  "float", "float"),
    "double":   ("Double", "float", "double"),
}

STRING_TYPES = {"AnsiString", "U8String", "UTF8String", "OSString"}

# 名字 → (分量数, 标量类)
VECTOR_TYPES: dict[str, tuple[int, str]] = {
    "Vector2f": (2, "float"), "Vector3f": (3, "float"), "Vector4f": (4, "float"),
    "Color3f":  (3, "float"), "Color4f":  (4, "float"),
    "Vector2u": (2, "uint"),  "Vector3u": (3, "uint"),  "Vector4u": (4, "uint"),
    "Vector2i": (2, "int"),   "Vector3i": (3, "int"),   "Vector4i": (4, "int"),
}

CONTAINER_TYPES = {"ValueArray": "ValueArray", "ManagedArray": "PtrArray"}

# `HGL_PROPERTY(...)` 里不带参数的行为标签 → 旗标名
FLAG_TAGS = {
    "EditAnywhere": "PF_EditAnywhere",
    "EditDefaultsOnly": "PF_EditDefaultsOnly",
    "VisibleAnywhere": "PF_VisibleAnywhere",
    "ReadOnly": "PF_ReadOnly",
    "EditOnly": "PF_EditOnly",
    "Transient": "PF_Transient",
}

PF_BITS = {
    "PF_EditAnywhere": 1 << 0,
    "PF_EditDefaultsOnly": 1 << 1,
    "PF_VisibleAnywhere": 1 << 2,
    "PF_ReadOnly": 1 << 3,
    "PF_EditOnly": 1 << 4,
    "PF_Transient": 1 << 5,
    "PF_HasRange": 1 << 6,
    "PF_HasClamp": 1 << 7,
    "PF_HasStep": 1 << 8,
    "PF_HasAssetRef": 1 << 9,
    "PF_Serialized": 1 << 10,
    "PF_SerializedToml": 1 << 11,
    "PF_SerializedFlatBuffers": 1 << 12,
}


def strip_scopes(text: str) -> str:
    text = text.strip()
    for prefix in ("const ", "class ", "struct ", "union ", "enum "):
        if text.startswith(prefix):
            text = text[len(prefix):]
    return text.strip()


def base_type_name(cpp_type: str) -> str:
    """`hgl::math::Vector4f` → `Vector4f`；`hgl::ValueArray<T>` → `ValueArray`。"""
    text = strip_scopes(cpp_type)
    if "<" in text:
        text = text[: text.index("<")]
    return text.split("::")[-1].strip()


def inner_template_args(cpp_type: str) -> list[str]:
    text = strip_scopes(cpp_type)
    if "<" not in text or not text.rstrip().endswith(">"):
        return []
    inner = text[text.index("<") + 1: text.rstrip().rindex(">")]
    args: list[str] = []
    depth = 0
    start = 0
    for i, ch in enumerate(inner):
        if ch in "<[(":
            depth += 1
        elif ch in ">])":
            depth -= 1
        elif ch == "," and depth == 0:
            args.append(inner[start:i].strip())
            start = i + 1
    tail = inner[start:].strip()
    if tail:
        args.append(tail)
    return args


# ── 数据结构 ────────────────────────────────────────────────────────────────
@dataclass
class TypeDesc:
    category: str       # scalar|string|vector|enum|struct|array|value_array|ptr_array
    kind: str = ""      # PropertyKind 名
    cpp: str = ""       # 基础 C++ 类型（去命名空间）
    fq: str = ""        # 完全限定 C++ 类型
    fbs: str = ""       # FlatBuffers 元素类型
    scalar_class: str = ""   # float|uint|int|bool
    components: int = 0
    elem: "TypeDesc | None" = None
    dim: str | None = None
    enum: str | None = None
    gen: str = ""       # 生成函数名后缀（struct 用）


@dataclass
class PropertyInfo:
    name: str
    toml_key: str
    display_name: str
    category: str
    tooltip: str
    cpp_type: str
    offset: int
    size: int
    type: TypeDesc
    flags: int = 0
    range_min: float = 0.0
    range_max: float = 0.0
    step: float = 0.0
    asset_ref: str | None = None
    enum_name: str | None = None

    @property
    def serialized(self) -> bool:
        return (self.flags & PF_BITS["PF_Transient"]) == 0

    @property
    def serialized_toml(self) -> bool:
        return self.serialized

    @property
    def serialized_flatbuffers(self) -> bool:
        return self.serialized and (self.flags & PF_BITS["PF_EditOnly"]) == 0


@dataclass
class AssetStruct:
    name: str
    kind: str
    header: str
    ns: str
    short_name: str
    fq: str
    gen_name: str
    doc: str = ""
    fields: list[PropertyInfo] = field(default_factory=list)
    size: int = 0
    align: int = 0
    inline_nested: bool = False
    asset_kind: str = ""
    group: str = ""


@dataclass
class EnumInfo:
    name: str
    fq: str
    values: list[tuple[str, int]] = field(default_factory=list)
    # 与 `values` 位置对应的 TOML 拼写别名（可选）
    aliases: list[str] = field(default_factory=list)

    def toml_names(self) -> list[str]:
        return [
            self.aliases[i] if i < len(self.aliases) else self.values[i][0]
            for i in range(len(self.values))
        ]


@dataclass
class GroupConfig:
    name: str
    namespace: str
    scan: list[str]
    toml_header: str
    meta_header: str
    fbs_schema: str
    fbs_generated: str
    flat_header: str
    fbs_namespace: str = "hgl.asset.generated"


# ── Clang 调用 ──────────────────────────────────────────────────────────────
class Clang:
    def __init__(self, exe: str, include_dirs: list[str]):
        self.exe = exe
        self.include_dirs = include_dirs

    def _base(self, header: Path) -> list[str]:
        cmd = [self.exe, "-x", "c++", "-std=c++20", "-fsyntax-only",
               "-DGLM_ENABLE_EXPERIMENTAL", "-D_CRT_SECURE_NO_WARNINGS", "-w"]
        for inc in self.include_dirs:
            cmd += ["-I", inc]
        cmd.append(str(header))
        return cmd

    def record_layouts(self, header: Path) -> str:
        cmd = self._base(header) + ["-Xclang", "-fdump-record-layouts-complete"]
        proc = subprocess.run(cmd, capture_output=True, text=True, errors="ignore")
        if proc.returncode != 0:
            raise GenError(f"clang 记录布局失败（{header.name}）：{proc.stderr.strip()[:900]}")
        return proc.stdout

    def ast_json(self, header: Path, name: str) -> dict | None:
        cmd = self._base(header) + [
            "-Xclang", "-ast-dump=json",
            "-Xclang", f"-ast-dump-filter={name}",
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True, errors="ignore")
        if proc.returncode != 0:
            raise GenError(f"clang AST 失败（{header.name}:{name}）：{proc.stderr.strip()[:900]}")
        decoder = json.JSONDecoder()
        text = proc.stdout
        pos = 0
        while pos < len(text):
            while pos < len(text) and text[pos] in " \t\r\n":
                pos += 1
            if pos >= len(text):
                break
            node, pos = decoder.raw_decode(text, pos)
            found = _find_node(node, name, ("CXXRecordDecl", "EnumDecl"))
            if found is not None:
                return found
        return None


def _find_node(node, name: str, kinds: tuple[str, ...]) -> dict | None:
    if not isinstance(node, dict):
        return None
    if node.get("kind") in kinds and node.get("name") == name:
        # EnumDecl 的 JSON 没有 `completeDefinition` 字段（枚举天然完整）；
        # CXXRecordDecl 需要它来排除前置声明。
        if node.get("kind") == "EnumDecl" or node.get("completeDefinition"):
            return node
    for child in node.get("inner", []):
        found = _find_node(child, name, kinds)
        if found is not None:
            return found
    return None


RECORD_HEAD_RE = re.compile(r"^\s*\d+ \| (?:struct|class|union) (\S+)\s*$")
RECORD_FIELD_RE = re.compile(r"^\s*(\d+) \|   (\S.*?)\s*$")
RECORD_TAIL_RE = re.compile(r"^\s*\| \[sizeof=(\d+), align=(\d+)")
RECORD_BANNER_RE = re.compile(r"^\*\*\* Dumping AST Record Layout")


def parse_layouts(text: str) -> dict[str, dict]:
    """`-fdump-record-layouts-complete` → {限定名: {...}}，并注册短名别名。"""
    result: dict[str, dict] = {}
    current: dict | None = None

    for line in text.splitlines():
        if RECORD_BANNER_RE.match(line):
            current = None
            continue

        head = RECORD_HEAD_RE.match(line)
        if head:
            qualified = head.group(1)
            current = {"size": 0, "align": 0, "fields": [], "qualified": qualified}
            result[qualified] = current
            result.setdefault(qualified.split("::")[-1], current)
            continue

        if current is None:
            continue

        tail = RECORD_TAIL_RE.match(line)
        if tail:
            current["size"] = int(tail.group(1))
            current["align"] = int(tail.group(2))
            current = None
            continue

        fld = RECORD_FIELD_RE.match(line)
        if fld:
            decl = fld.group(2)
            for prefix in ("struct ", "class ", "union "):
                if decl.startswith(prefix):
                    decl = decl[len(prefix):]
            m = re.match(r"^(?P<type>.*?)\s+(?P<name>[A-Za-z_]\w*)(?P<dim>\[\w*\])?$", decl)
            if m:
                current["fields"].append(
                    {"offset": int(fld.group(1)), "type": m.group("type"),
                     "name": m.group("name"), "dim": m.group("dim")}
                )
    return result


# ── 源文本辅助（属性标签 / 文档注释）────────────────────────────────────────
def read_lines(path: Path) -> list[str]:
    return path.read_text(encoding="utf-8", errors="ignore").splitlines()


def balanced_parens(text: str, start: int) -> str | None:
    if start >= len(text) or text[start] != "(":
        return None
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return text[start + 1:i]
    return None


def split_top_level(text: str) -> list[str]:
    parts: list[str] = []
    depth = 0
    start = 0
    in_string = False
    for i, ch in enumerate(text):
        if in_string:
            if ch == '"' and (i == 0 or text[i - 1] != "\\"):
                in_string = False
            continue
        if ch == '"':
            in_string = True
        elif ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == "," and depth == 0:
            parts.append(text[start:i].strip())
            start = i + 1
    tail = text[start:].strip()
    if tail:
        parts.append(tail)
    return parts


TOOLTIP_TAIL_RE = re.compile(r"///<+\s*(.*)$")


def line_tail_doc(line: str) -> str:
    m = TOOLTIP_TAIL_RE.search(line)
    return m.group(1).strip() if m else ""


def find_property_annotation(lines: list[str], line_no: int) -> tuple[list[str], str] | None:
    """取本字段声明里的 `HGL_PROPERTY(...)`。

    `HGL_PROPERTY(...)` 可以写在字段同一行的开头，也可以独占一行写在字段上方，
    因此从字段行开始**向上**收集，遇到 `;` / `{` / `}` 结尾的行（上一条声明或块的
    边界）就停 —— 这样既支持多行写法，又不会把上一条字段的属性标签抢过来。
    """
    if line_no <= 0 or line_no > len(lines):
        return None

    collected: list[str] = []
    for probe in range(line_no - 1, max(-1, line_no - 10), -1):
        line = lines[probe]
        collected.insert(0, line)
        joined = "\n".join(collected)

        if "HGL_PROPERTY" in joined:
            at = joined.find("HGL_PROPERTY")
            open_at = joined.find("(", at)
            body = balanced_parens(joined, open_at) if open_at >= 0 else None
            if body is not None:
                return split_top_level(body), line_tail_doc(lines[line_no - 1])

        # 字段自己那行天然以 `;` 结束（或带 `///<` 尾注释），不能当作边界；
        # 边界只对**字段行之上**的行有意义。
        if probe < line_no - 1 and line.rstrip().endswith((";", "{", "}")):
            break
    return None


def doc_above(lines: list[str], line_no: int) -> str:
    if line_no <= 1:
        return ""
    collected: list[str] = []
    idx = line_no - 2
    in_block = False
    while idx >= 0:
        text = lines[idx].strip()
        if not in_block:
            if text.endswith("*/"):
                in_block = True
                collected.append(text[:-2].strip().lstrip("/*").strip())
            elif text.startswith("///"):
                collected.append(text[3:].strip())
            elif text.startswith("//"):
                collected.append(text[2:].strip())
            else:
                break
        else:
            collected.append(text.lstrip("/*").strip())
            if text.startswith("/*"):
                break
        idx -= 1
    return " ".join(reversed([c for c in collected if c])).strip()


def apply_property_tags(prop: PropertyInfo, tags: list[str], tail_doc: str) -> None:
    if tail_doc and not prop.tooltip:
        prop.tooltip = tail_doc
    for tag in tags:
        tag = tag.strip()
        if not tag:
            continue
        if "=" in tag:
            key, value = tag.split("=", 1)
            key = key.strip()
            value = value.strip().strip('"')
            if key == "Category":
                prop.category = value
            elif key == "DisplayName":
                prop.display_name = value
            elif key == "Tooltip":
                prop.tooltip = value
            elif key == "Key":
                prop.toml_key = value
            elif key == "AssetRef":
                prop.asset_ref = value
                prop.flags |= PF_BITS["PF_HasAssetRef"]
            elif key == "Step":
                prop.step = _parse_finite_number(value, prop.name, key)
                if prop.step <= 0.0:
                    raise GenError(f"{prop.name}: Step 必须大于 0")
                prop.flags |= PF_BITS["PF_HasStep"]
            else:
                raise GenError(f"{prop.name}: 未知的属性标签 {key}")
            continue
        if "(" in tag:
            fname = tag[: tag.index("(")].strip()
            body = tag[tag.index("(") + 1: tag.rindex(")")] if ")" in tag else ""
            args = [a.strip() for a in body.split(",") if a.strip()]
            if fname in ("Range", "Clamp"):
                if len(args) != 2:
                    raise GenError(f"{prop.name}: {fname} 需要两个参数")
                prop.range_min = _parse_finite_number(args[0], prop.name, fname)
                prop.range_max = _parse_finite_number(args[1], prop.name, fname)
                if prop.range_min > prop.range_max:
                    raise GenError(f"{prop.name}: {fname} 下限不能大于上限")
                prop.flags |= PF_BITS["PF_HasRange" if fname == "Range" else "PF_HasClamp"]
            else:
                raise GenError(f"{prop.name}: 未知的属性标签 {fname}")
            continue
        if tag in FLAG_TAGS:
            prop.flags |= PF_BITS[FLAG_TAGS[tag]]
            continue
        raise GenError(f"{prop.name}: 未知的属性标签 {tag}")


def _parse_finite_number(text: str, prop_name: str, tag_name: str) -> float:
    try:
        value = float(text)
    except ValueError as error:
        raise GenError(f"{prop_name}: {tag_name} 参数不是有效数字：{text}") from error
    if not math.isfinite(value):
        raise GenError(f"{prop_name}: {tag_name} 参数必须是有限数字")
    return value


# ── 字段分类 ────────────────────────────────────────────────────────────────
class TypeRegistry:
    def __init__(self) -> None:
        self.structs: dict[str, AssetStruct] = {}
        self.enums: dict[str, EnumInfo] = {}
        self.integer_enums: set[str] = set()

    def classify(self, cpp_type: str) -> TypeDesc:
        raw = strip_scopes(cpp_type)
        m = re.match(r"^(?P<base>.*?)\s*\[\s*(?P<dim>\w+)\s*\]$", raw)
        if m:
            dim = m.group("dim")
            if not dim.isdigit() or int(dim) <= 0:
                raise GenError(f"定长数组长度必须是正整数常量：{cpp_type}")
            elem = self.classify(m.group("base"))
            if elem.category not in ("scalar", "string", "enum", "struct"):
                raise GenError(f"定长数组的元素类型不受支持：{cpp_type}")
            return TypeDesc(category="array", cpp=base_type_name(cpp_type),
                            fq=m.group("base").strip(), elem=elem, dim=dim,
                            components=elem.components)

        base = base_type_name(raw)

        if base in SCALAR_TYPES:
            kind, scalar_class, fbs = SCALAR_TYPES[base]
            return TypeDesc(category="scalar", kind=kind, cpp=base, fq=raw,
                            fbs=fbs, scalar_class=scalar_class)

        if base in STRING_TYPES:
            return TypeDesc(category="string", kind="String", cpp=base, fq=raw, fbs="string")

        if base in VECTOR_TYPES:
            components, scalar_class = VECTOR_TYPES[base]
            kind = {2: "Vector2f", 3: "Vector3f", 4: "Vector4f"}[components]
            return TypeDesc(category="vector", kind=kind, cpp=base, fq=raw,
                            fbs=_fbs_vec_type(scalar_class, components),
                            scalar_class=scalar_class, components=components)

        if base in CONTAINER_TYPES:
            args = inner_template_args(raw)
            if len(args) != 1:
                raise GenError(f"容器类型必须有且只有一个模板参数：{cpp_type}")
            elem = self.classify(args[0])
            return TypeDesc(category="value_array" if base == "ValueArray" else "ptr_array",
                            kind=CONTAINER_TYPES[base], cpp=base, fq=raw, elem=elem,
                            components=elem.components)

        if base in self.structs:
            target = self.structs[base]
            return TypeDesc(category="struct", kind="Struct", cpp=base, fq=target.fq,
                            fbs=target.gen_name, gen=target.gen_name)

        if base in self.enums:
            info = self.enums[base]
            return TypeDesc(category="enum", kind="Enum", cpp=base, fq=info.fq, fbs="int",
                            enum=info.fq)

        if base in self.integer_enums:
            return TypeDesc(category="enum", kind="Enum", cpp=base, fq=raw, fbs="int")

        raise GenError(
            f"未知的资产字段类型：{cpp_type}（若是外部枚举请在 asset_gen.toml 的 "
            f"integer_enums 里登记；若是结构请加 HGL_ASSET_STRUCT 注解）"
        )


def _fbs_vec_type(scalar_class: str, components: int) -> str:
    prefix = {"float": "Float", "uint": "UInt", "int": "Int"}[scalar_class]
    return f"{prefix}{components}"


# ── 元数据渲染 ──────────────────────────────────────────────────────────────
def c_escape(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


def render_meta_header(group: GroupConfig, structs: list[AssetStruct]) -> str:
    out: list[str] = []
    out.append(
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "// @ulre-codegen begin\n"
        "//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。\n"
        "//   属性元数据表：供检查器 GUI、文档导出与通用批量工具消费。\n"
        "// @ulre-codegen end\n"
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "#pragma once\n\n"
        "#include <hgl/asset/AssetObject.h>\n"
    )
    for header in sorted({s.header for s in structs}):
        rel = header[len("inc/"):] if header.startswith("inc/") else header
        out.append(f"#include <{rel}>\n")
    out.append("\nnamespace hgl::asset::generated\n{\n")

    for info in structs:
        table = f"Meta_{info.gen_name}"
        out.append(f"    inline const PropertyMetaInfo {table}[] =\n    {{\n")
        for prop in info.fields:
            names = [name for name, bit in PF_BITS.items() if prop.flags & bit]
            if not names:
                names = ["PF_None"]
            flags_literal = " | ".join(names)
            enum_name = f'"{c_escape(prop.enum_name)}"' if prop.enum_name else "nullptr"
            asset_ref = f'"{c_escape(prop.asset_ref)}"' if prop.asset_ref else "nullptr"
            fixed_len = int(prop.type.dim) if prop.type.dim and prop.type.dim.isdigit() else 0
            kind = prop.type.kind or "String"
            out.append(
                f'        {{ "{c_escape(prop.name)}", "{c_escape(prop.toml_key)}", '
                f'"{c_escape(prop.display_name)}", "{c_escape(prop.category)}", '
                f'"{c_escape(prop.tooltip)}", "{c_escape(prop.cpp_type)}", '
                f"{enum_name}, {asset_ref}, "
                f"PropertyKind::{kind}, {flags_literal}, "
                f"{prop.offset}u, {prop.size}u, {fixed_len}u, "
                f"{prop.range_min}, {prop.range_max}, {prop.step} }},\n"
            )
        out.append("    };\n\n")
        out.append(
            f"    inline const ClassMetaInfo Class_{info.gen_name} =\n"
            f"    {{\n"
            f'        "{c_escape(info.short_name)}", "{c_escape(info.asset_kind)}", '
            f'"{c_escape(info.doc)}",\n'
            f"        {table}, (uint32_t)(sizeof({table}) / sizeof({table}[0])),\n"
            f"        {info.size}u, {info.align}u\n"
            f"    }};\n\n"
        )

    for info in structs:
        out.append(
            f"    inline const ClassMetaInfo *GetClassMeta(const {info.fq} *) "
            f"{{ return &Class_{info.gen_name}; }}\n"
        )

    out.append("} // namespace hgl::asset::generated\n")
    return "".join(out)


# ── TOML 渲染 ───────────────────────────────────────────────────────────────
def read_stmts(t: TypeDesc, value_expr: str, target: str, indent: str, where: str) -> list[str]:
    """生成「把 toml::value 表达式读进目标」的语句行。"""
    # 调用方给的多半是 `*v` 这类表达式，成员访问必须加括号，
    # 否则 `*v.is_string()` 会被解析成 `*(v.is_string())`。
    val = f"({value_expr})"
    cat = t.category

    if cat == "scalar":
        fn = {"bool": "AsBool", "float": "AsFloat",
              "uint": "AsUnsigned", "int": "AsSigned"}[t.scalar_class]
        return [f"{indent}if (!detail::{fn}({value_expr}, {target})) return false;"]

    if cat == "string":
        return [f"{indent}if (!detail::AsString({value_expr}, {target})) return false;"]

    if cat == "vector":
        fn = {"float": "AsFloatArray", "uint": "AsUnsignedArray",
              "int": "AsSignedArray"}[t.scalar_class]
        return [f"{indent}if (!detail::{fn}({value_expr}, &{target}.x, {t.components})) return false;"]

    if cat == "enum":
        lines = [f"{indent}if ({val}.is_string())", f"{indent}{{"]
        if t.enum:
            lines.append(
                f"{indent}    if (!AssetEnumParse_{t.cpp}({val}.as_string().c_str(), "
                f"{target})) return false;"
            )
        else:
            lines.append(f"{indent}    return false;")
        lines.append(f"{indent}}}")
        lines.append(f"{indent}else if ({val}.is_integer())")
        lines.append(f"{indent}{{")
        lines.append(f"{indent}    {target} = static_cast<{t.fq}>({val}.as_integer());")
        lines.append(f"{indent}}}")
        lines.append(f"{indent}else")
        lines.append(f"{indent}    return false;")
        return lines

    if cat == "struct":
        return [f"{indent}if (!Read{t.gen}({value_expr}, {target})) return false;"]

    if cat == "array":
        elem = t.elem
        count = t.dim
        if elem.category == "scalar":
            fn = {"bool": "AsBoolArray", "float": "AsFloatArray",
                  "uint": "AsUnsignedArray", "int": "AsSignedArray"}[elem.scalar_class]
            return [f"{indent}if (!detail::{fn}({value_expr}, {target}, {count})) return false;"]
        lines = [
            f"{indent}if (!{val}.is_array()) return false;",
            f"{indent}{{",
            f"{indent}    const auto &arr = {val}.as_array();",
            f"{indent}    if (arr.size() != {count}) return false;",
        ]
        if elem.category == "string":
            lines.append(f"{indent}    for (size_t i = 0; i < {count}; ++i)")
            lines.append(f"{indent}        if (!detail::AsString(arr[i], {target}[i])) return false;")
        else:
            lines.append(f"{indent}    for (size_t i = 0; i < {count}; ++i)")
            lines.append(f"{indent}    {{")
            lines += read_stmts(elem, "arr[i]", f"{target}[i]", indent + "        ", where)
            lines.append(f"{indent}    }}")
        lines.append(f"{indent}}}")
        return lines

    if cat == "value_array":
        lines = [
            f"{indent}if (!{val}.is_array()) return false;",
            f"{indent}{{",
            f"{indent}    const auto &arr = {val}.as_array();",
            f"{indent}    {target}.Clear();",
            f"{indent}    {target}.Resize((int)arr.size());",
            f"{indent}    auto *items = {target}.GetData();",
            f"{indent}    for (size_t i = 0; i < arr.size(); ++i)",
            f"{indent}    {{",
        ]
        lines += read_stmts(t.elem, "arr[i]", "items[i]", indent + "        ", where)
        lines += [f"{indent}    }}", f"{indent}}}"]
        return lines

    if cat == "ptr_array":
        lines = [
            f"{indent}if (!{val}.is_array()) return false;",
            f"{indent}{{",
            f"{indent}    const auto &arr = {val}.as_array();",
            f"{indent}    {target}.Clear();",
            f"{indent}    for (size_t i = 0; i < arr.size(); ++i)",
            f"{indent}    {{",
            f"{indent}        auto *item = {target}.Create();",
        ]
        lines += read_stmts(t.elem, "arr[i]", "*item", indent + "        ", where)
        lines += [f"{indent}    }}", f"{indent}}}"]
        return lines

    raise GenError(f"{where}: 暂不支持的读取类别 {cat}")


def write_expr(t: TypeDesc, src: str, where: str) -> str:
    """生成一个产生 `toml::value` 的 C++ 表达式。"""
    cat = t.category

    if cat == "scalar":
        fn = {"bool": "FromBool", "float": "FromFloat",
              "uint": "FromInteger", "int": "FromInteger"}[t.scalar_class]
        return f"detail::{fn}({src})"

    if cat == "string":
        return f"detail::FromString({src})"

    if cat == "vector":
        fn = "FromFloatArray" if t.scalar_class == "float" else "FromIntegerArray"
        return f"detail::{fn}(&{src}.x, {t.components})"

    if cat == "enum":
        return f"detail::FromInteger((int64_t){src})"

    if cat == "struct":
        return f"toml::value(Write{t.gen}({src}))"

    raise GenError(f"{where}: 不能直接写成单个 TOML 值的类别 {cat}")


def render_toml_header(structs: list[AssetStruct], enums: list[EnumInfo]) -> str:
    out: list[str] = []
    out.append(
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "// @ulre-codegen begin\n"
        "//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。\n"
        "//   语义：键缺失 = 保留 C++ 类内初始值；键存在但类型/长度不符 = 解析失败。\n"
        "// @ulre-codegen end\n"
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "#pragma once\n\n"
        "#include <toml/toml.hpp>\n"
        "#include <hgl/asset/AssetSerialize.h>\n"
    )
    for header in sorted({s.header for s in structs}):
        rel = header[len("inc/"):] if header.startswith("inc/") else header
        out.append(f"#include <{rel}>\n")
    out.append("\nnamespace hgl::asset::generated\n{\n")

    for info in enums:
        out.append(f"    inline const char *AssetEnumName_{info.name}(const {info.fq} v)\n    {{\n")
        out.append("        switch (v)\n        {\n")
        for index, (name, _value) in enumerate(info.values):
            toml_name = info.toml_names()[index]
            out.append(f'        case {info.fq}::{name}: return "{toml_name}";\n')
        out.append("        }\n        return nullptr;\n    }\n\n")

        out.append(f"    inline bool AssetEnumParse_{info.name}(const char *s, {info.fq} &out)\n    {{\n")
        out.append("        if (!s) return false;\n")
        for index, (name, _value) in enumerate(info.values):
            toml_name = info.toml_names()[index]
            out.append(
                f'        if (hgl::stricmp(s, "{toml_name}") == 0) '
                f"{{ out = {info.fq}::{name}; return true; }}\n"
            )
            # 别名与 C++ 名不同时，两种拼写都接受（老 TOML 与新写的都能读）。
            if toml_name != name:
                out.append(
                    f'        if (hgl::stricmp(s, "{name}") == 0) '
                    f"{{ out = {info.fq}::{name}; return true; }}\n"
                )
        out.append("        return false;\n    }\n\n")

    for info in structs:
        out.append(f"    inline bool Read{info.gen_name}(const toml::value &table, {info.fq} &out);\n")
    for info in structs:
        out.append(f"    inline toml::value Write{info.gen_name}(const {info.fq} &in);\n")
    out.append("\n")

    for info in structs:
        out.append(f"    inline bool Read{info.gen_name}(const toml::value &table, {info.fq} &out)\n    {{\n")
        out.append("        if (!table.is_table()) return false;\n")
        for prop in info.fields:
            if not prop.serialized_toml:
                continue
            out.append(f'        if (const toml::value *v = detail::Find(table, "{prop.toml_key}"))\n')
            out.append("        {\n")
            for line in read_stmts(prop.type, "*v", f"out.{prop.name}", "            ", prop.name):
                out.append(line + "\n")
            out.append("        }\n")
        out.append("        return true;\n    }\n\n")

    for info in structs:
        out.append(f"    inline toml::value Write{info.gen_name}(const {info.fq} &in)\n    {{\n")
        out.append("        toml::table t;\n")
        for prop in info.fields:
            if not prop.serialized_toml:
                continue
            src = f"in.{prop.name}"
            t = prop.type
            if t.category in ("value_array", "ptr_array"):
                elem_src = (f"{src}.GetData()[i]" if t.category == "value_array" else f"*{src}[i]")
                out.append("        {\n")
                out.append("            toml::array a;\n")
                out.append(f"            a.reserve((size_t){src}.GetCount());\n")
                out.append(f"            for (int i = 0; i < {src}.GetCount(); ++i)\n")
                out.append(f"                a.push_back({write_expr(t.elem, elem_src, prop.name)});\n")
                out.append(f'            t["{prop.toml_key}"] = toml::value(std::move(a));\n')
                out.append("        }\n")
                continue
            if t.category == "array" and t.elem.category not in ("scalar",):
                out.append("        {\n")
                out.append("            toml::array a;\n")
                out.append(f"            a.reserve({t.dim});\n")
                out.append(f"            for (size_t i = 0; i < {t.dim}; ++i)\n")
                out.append(f"                a.push_back({write_expr(t.elem, f'{src}[i]', prop.name)});\n")
                out.append(f'            t["{prop.toml_key}"] = toml::value(std::move(a));\n')
                out.append("        }\n")
                continue
            if t.category == "array":
                fn = {"bool": "FromBoolArray", "float": "FromFloatArray",
                      "uint": "FromIntegerArray", "int": "FromIntegerArray"}[t.elem.scalar_class]
                out.append(f'        t["{prop.toml_key}"] = detail::{fn}({src}, {t.dim});\n')
                continue
            if t.category == "enum" and t.enum:
                out.append("        {\n")
                out.append(f"            const char *nm = AssetEnumName_{t.cpp}({src});\n")
                out.append(f'            t["{prop.toml_key}"] = nm\n')
                out.append("                ? toml::value(toml::value::string_type(nm))\n")
                out.append(f"                : detail::FromInteger((int64_t){src});\n")
                out.append("        }\n")
                continue
            out.append(f'        t["{prop.toml_key}"] = {write_expr(t, src, prop.name)};\n')
        out.append("        return toml::value(std::move(t));\n    }\n\n")

    out.append("} // namespace hgl::asset::generated\n")
    return "".join(out)


# ── FlatBuffers schema 渲染 ─────────────────────────────────────────────────
FBS_ELEM = {"float": "float", "uint": "uint", "int": "int", "bool": "bool",
            "double": "double"}


def _wrapper_name(elem_fbs: str, count: int) -> str:
    camel = {
        "float": "Float", "double": "Double", "uint": "UInt", "int": "Int",
        "bool": "Bool", "ubyte": "UByte", "ushort": "UShort", "ulong": "ULong",
        "byte": "Byte", "short": "Short", "long": "Long",
    }.get(elem_fbs, elem_fbs)
    return f"{camel}Array{count}"


def _vec_struct_name(elem_fbs: str, count: int) -> str:
    camel = {"float": "Float", "uint": "UInt", "int": "Int"}[elem_fbs]
    return f"{camel}{count}"


def _fbs_elem_of(t: TypeDesc) -> str:
    if t.category == "scalar":
        return t.fbs
    if t.category == "enum":
        return "int"
    return ""


def collect_fbs_wrappers(structs: list[AssetStruct]) -> tuple[dict, dict]:
    """扫描所有字段，算出需要生成的向量 struct 与定长数组包装 struct。"""
    vecs: dict[tuple[str, int], str] = {}
    arrays: dict[tuple[str, int], str] = {}

    def walk(t: TypeDesc, where: str) -> None:
        if t.category == "vector":
            elem = {"float": "float", "uint": "uint", "int": "int"}[t.scalar_class]
            vecs[(elem, t.components)] = _vec_struct_name(elem, t.components)
        elif t.category == "array":
            if t.elem.category == "struct":
                arrays[(t.elem.gen, int(t.dim))] = _wrapper_name(t.elem.gen, int(t.dim))
            else:
                elem = _fbs_elem_of(t.elem)
                if not elem:
                    raise GenError(f"{where}: FlatBuffers 不支持该定长数组元素类型")
                arrays[(elem, int(t.dim))] = _wrapper_name(elem, int(t.dim))
        elif t.category in ("value_array", "ptr_array"):
            walk(t.elem, where)

    for info in structs:
        for prop in info.fields:
            if prop.serialized_flatbuffers:
                walk(prop.type, f"{info.name}.{prop.name}")
    return vecs, arrays


def render_fbs_schema(group: GroupConfig, structs: list[AssetStruct]) -> str:
    vecs, arrays = collect_fbs_wrappers(structs)

    out: list[str] = []
    out.append(
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "// @ulre-codegen begin\n"
        "//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。\n"
        "//   用途：发行版资产二进制格式。语义是「完整快照」，不是 TOML 那样的增量补丁：\n"
        "//         TOML 缺键 = 保留 C++ 类内初始值；FlatBuffers 解码一律覆盖全部字段。\n"
        "//   资产结构一律 table（可能含 string / 动态数组）；向量与定长数组用本文件\n"
        "//   生成的包装 struct（FlatBuffers 的 table 不允许定长数组字段）。\n"
        "// @ulre-codegen end\n"
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        f"\nnamespace {group.fbs_namespace};\n\n"
    )

    for (elem, count), name in sorted(arrays.items()):
        out.append(f"struct {name}\n{{\n  v:[{elem}:{count}];\n}}\n\n")

    for (elem, count), name in sorted(vecs.items()):
        out.append(f"struct {name}\n{{\n")
        for comp in ["x", "y", "z", "w"][:count]:
            out.append(f"  {comp}:{elem};\n")
        out.append("}\n\n")

    for info in structs:
        out.append(f"// {info.header} -> {info.short_name}\n")
        out.append(f"table {info.gen_name}\n{{\n")
        for prop in info.fields:
            if not prop.serialized_flatbuffers:
                continue
            t = prop.type
            if t.category in ("scalar", "enum"):
                out.append(f"  {prop.toml_key}:{_fbs_elem_of(t)};\n")
            elif t.category == "string":
                out.append(f"  {prop.toml_key}:string;\n")
            elif t.category == "struct":
                out.append(f"  {prop.toml_key}:{t.gen};\n")
            elif t.category == "vector":
                elem = {"float": "float", "uint": "uint", "int": "int"}[t.scalar_class]
                out.append(f"  {prop.toml_key}:{_vec_struct_name(elem, t.components)};\n")
            elif t.category == "array":
                if t.elem.category == "struct":
                    out.append(f"  {prop.toml_key}:{_wrapper_name(t.elem.gen, int(t.dim))};\n")
                else:
                    out.append(f"  {prop.toml_key}:{_wrapper_name(_fbs_elem_of(t.elem), int(t.dim))};\n")
            elif t.category == "value_array":
                elem = _fbs_elem_of(t.elem)
                if not elem:
                    raise GenError(
                        f"{info.name}.{prop.toml_key}: ValueArray 的 FlatBuffers 编码只支持"
                        f"标量与枚举元素（结构元素请改用 ManagedArray）"
                    )
                out.append(f"  {prop.toml_key}:[{elem}];\n")
            elif t.category == "ptr_array":
                if t.elem.category != "struct":
                    raise GenError(
                        f"{info.name}.{prop.toml_key}: ManagedArray 的 FlatBuffers 编码"
                        f"只支持资产结构元素"
                    )
                out.append(f"  {prop.toml_key}:[{t.elem.gen}];\n")
        out.append("}\n\n")

    return "".join(out)


# ── FlatBuffers 编解码桥 ────────────────────────────────────────────────────
def _fb_type(group: GroupConfig, name: str) -> str:
    return f"::hgl::asset::schema::{name}"


def _string_encode(t: TypeDesc, src: str, var: str) -> tuple[list[str], str]:
    """返回 (前置语句, CreateString 表达式)。"""
    if t.cpp == "OSString":
        return ([f"        const hgl::U8String {var}_u8 = hgl::ToU8String({src});"],
                f"fbb.CreateString(reinterpret_cast<const char *>({var}_u8.c_str()))")
    if t.cpp == "U8String":
        return ([], f"fbb.CreateString(reinterpret_cast<const char *>({src}.c_str()))")
    return ([], f"fbb.CreateString({src}.c_str())")


def _string_decode(t: TypeDesc, src: str, dst: str) -> list[str]:
    if t.cpp == "OSString":
        return [f"        if (const auto *s = {src}) {dst} = hgl::ToOSString(s->str().c_str());".replace(
            "if (const auto *s = ", "if (const ::flatbuffers::String *s = ")]
    if t.cpp == "U8String":
        return [
            f"        if (const ::flatbuffers::String *s = {src}) "
            f"{dst} = hgl::U8String(reinterpret_cast<const hgl::u8char *>(s->str().c_str()));"
        ]
    return [f"        if (const ::flatbuffers::String *s = {src}) {dst} = s->str().c_str();"]


def render_flat_header(group: GroupConfig, structs: list[AssetStruct]) -> str:
    schema_include = group.fbs_generated[len("inc/"):] if group.fbs_generated.startswith("inc/") \
        else group.fbs_generated
    out: list[str] = []
    out.append(
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "// @ulre-codegen begin\n"
        "//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。\n"
        "//   语义：**完整快照**。与 TOML 的「缺键即保留 C++ 缺省值」不同，这里的 Decode\n"
        "//         会把 schema 里存在的每个字段都写回，缺省值来自类内初始化（Transient\n"
        "//         字段不进 schema，因此保持类内缺省）。\n"
        "// @ulre-codegen end\n"
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "#pragma once\n\n"
        "#include <flatbuffers/flatbuffers.h>\n"
        "#include <hgl/utf.h>\n"
        f"#include <{schema_include}>\n"
    )
    for header in sorted({s.header for s in structs}):
        rel = header[len("inc/"):] if header.startswith("inc/") else header
        out.append(f"#include <{rel}>\n")
    out.append("\nnamespace hgl::asset::generated\n{\n")

    for info in structs:
        out.append(
            f"    inline flatbuffers::Offset<{_fb_type(group, info.gen_name)}>\n"
            f"    Encode{info.gen_name}(flatbuffers::FlatBufferBuilder &fbb, "
            f"const {info.fq} &in);\n"
        )
    for info in structs:
        out.append(
            f"    inline void Decode{info.gen_name}(const {_fb_type(group, info.gen_name)} *t, "
            f"{info.fq} &out);\n"
        )
    out.append("\n")

    for info in structs:
        pre: list[str] = []
        adds: list[str] = []
        preamble: list[str] = []

        for prop in info.fields:
            if not prop.serialized_flatbuffers:
                continue
            t = prop.type
            key = prop.toml_key
            src = f"in.{prop.name}"
            if t.category in ("scalar", "enum"):
                expr = f"static_cast<int32_t>({src})" if t.category == "enum" else src
                adds.append(f"        b.add_{key}({expr});")
            elif t.category == "string":
                var = f"_{info.gen_name}_{key}_str"
                lines, expr = _string_encode(t, src, var)
                preamble += lines
                offset_var = f"{var}_offset"
                preamble.append(f"        const auto {offset_var} = {expr};")
                adds.append(f"        b.add_{key}({offset_var});")
            elif t.category == "struct":
                var = f"_{info.gen_name}_{key}_tab"
                preamble.append(f"        const auto {var} = Encode{t.gen}(fbb, {src});")
                adds.append(f"        b.add_{key}({var});")
            elif t.category == "vector":
                var = f"_{info.gen_name}_{key}_vec"
                comps = ["x", "y", "z", "w"][:t.components]
                ctor = ", ".join(f"{src}.{c}" for c in comps)
                preamble.append(
                    f"        const {_fb_type(group, _vec_struct_name(_fbs_elem_of_scalar(t), t.components))}"
                    f" {var}({ctor});"
                )
                adds.append(f"        b.add_{key}(&{var});")
            elif t.category == "array":
                var = f"_{info.gen_name}_{key}_arr"
                if t.elem.category == "struct":
                    raise GenError(
                        f"{info.name}.{prop.name}: FlatBuffers 暂不支持「资产结构定长数组」"
                        f"（请改用 ManagedArray）"
                    )
                wrapper = _wrapper_name(_fbs_elem_of(t.elem), int(t.dim))
                preamble.append(f"        const {_fb_type(group, wrapper)} {var}({src});")
                adds.append(f"        b.add_{key}(&{var});")
            elif t.category == "value_array":
                var = f"_{info.gen_name}_{key}_dyn"
                pre.append(
                    f"fbb.CreateVector({src}.GetData(), (size_t){src}.GetCount())"
                )
                preamble.append(f"        const auto {var} = {pre[-1]};")
                pre.pop()
                adds.append(f"        b.add_{key}({var});")
            elif t.category == "ptr_array":
                var = f"_{info.gen_name}_{key}_vec"
                preamble.append(
                    f"        const auto {var} = fbb.CreateVector<flatbuffers::Offset<"
                    f"{_fb_type(group, t.elem.gen)}>>((size_t){src}.GetCount(), "
                    f"[&](size_t i, const {info.fq} *source) "
                    f"{{ return Encode{t.elem.gen}(fbb, *source->{prop.name}[(int)i]); }}, &in);"
                )
                adds.append(f"        b.add_{key}({var});")

        out.append(
            f"    inline flatbuffers::Offset<{_fb_type(group, info.gen_name)}>\n"
            f"    Encode{info.gen_name}(flatbuffers::FlatBufferBuilder &fbb, "
            f"const {info.fq} &in)\n    {{\n"
            f"        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。\n"
        )
        for line in preamble:
            out.append(line + "\n")
        out.append(f"        {_fb_type(group, info.gen_name)}Builder b(fbb);\n")
        for line in adds:
            out.append(line + "\n")
        out.append("        return b.Finish();\n    }\n\n")

    for info in structs:
        out.append(
            f"    inline void Decode{info.gen_name}(const {_fb_type(group, info.gen_name)} *t, "
            f"{info.fq} &out)\n    {{\n"
            f"        if (!t) return;\n"
        )
        for prop in info.fields:
            if not prop.serialized_flatbuffers:
                continue
            t = prop.type
            key = prop.toml_key
            dst = f"out.{prop.name}"
            if t.category in ("scalar", "enum"):
                if t.category == "enum":
                    out.append(f"        {dst} = static_cast<{t.fq}>(t->{key}());\n")
                else:
                    out.append(f"        {dst} = t->{key}();\n")
            elif t.category == "string":
                for line in _string_decode(t, f"t->{key}()", dst):
                    out.append(line + "\n")
            elif t.category == "struct":
                out.append(f"        Decode{t.gen}(t->{key}(), {dst});\n")
            elif t.category == "vector":
                comps = ["x", "y", "z", "w"][:t.components]
                out.append(f"        if (const auto *v = t->{key}())\n        {{\n")
                for comp in comps:
                    out.append(f"            {dst}.{comp} = v->{comp}();\n")
                out.append("        }\n")
            elif t.category == "array":
                wrapper = (f"_wrapper_name" if False else
                           (_wrapper_name(t.elem.gen, int(t.dim)) if t.elem.category == "struct"
                            else _wrapper_name(_fbs_elem_of(t.elem), int(t.dim))))
                out.append(f"        if (const auto *w = t->{key}())\n        {{\n")
                if t.elem.category in ("scalar", "enum"):
                    if t.elem.category == "enum":
                        out.append(f"            for (int i = 0; i < {t.dim}; ++i)\n")
                        out.append(f"                {dst}[i] = static_cast<{t.elem.fq}>(w->v()->Get(i));\n")
                    else:
                        out.append(f"            for (int i = 0; i < {t.dim}; ++i)\n")
                        out.append(f"                {dst}[i] = w->v()->Get(i);\n")
                else:
                    for index in range(int(t.dim)):
                        out.append(f"            Decode{t.elem.gen}(&w->v()->Get({index}), {dst}[{index}]);\n")
                out.append("        }\n")
            elif t.category == "value_array":
                elem_desc = t.elem
                out.append(f"        if (const auto *v = t->{key}())\n        {{\n")
                out.append(f"            {dst}.Clear();\n")
                out.append(f"            {dst}.Resize((int)v->size());\n")
                out.append(f"            auto *items = {dst}.GetData();\n")
                out.append("            for (uint32_t i = 0; i < v->size(); ++i)\n")
                if elem_desc.category == "enum":
                    out.append(f"                items[i] = static_cast<{elem_desc.fq}>(v->Get(i));\n")
                else:
                    out.append("                items[i] = v->Get(i);\n")
                out.append("        }\n")
            elif t.category == "ptr_array":
                out.append(f"        if (const auto *v = t->{key}())\n        {{\n")
                out.append(f"            {dst}.Clear();\n")
                out.append("            for (uint32_t i = 0; i < v->size(); ++i)\n")
                out.append("            {\n")
                out.append(f"                auto *item = {dst}.Create();\n")
                out.append(f"                Decode{t.elem.gen}(v->Get(i), *item);\n")
                out.append("            }\n")
                out.append("        }\n")
        out.append("    }\n\n")

    out.append("} // namespace hgl::asset::generated\n")
    return "".join(out)


def _fbs_elem_of_scalar(t: TypeDesc) -> str:
    return {"float": "float", "uint": "uint", "int": "int"}[t.scalar_class]


# ── 构建 ────────────────────────────────────────────────────────────────────
@dataclass
class RawField:
    name: str
    cpp_type: str
    offset: int
    size: int
    line: int
    tags: list[str] = field(default_factory=list)
    tail_doc: str = ""


def load_struct_meta(clang: Clang, repo_root: Path, group: GroupConfig,
                     info: AssetStruct, layout: dict) -> tuple[list[RawField], str]:
    """第一遍：只取字段名/类型/偏移与属性标签，**不做类型分类**（枚举尚未探测）。"""
    header_path = repo_root / info.header
    lines = read_lines(header_path)

    record = clang.ast_json(header_path, info.name)
    if record is None:
        raise GenError(f"{info.header}: AST 里找不到结构 {info.name}")

    qualified = record.get("type", {}).get("qualType") or info.name
    info.fq = qualified if "::" in qualified else f"{group.namespace}::{info.name}"
    info.ns = info.fq.rsplit("::", 1)[0]
    info.short_name = info.name
    info.gen_name = info.name

    if layout["size"] == 0:
        raise GenError(f"{info.name}: 记录布局尺寸为 0（结构未被定义？）")
    info.size = layout["size"]
    info.align = layout["align"]

    ast_fields = [c for c in record.get("inner", [])
                  if c.get("kind") == "FieldDecl" and not c.get("isImplicit")]
    layout_fields = layout.get("fields", [])
    if len(ast_fields) != len(layout_fields):
        raise GenError(
            f"{info.name}: AST 字段数 {len(ast_fields)} 与布局字段数 {len(layout_fields)} 不一致"
            f"（模板 / 位域 / 匿名成员不支持）"
        )

    doc = doc_above(lines, record.get("loc", {}).get("line") or 0)

    raw: list[RawField] = []
    for index, (ast_field, layout_field) in enumerate(zip(ast_fields, layout_fields)):
        if ast_field.get("name") != layout_field["name"]:
            raise GenError(f"{info.name}: AST 与布局字段顺序不一致")
        if index + 1 < len(layout_fields):
            size = layout_fields[index + 1]["offset"] - layout_field["offset"]
        else:
            size = layout["size"] - layout_field["offset"]
        qual = ast_field.get("type", {}).get("qualType", "")
        field_line = ast_field.get("loc", {}).get("line") or 0
        annotation = find_property_annotation(lines, field_line)
        raw.append(
            RawField(
                name=layout_field["name"],
                cpp_type=qual,
                offset=layout_field["offset"],
                size=size,
                line=field_line,
                tags=annotation[0] if annotation else [],
                tail_doc=annotation[1] if annotation else "",
            )
        )
    return raw, doc


def finalize_struct(info: AssetStruct, raw: list[RawField], registry: TypeRegistry) -> None:
    """第二遍：枚举已探测，做类型分类并生成属性描述。"""
    for field_info in raw:
        prop = PropertyInfo(
            name=field_info.name,
            toml_key=field_info.name,
            display_name=field_info.name,
            category="Default",
            tooltip="",
            cpp_type=field_info.cpp_type,
            offset=field_info.offset,
            size=field_info.size,
            type=TypeDesc(category="scalar"),
        )
        apply_property_tags(prop, field_info.tags, field_info.tail_doc)
        prop.type = registry.classify(field_info.cpp_type)
        if prop.type.category == "enum":
            prop.enum_name = prop.type.enum
        if prop.serialized:
            prop.flags |= PF_BITS["PF_Serialized"]
        if prop.serialized_toml:
            prop.flags |= PF_BITS["PF_SerializedToml"]
        if prop.serialized_flatbuffers:
            prop.flags |= PF_BITS["PF_SerializedFlatBuffers"]
        info.fields.append(prop)


def candidate_type_names(raw_map: dict[str, list[RawField]],
                         header_of: dict[str, str],
                         known: set[str]) -> dict[str, set[str]]:
    """需要探测的未知类型名 → 可能声明它的头文件集合。"""
    result: dict[str, set[str]] = {}
    for struct_name, fields in raw_map.items():
        header = header_of[struct_name]
        for field_info in fields:
            for name in _walk_type_names(field_info.cpp_type):
                if name in known:
                    continue
                result.setdefault(name, set()).add(header)
    return result


def _walk_type_names(cpp_type: str) -> list[str]:
    names: list[str] = []
    raw = strip_scopes(cpp_type)
    m = re.match(r"^(?P<base>.*?)\s*\[\s*(?P<dim>\w+)\s*\]$", raw)
    if m:
        return _walk_type_names(m.group("base"))
    if "<" in raw:
        for arg in inner_template_args(raw):
            names += _walk_type_names(arg)
    names.append(base_type_name(raw))
    return names


def collect_enums(clang: Clang, repo_root: Path, header: str, names: set[str],
                  default_ns: str) -> dict[str, EnumInfo]:
    result: dict[str, EnumInfo] = {}
    path = repo_root / header
    for name in sorted(names):
        node = clang.ast_json(path, name)
        if node is None or node.get("kind") != "EnumDecl":
            continue
        values: list[tuple[str, int]] = []
        next_value = 0
        for child in node.get("inner", []):
            if child.get("kind") != "EnumConstantDecl":
                continue
            cname = child.get("name")
            if not cname or cname in ("BEGIN_RANGE", "END_RANGE", "RANGE_SIZE"):
                continue
            value = next_value
            for expr in child.get("inner", []):
                if expr.get("kind") == "ConstantExpr" and "value" in expr:
                    value = int(expr["value"], 0)
                    break
            values.append((cname, value))
            next_value = value + 1
        # AST 的 qualType 对同文件内声明的枚举常常只给短名；补上分组命名空间，
        # 否则生成代码在 hgl::asset::generated 里找不到该类型。
        qualified = node.get("type", {}).get("qualType") or name
        fq = qualified if "::" in qualified else f"{default_ns}::{name}"
        result[name] = EnumInfo(name=name, fq=fq, values=values)
    return result


def discover(repo_root: Path, scan_dirs: list[str]) -> list[AssetStruct]:
    """找带 `HGL_ASSET_STRUCT` / `HGL_ASSET_CLASS` 注解的结构。

    `scan_dirs` 的每一项可以是目录（递归扫描）也可以是单个头文件 —— 后者让
    「同一个目录里不同资产的规约」可以分到不同分组，避免生成文件混在一起。
    """
    found: list[AssetStruct] = []
    for scan_dir in scan_dirs:
        target = repo_root / scan_dir
        if target.is_file():
            paths = [target]
        elif target.is_dir():
            paths = sorted(target.rglob("*.h"))
        else:
            continue
        for path in paths:
            text = path.read_text(encoding="utf-8", errors="ignore")
            for match in ANNOT_RE.finditer(text):
                name = match.group("name")
                found.append(
                    AssetStruct(
                        name=name,
                        kind=match.group("kind"),
                        header=path.relative_to(repo_root).as_posix(),
                        ns="",
                        short_name=name,
                        fq=name,
                        gen_name=name,
                    )
                )
    if len({s.name for s in found}) != len(found):
        seen: set[str] = set()
        dup: list[str] = []
        for s in found:
            if s.name in seen:
                dup.append(f"{s.name}({s.header})")
            seen.add(s.name)
        raise GenError("同一分组内出现重名资产结构（生成函数名会冲突）：" + ", ".join(dup))
    return found


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
        limit = min(len(current or ""), len(content))
        while first_diff < limit and current[first_diff] == content[first_diff]:
            first_diff += 1
        report.append(
            f"{path}: 与生成结果不一致（首个差异偏移 {first_diff}；"
            f"on-disk={len(current) if current is not None else 'missing'} / gen={len(content)}）"
        )
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def run_flatc(repo_root: Path, group: GroupConfig, flatc: str) -> None:
    if not group.fbs_generated:
        return
    schema = repo_root / group.fbs_schema
    outdir = (repo_root / group.fbs_generated).parent
    outdir.mkdir(parents=True, exist_ok=True)
    cmd = [flatc, "--cpp", "-o", str(outdir), str(schema)]
    proc = subprocess.run(cmd, capture_output=True, text=True, errors="ignore")
    if proc.returncode != 0:
        raise GenError(
            f"flatc 失败（{group.fbs_schema}）：{proc.stderr.strip()[:900]}"
        )


def check_flatc(group: GroupConfig, flatc: str, schema_content: str,
                report: list[str], generated_path: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="assetgen-flatc-") as temp_dir:
        temp_root = Path(temp_dir)
        schema_path = temp_root / Path(group.fbs_schema).name
        schema_path.write_text(schema_content, encoding="utf-8", newline="\n")
        cmd = [flatc, "--cpp", "-o", str(temp_root), str(schema_path)]
        proc = subprocess.run(cmd, capture_output=True, text=True, errors="ignore")
        if proc.returncode != 0:
            raise GenError(
                f"flatc 失败（{group.fbs_schema}）：{proc.stderr.strip()[:900]}"
            )

        generated_name = Path(group.fbs_generated).name
        generated_temp = temp_root / generated_name
        if not generated_temp.is_file():
            raise GenError(f"flatc 未生成预期文件：{generated_name}")
        if not generated_path.is_file():
            report.append(f"{generated_path}: 缺少 flatc 生成头")
            return

        expected = generated_path.read_text(encoding="utf-8", errors="ignore")
        actual = generated_temp.read_text(encoding="utf-8", errors="ignore")
        if expected.startswith("\ufeff"):
            expected = expected[1:]
        if actual != expected:
            report.append(f"{generated_path}: 与当前 .fbs 的 flatc 输出不一致")


def load_groups(config: dict) -> list[GroupConfig]:
    groups: list[GroupConfig] = []
    for name, entry in config.get("group", {}).items():
        groups.append(
            GroupConfig(
                name=name,
                namespace=entry["namespace"],
                scan=entry.get("scan", []),
                toml_header=entry["toml"],
                meta_header=entry["meta"],
                fbs_schema=entry.get("fbs", ""),
                fbs_generated=entry.get("fbs_generated", ""),
                flat_header=entry.get("flat", ""),
                fbs_namespace=entry.get("fbs_namespace", "hgl.asset.generated"),
            )
        )
    return groups


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="资产对象派生代码生成器（Clang AST 驱动）")
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--config", default="src/Tools/AssetGen/asset_gen.toml")
    parser.add_argument("--clang", default=os.environ.get("HGL_CLANG", "clang++"))
    parser.add_argument("--include-dir", action="append", default=[])
    parser.add_argument("--check", action="store_true", help="只比对不落盘")
    parser.add_argument("--write", action="store_true", help="生成并落盘（默认行为）")
    parser.add_argument("--flatc", default=os.environ.get("HGL_FLATC", "flatc"))
    parser.add_argument("--no-flatc", action="store_true",
                        help="跳过 flatc（只更新 .fbs 与 TOML/Meta，不重新生成二进制桥头）")
    parser.add_argument("--only", default="", help="只生成指定 group（调试用）")
    args = parser.parse_args(argv)

    repo_root = Path(args.repo_root).resolve()
    config_path = repo_root / args.config
    if not config_path.is_file():
        raise GenError(f"找不到配置：{config_path}")
    config = tomllib.loads(config_path.read_text(encoding="utf-8"))

    include_dirs = [str((repo_root / d).resolve()) for d in DEFAULT_INCLUDE_DIRS]
    env_extra = os.environ.get("HGL_ASSETGEN_INCLUDE_DIRS", "")
    for extra in env_extra.split(os.pathsep):
        if extra.strip():
            include_dirs.append(extra.strip())
    include_dirs.extend(args.include_dir)

    clang = Clang(args.clang, include_dirs)
    groups = load_groups(config)
    if args.only:
        groups = [g for g in groups if g.name == args.only]

    registry = TypeRegistry()
    registry.integer_enums = set(config.get("integer_enums", []))
    enum_aliases: dict[str, list[str]] = {
        entry["name"]: list(entry.get("aliases", []))
        for entry in config.get("enum", [])
    }

    report: list[str] = []
    total_structs = 0

    for group in groups:
        discovered = discover(repo_root, group.scan)
        if not discovered:
            continue
        for info in discovered:
            info.group = group.name
            struct_cfg = config.get("struct", {}).get(info.name, {})
            info.asset_kind = struct_cfg.get("asset_kind", "")
        registry.structs.update({s.name: s for s in discovered})

        layouts_cache: dict[str, dict] = {}
        for info in discovered:
            if info.header not in layouts_cache:
                layouts_cache[info.header] = parse_layouts(
                    clang.record_layouts(repo_root / info.header)
                )

        raw_map: dict[str, list[RawField]] = {}
        docs: dict[str, str] = {}
        for info in discovered:
            layout = (layouts_cache[info.header].get(info.name))
            if layout is None:
                raise GenError(f"{info.header}: 记录布局里找不到 {info.name}")
            raw_map[info.name], docs[info.name] = load_struct_meta(
                clang, repo_root, group, info, layout
            )
            info.doc = docs[info.name]

        known = (set(SCALAR_TYPES) | set(STRING_TYPES) | set(VECTOR_TYPES)
                 | set(CONTAINER_TYPES) | {s.name for s in discovered}
                 | registry.integer_enums | set(registry.enums))
        candidates = candidate_type_names(
            raw_map, {info.name: info.header for info in discovered}, known
        )

        found_enums: dict[str, EnumInfo] = {}
        unresolved: dict[str, set[str]] = {}
        for name, headers in candidates.items():
            hit = None
            for header in sorted(headers):
                got = collect_enums(clang, repo_root, header, {name}, group.namespace)
                if name in got:
                    hit = got[name]
                    break
            if hit:
                found_enums[name] = hit
            else:
                unresolved[name] = headers
        for name, info in found_enums.items():
            aliases = enum_aliases.get(name, [])
            if len(aliases) > len(info.values):
                raise GenError(
                    f"asset_gen.toml 给枚举 {name} 的 aliases 比实际枚举值还多"
                    f"（{len(aliases)} > {len(info.values)}）"
                )
            info.aliases = aliases
        registry.enums.update(found_enums)

        if unresolved:
            raise GenError(
                "以下字段类型既不是已知内建类型、也不是已注解结构或可解析枚举："
                + ", ".join(sorted(unresolved))
            )

        for info in discovered:
            finalize_struct(info, raw_map[info.name], registry)

        ordered = _order_for_emission(discovered)

        write_or_check(repo_root / group.toml_header,
                       render_toml_header(ordered, list(found_enums.values())),
                       args.check, report)
        write_or_check(repo_root / group.meta_header,
                       render_meta_header(group, ordered),
                       args.check, report)
        if group.fbs_schema:
            fbs_content = render_fbs_schema(group, ordered)
            write_or_check(repo_root / group.fbs_schema,
                           fbs_content,
                           args.check, report)
            if group.fbs_generated and not args.no_flatc:
                if args.check:
                    check_flatc(
                        group,
                        args.flatc,
                        fbs_content,
                        report,
                        repo_root / group.fbs_generated,
                    )
                else:
                    run_flatc(repo_root, group, args.flatc)
            if group.flat_header:
                write_or_check(repo_root / group.flat_header,
                               render_flat_header(group, ordered),
                               args.check, report)
        total_structs += len(ordered)

    if report:
        for line in report:
            print(f"[ASSETGEN] {line}", file=sys.stderr)
        print(f"[ASSETGEN] 共 {len(report)} 个产物与源不一致；改完 C++ 结构后请重新运行本工具。",
              file=sys.stderr)
        return 1

    print(f"[ASSETGEN] OK：{total_structs} 个资产结构，{len(groups)} 个分组")
    return 0


def _order_for_emission(structs: list[AssetStruct]) -> list[AssetStruct]:
    """被引用的结构必须先定义（生成代码按顺序前向声明 + 定义）。"""
    by_name = {s.name: s for s in structs}
    emitted: list[AssetStruct] = []
    pending = list(structs)

    def deps_of(info: AssetStruct) -> set[str]:
        names: set[str] = set()

        def walk(t: TypeDesc | None) -> None:
            if t is None:
                return
            if t.category == "struct" and t.cpp in by_name:
                names.add(t.cpp)
            walk(t.elem)

        for prop in info.fields:
            walk(prop.type)
        names.discard(info.name)
        return names

    while pending:
        progressed = False
        for info in list(pending):
            if deps_of(info) <= {s.name for s in emitted}:
                emitted.append(info)
                pending.remove(info)
                progressed = True
        if not progressed:
            raise GenError("资产结构之间存在循环引用：" + ", ".join(s.name for s in pending))
    return emitted


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except GenError as error:
        print(f"[ASSETGEN] ERROR: {error}", file=sys.stderr)
        raise SystemExit(2)
