#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AssetGen —— 资产对象派生代码生成器

唯一真源：`inc/hgl/**` 里带 `HGL_ASSET_STRUCT` / `HGL_ASSET_CLASS` 注解的 C++ 结构。
注解只表达「这个结构是资产规约」，字段清单、类型、顺序、偏移、类内缺省值全部由
Clang AST + 记录布局自动提取 —— **不需要逐字段标注**。`HGL_PROPERTY(...)` 是可选的
编辑器/文档元数据增强。

产出（一律提交进仓库，MSVC 构建不需要 Clang）：

    inc/hgl/asset/generated/<Group>Meta.gen.h            属性元数据表（GUI / 文档 / 序列化驱动）
    inc/hgl/asset/generated/<Group>Serialize.gen.h       自有 Archive 序列化（StructSerializer<T>）

格式（TOML / FlatBuffers）不生成任何代码：两者都是 `hgl::serialize` 的纯后端，
由 `ClassMetaInfo` 在运行期驱动。

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
# C++ 基础类型 → (PropertyKind, 标量类)
SCALAR_TYPES: dict[str, tuple[str, str]] = {
    "bool":     ("Bool",   "  "),
    "uint8_t":  ("UInt8",  "  "),
    "uint16_t": ("UInt16", "  "),
    "uint32_t": ("UInt32", "  "),
    "uint64_t": ("UInt64", "  "),
    "uint8":    ("UInt8",  "  "),
    "uint16":   ("UInt16", "  "),
    "uint32":   ("UInt32", "  "),
    "uint64":   ("UInt64", "  "),
    "uint":     ("UInt32", "  "),
    "int8_t":   ("Int8",   "   "),
    "int16_t":  ("Int16",  "   "),
    "int32_t":  ("Int32",  "   "),
    "int64_t":  ("Int64",  "   "),
    "int8":     ("Int8",   "   "),
    "int16":    ("Int16",  "   "),
    "int32":    ("Int32",  "   "),
    "int64":    ("Int64",  "   "),
    "int":      ("Int32",  "   "),
    "float":    ("Float",  " "),
    "double":   ("Double", " "),
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
    scalar_class: str = ""   # float|uint|int|bool
    components: int = 0
    elem: "TypeDesc | None" = None
    dim: str | None = None
    enum: str | None = None
    gen: str = ""       # 生成函数名后缀（struct 用）
    presence: bool = False  # 由 `hgl::serialize::Presence<T>` 包装（记录「键是否出现」）


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
    # 外部枚举（如 Vulkan）：名字↔值已有唯一真源，生成器只转发、不复制拼写表。
    extern_from: str = ""
    extern_to: str = ""
    # 转发目标所在的头文件（`hgl/...` 相对 `inc/`），必须声明，否则生成的转发调用无法编译。
    extern_include: str = ""
    # 转发目标接受的参数类型（`extern_from` 的 out 参数 / `extern_to` 的入参）。
    # 只有在真源函数签名与本枚举类型不同（如 `VkCullModeFlagBits` ↔ `VkCullModeFlags`）时才需要。
    extern_mediator: str = ""

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
    serialize_header: str
    meta_header: str


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

        if base == "Presence":
            args = inner_template_args(raw)
            if len(args) != 1:
                raise GenError(f"hgl::serialize::Presence 必须只有一个模板参数：{cpp_type}")
            inner = self.classify(args[0])
            if inner.presence:
                raise GenError(f"Presence 不能嵌套：{cpp_type}")
            if inner.category == "array":
                raise GenError(f"Presence 不支持定长 C 数组字段：{cpp_type}")
            inner.presence = True
            return inner

        if base in SCALAR_TYPES:
            kind, scalar_class = SCALAR_TYPES[base]
            return TypeDesc(category="scalar", kind=kind, cpp=base, fq=raw,
                            scalar_class=scalar_class)

        if base in STRING_TYPES:
            return TypeDesc(category="string", kind="String", cpp=base, fq=raw)

        if base in VECTOR_TYPES:
            components, scalar_class = VECTOR_TYPES[base]
            kind = {2: "Vector2f", 3: "Vector3f", 4: "Vector4f"}[components]
            return TypeDesc(category="vector", kind=kind, cpp=base, fq=raw,
                            scalar_class=scalar_class, components=components)

        if base in CONTAINER_TYPES:
            args = inner_template_args(raw)
            if len(args) != 1:
                raise GenError(f"容器类型必须有且只有一个模板参数：{cpp_type}")
            elem = self.classify(args[0])
            # 元数据里 `fq` 会原样进入 `sizeof(...)`，而模板实参在 AST 里是**按原样拼写**
            # 返回的（`ValueArray<Foo>`），落到 `hgl::asset::generated` 作用域里解析不了
            # 非 `hgl::` 命名空间的 `Foo`。所以容器的 fq 由元素 fq 重建，而不是抄原文。
            fq = f"hgl::{base}<{elem.fq}>"
            return TypeDesc(category="value_array" if base == "ValueArray" else "ptr_array",
                            kind=CONTAINER_TYPES[base], cpp=base, fq=fq, elem=elem,
                            components=elem.components)

        if base in self.structs:
            target = self.structs[base]
            return TypeDesc(category="struct", kind="Struct", cpp=base, fq=target.fq,
                            gen=target.gen_name)

        if base in self.enums:
            info = self.enums[base]
            return TypeDesc(category="enum", kind="Enum", cpp=base, fq=info.fq,
                            enum=info.fq)

        if base in self.integer_enums:
            return TypeDesc(category="enum", kind="Enum", cpp=base, fq=raw)

        raise GenError(
            f"未知的资产字段类型：{cpp_type}（若是外部枚举请在 asset_gen.toml 的 "
            f"integer_enums 里登记；若是结构请加 HGL_ASSET_STRUCT 注解）"
        )


# ── 元数据渲染 ──────────────────────────────────────────────────────────────
def c_escape(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


def render_meta_header(group: GroupConfig, structs: list[AssetStruct],
                       enums: list[EnumInfo]) -> str:
    out: list[str] = []
    out.append(
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "// @ulre-codegen begin\n"
        "//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。\n"
        "//   属性元数据表：供检查器 GUI、文档导出、严格键校验与通用格式后端消费。\n"
        "//   它是「结构长什么样」的**唯一运行期真源**，与格式无关。\n"
        "// @ulre-codegen end\n"
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "#pragma once\n\n"
        "#include <cstddef>\n\n"
        "#include <hgl/asset/AssetObject.h>\n"
    )
    for header in sorted({s.header for s in structs}):
        rel = header[len("inc/"):] if header.startswith("inc/") else header
        out.append(f"#include <{rel}>\n")
    # 外部枚举的转发目标（如 `VkEnumText.h`）：生成代码直接调用它，必须显式包含。
    for rel in sorted({i.extern_include for i in enums if i.extern_include}):
        out.append(f"#include <{rel}>\n")
    out.append("\nnamespace hgl::asset::generated\n{\n")
    out.append(render_enum_tables(enums))

    # `prop.enum_name` 存的是枚举完全限定名；适配器函数名用的是生成器里登记的短名。
    enum_fn_by_fq = {info.fq: info.name for info in enums}

    # 属性表的 `struct_meta` 要指向子结构的 `ClassMetaInfo`，而结构之间可以互相
    # 引用（A 的子表是 B、B 的子表是 A），所以先统一前向声明一次。
    for info in structs:
        out.append(f"    extern const ClassMetaInfo Class_{info.gen_name};\n")
    out.append("\n")

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
            elem = prop.type
            while elem.elem is not None and elem.category in ("value_array", "ptr_array"):
                elem = elem.elem
            struct_meta = f"&Class_{elem.gen}" if elem.category == "struct" and elem.gen else "nullptr"
            # 容器元素的类别：FlatBuffers 后端据此决定向量元素是标量、字符串还是子表。
            if prop.type.category in ("value_array", "ptr_array"):
                elem_kind = f"PropertyKind::{elem.kind or 'String'}"
            else:
                elem_kind = "PropertyKind::None"
            if prop.type.category == "enum" and prop.type.enum:
                fn = enum_fn_by_fq.get(prop.type.enum, base_type_name(prop.type.enum))
                enum_parse = f"&AssetEnumParse64_{fn}"
                enum_to_name = f"&AssetEnumName64_{fn}"
            else:
                enum_parse = "nullptr"
                enum_to_name = "nullptr"
            # `Presence<T>` 是「值 + 是否出现」的包装；元数据描述的是里面的真实类型。
            #
            # 偏移与尺寸一律用编译期表达式，**不**写 Clang 记录布局算出的常量：
            # 布局常量取决于编译配置（MSVC 的 `std::string` 在 Debug 下比 Clang
            # 默认环境大 16 字节），写死会让含字符串字段的结构在另一种配置下
            # 偏移全错。`sizeof(decltype(...))` / `offsetof` 由当前编译器自己算。
            offset_literal = f"(uint32_t)offsetof({info.fq}, {prop.name})"
            size_literal = (f"(uint32_t)sizeof({prop.type.fq})"
                            if prop.type.presence
                            else f"(uint32_t)sizeof(decltype({info.fq}::{prop.name}))")
            if prop.type.presence:
                names = [n for n in names if n != "PF_None"]
                names.append("PF_Presence")
                flags_literal = " | ".join(names)
            out.append(
                f'        {{ "{c_escape(prop.name)}", "{c_escape(prop.toml_key)}", '
                f'"{c_escape(prop.display_name)}", "{c_escape(prop.category)}", '
                f'"{c_escape(prop.tooltip)}", "{c_escape(prop.cpp_type)}", '
                f"{enum_name}, {asset_ref}, "
                f"PropertyKind::{kind}, {flags_literal}, "
                f"{offset_literal}, {size_literal}, {fixed_len}u, "
                f"{prop.range_min}, {prop.range_max}, {prop.step}, {struct_meta}, "
                f"{elem_kind}, {enum_parse}, {enum_to_name} }},\n"
            )
        out.append("    };\n\n")
        out.append(
            f"    inline const ClassMetaInfo Class_{info.gen_name} =\n"
            f"    {{\n"
            f'        "{c_escape(info.short_name)}", "{c_escape(info.asset_kind)}", '
            f'"{c_escape(info.doc)}",\n'
            f"        {table}, (uint32_t)(sizeof({table}) / sizeof({table}[0])),\n"
            f"        (uint32_t)sizeof({info.fq}), (uint32_t)alignof({info.fq})\n"
            f"    }};\n\n"
        )

    for info in structs:
        out.append(
            f"    inline const ClassMetaInfo *GetClassMeta(const {info.fq} *) "
            f"{{ return &Class_{info.gen_name}; }}\n"
        )

    out.append("} // namespace hgl::asset::generated\n")
    return "".join(out)


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
    if base_type_name(raw) == "Presence":
        # 只是「键是否出现过」的包装，要探测的是里面的真实类型
        for arg in inner_template_args(raw):
            names += _walk_type_names(arg)
        return names
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


def render_enum_tables(enums: list[EnumInfo]) -> str:
    """枚举名字表：`AssetEnumName_*` / `AssetEnumParse_*` 及其 `int64_t` 适配器。

    这些表是**元数据**（名字↔值），不是序列化逻辑，所以落在 `Meta.gen.h`；
    依赖方向因此固定为 `Serialize.gen.h` → `Meta.gen.h`（单向，不成环）。
    """
    out: list[str] = []

    # 枚举名表（TOML 文本、FlatBuffers 枚举、反射适配器都复用这一份）
    for info in enums:
        if info.extern_from and info.extern_to:
            # 外部枚举（Vulkan …）：拼写表已经有唯一真源，这里只转发，不复制。
            if info.extern_mediator:
                # 真源函数的参数类型 ≠ 本枚举类型：先转换再调用。
                out.append(f"    inline const char *AssetEnumName_{info.name}(const {info.fq} v)\n"
                           f"    {{ return {info.extern_to}(static_cast<{info.extern_mediator}>(v)); }}\n\n")
                out.append(f"    inline bool AssetEnumParse_{info.name}(const char *s, {info.fq} &out)\n"
                           "    {\n"
                           f"        {info.extern_mediator} tmp = static_cast<{info.extern_mediator}>(out);\n"
                           f"        if (!{info.extern_from}(s, tmp)) return false;\n"
                           f"        out = static_cast<{info.fq}>(tmp);\n"
                           "        return true;\n"
                           "    }\n\n")
                continue
            out.append(f"    inline const char *AssetEnumName_{info.name}(const {info.fq} v)\n"
                       f"    {{ return {info.extern_to}(v); }}\n\n")
            out.append(f"    inline bool AssetEnumParse_{info.name}(const char *s, {info.fq} &out)\n"
                       f"    {{ return {info.extern_from}(s, out); }}\n\n")
            continue
        out.append(f"    inline const char *AssetEnumName_{info.name}(const {info.fq} v)\n    {{\n")
        out.append("        switch (v)\n        {\n")
        for index, (name, _value) in enumerate(info.values):
            out.append(f'        case {info.fq}::{name}: return "{info.toml_names()[index]}";\n')
        out.append("        }\n        return nullptr;\n    }\n\n")

        out.append(f"    inline bool AssetEnumParse_{info.name}(const char *s, {info.fq} &out)\n    {{\n")
        out.append("        if (!s) return false;\n")
        for index, (name, _value) in enumerate(info.values):
            toml_name = info.toml_names()[index]
            out.append(
                f'        if (hgl::stricmp(s, "{toml_name}") == 0) '
                f"{{ out = {info.fq}::{name}; return true; }}\n"
            )
            if toml_name != name:
                out.append(
                    f'        if (hgl::stricmp(s, "{name}") == 0) '
                    f"{{ out = {info.fq}::{name}; return true; }}\n"
                )
        out.append("        return false;\n    }\n\n")

    # `PropertyMetaInfo::enum_parse` / `enum_to_name` 走 `int64_t`（反射层不认识具体枚举
    # 类型），这里为每个枚举补一对薄适配器；真正的拼写表仍然只有上面那一份。
    for info in enums:
        out.append(f"    inline const char *AssetEnumName64_{info.name}(const int64_t v)\n"
                   f"    {{ return AssetEnumName_{info.name}(static_cast<{info.fq}>(v)); }}\n\n")
        out.append(f"    inline bool AssetEnumParse64_{info.name}(const char *s, int64_t &out)\n"
                   "    {\n"
                   f"        {info.fq} tmp{{}};\n"
                   f"        if (!AssetEnumParse_{info.name}(s, tmp)) return false;\n"
                   "        out = static_cast<int64_t>(tmp);\n"
                   "        return true;\n"
                   "    }\n\n")

    return "".join(out)


def render_serialize_header(group: GroupConfig, structs: list[AssetStruct],
                            enums: list[EnumInfo]) -> str:
    """生成 `<Group>Serialize.gen.h`：唯一一份字段遍历（EnumTraits + StructSerializer）。"""
    out: list[str] = []
    out.append(
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "// @ulre-codegen begin\n"
        "//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。\n"
        "//   这是**唯一一份**字段遍历代码：它只说「哪个字段、什么名字、什么 id」，\n"
        "//   不关心落到 TOML 还是 FlatBuffers。新增格式后端不需要重新生成任何东西。\n"
        "// @ulre-codegen end\n"
        "// ─────────────────────────────────────────────────────────────────────────────\n"
        "#pragma once\n\n"
        "#include <hgl/serialize/Archive.h>\n"
    )
    for header in sorted({s.header for s in structs}):
        rel = header[len("inc/"):] if header.startswith("inc/") else header
        out.append(f"#include <{rel}>\n")
    # 枚举拼写表与 `ClassMetaInfo` 都在 Meta.gen.h 里；本文件只做「字段遍历」。
    rel = group.meta_header
    rel = rel[len("inc/"):] if rel.startswith("inc/") else rel
    out.append(f"#include <{rel}>\n")
    out.append("\nnamespace hgl::serialize\n{\n")
    for info in enums:
        out.append(f"    template<> struct EnumTraits<{info.fq}>\n    {{\n")
        out.append("        static constexpr bool defined = true;\n")
        out.append(f"        static const char *ToName({info.fq} v)\n")
        out.append(f"        {{ return hgl::asset::generated::AssetEnumName_{info.name}(v); }}\n")
        out.append(f"        static bool FromName(const char *s, {info.fq} &out)\n")
        out.append(f"        {{ return hgl::asset::generated::AssetEnumParse_{info.name}(s, out); }}\n")
        out.append("    };\n\n")

    for info in structs:
        out.append(f"    template<> struct StructSerializer<{info.fq}>\n    {{\n")
        out.append("        static constexpr bool defined = true;\n\n")
        out.append("        template<typename Self>\n")
        out.append("        static void Do(Archive &ar, Self &v)\n        {\n")
        field_id = 0
        for prop in info.fields:
            if not prop.serialized_toml:
                continue
            field_id += 1
            out.append(f'            ar.Field({field_id}, "{prop.toml_key}", v.{prop.name});\n')
        out.append("        }\n")
        out.append(f"        static const hgl::asset::ClassMetaInfo *Meta()\n"
                   f"        {{ return &hgl::asset::generated::Class_{info.gen_name}; }}\n")
        out.append("    };\n\n")

    out.append("}//namespace hgl::serialize\n")
    return "".join(out)


def load_groups(config: dict) -> list[GroupConfig]:
    groups: list[GroupConfig] = []
    for name, entry in config.get("group", {}).items():
        groups.append(
            GroupConfig(
                name=name,
                namespace=entry["namespace"],
                scan=entry.get("scan", []),
                serialize_header=entry["serialize"],
                meta_header=entry["meta"],
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

    # 外部枚举：名字↔值已在本仓另有唯一真源（如 VkEnumText.h 的 Vulkan 拼写表）。
    # 生成器只输出转发，避免把同一张拼写表复制出第二份。
    extern_enums: dict[str, EnumInfo] = {}
    for entry in config.get("enum", []):
        extern_from = entry.get("extern_from", "")
        if not extern_from:
            continue
        if not entry.get("extern_to"):
            raise GenError(f"asset_gen.toml: 枚举 {entry['name']} 声明了 extern_from 却缺 extern_to")
        if not entry.get("extern_include"):
            raise GenError(f"asset_gen.toml: 枚举 {entry['name']} 声明了 extern_from 却缺 extern_include"
                           "（生成代码要直接调用转发目标，必须知道它在哪个头文件里）")
        extern_enums[entry["name"]] = EnumInfo(
            name=entry["name"],
            fq=entry.get("fq") or f"::{entry['name']}",
            extern_from=extern_from,
            extern_to=entry["extern_to"],
            extern_include=entry["extern_include"],
            extern_mediator=entry.get("extern_mediator", ""),
        )
    registry.enums.update(extern_enums)

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

        # 只为本分组实际用到的外部枚举生成转发与 EnumTraits，避免跨分组重复定义。
        group_enums = list(found_enums.values())
        seen_extern: set[str] = set()
        for info in discovered:
            for prop in info.fields:
                t = prop.type
                if t.category != "enum" or t.cpp not in extern_enums:
                    continue
                if t.cpp in seen_extern:
                    continue
                seen_extern.add(t.cpp)
                group_enums.append(extern_enums[t.cpp])

        ordered = _order_for_emission(discovered)

        write_or_check(repo_root / group.serialize_header,
                       render_serialize_header(group, ordered, group_enums),
                       args.check, report)
        write_or_check(repo_root / group.meta_header,
                       render_meta_header(group, ordered, group_enums),
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
