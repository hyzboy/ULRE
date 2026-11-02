#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ShaderConstGen —— C++ ↔ GLSL 数值契约派生代码生成器

唯一真源：`inc/hgl/shader/contract/*.h` 里带 `HGL_SHADER_ENUM` / `HGL_SHADER_CONST`
注解的枚举与常量（值、项序、项名全部由 Clang AST 直接读出，**不需要逐项标注**）。
命名/格式化策略（GLSL 宏名前后缀、数值后缀）声明在 `shader_const_gen.toml`。

产出（一律提交进仓库，MSVC 构建不需要 Clang）：

    ShaderLibrary/contract/sampler_presets.glsl     采样器预设序号宏
    ShaderLibrary/contract/engine_constants.glsl    GPU 剔除 / 拓扑 / 光照阴影 / SDF 常量宏
    inc/hgl/shader/generated/ShaderConstantsParity.gen.h
                                                    与遗留 C++ 常量的编译期 static_assert

另外做两类**只校验不产出**的一致性检查：

    * `[[verify_glsl]]`：手写 GLSL 里的 `#define` 必须与契约枚举逐条同值。
      这些文件（descriptor_macros / interstage_locations / domain 绑定表）目前仍由
      既有生成器或人手维护 —— 它们是 shader 内容哈希的参与者，重写会一次性失效
      全量着色器缓存，故此处只对拍、不改写。
    * `[[sampler_registry]]`：`ShaderLibrary/contract/sampler.toml` 的条目顺序必须与
      `SamplerPresetId` 枚举逐项同名同序 —— 采样器序号写错只会静默降级为 Nearest。

用法
----
    python src/Tools/ShaderConstGen/shader_const_gen.py --repo-root . --write
    python src/Tools/ShaderConstGen/shader_const_gen.py --repo-root . --check

`--check` 只比对不落盘：有差异则打印首个差异并返回 1（CI / 门禁用）。
"""

from __future__ import annotations

import argparse
import json
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

DEFAULT_INCLUDE_DIRS = ("inc",)
DEFAULT_CONFIG = "src/Tools/ShaderConstGen/shader_const_gen.toml"

# 契约头目录（相对仓库根）：注解发现与 AST 探针 TU 的 include 面
CONTRACT_DIR = "inc/hgl/shader/contract"

# `enum class HGL_SHADER_ENUM Name : uint32_t`
ANNOT_ENUM_RE = re.compile(r"\benum\s+class\s+HGL_SHADER_ENUM\s+([A-Za-z_]\w*)")
# `static constexpr <type> HGL_SHADER_CONST Name = <literal>;`
ANNOT_CONST_RE = re.compile(
    r"\bstatic\s+constexpr\s+[\w:<>\s]+\bHGL_SHADER_CONST\s+([A-Za-z_]\w*)\s*=\s*([^;]+);"
)
# GLSL `#define NAME VALUE`（抓取手写文件里的宏用于对拍）
GLSL_DEFINE_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)\s+(.+?)\s*(?://.*)?$")
# GLSL `const uint NAME = VALUE;`（不走 #define 的声明式常量）
GLSL_CONST_DECL_RE = re.compile(
    r"^\s*const\s+\w+\s+([A-Za-z_]\w*)\s*=\s*([^;]+);")

INT_LITERAL_RE = re.compile(r"^(0[xX][0-9a-fA-F]+|\d+)[uUlL]*$")


class GenError(RuntimeError):
    pass


# ── 数据模型 ────────────────────────────────────────────────────────────────

@dataclass
class EnumItem:
    cpp_name: str
    value: int
    glsl_name: str
    comment: str | None = None


@dataclass
class EnumBlock:
    cpp: str                       # 完整限定名，如 hgl::shader::contract::SamplerPresetId
    name: str                      # 简单名
    value_style: str = "uint"      # uint | int | hex | float
    value_suffix: str = "u"
    items: list[EnumItem] = field(default_factory=list)
    section: list[str] = field(default_factory=list)   # 生成的区块注释（每行一条）
    footer: list[str] = field(default_factory=list)

    def glsl_value(self, value: int) -> str:
        return format_value(value, self.value_style, self.value_suffix)


@dataclass
class GlslFile:
    name: str                      # 输出文件名（落在 glsl_dir 下）
    guard: str
    doc: list[str]                 # 文件头说明（在 GENERATED 横幅之后）
    blocks: list[EnumBlock] = field(default_factory=list)


# ── 命名策略 ────────────────────────────────────────────────────────────────

def upper_snake(name: str) -> str:
    """CamelCase → UPPER_SNAKE。

    连续大写视为一个词（`DataIndexID` → `DATA_INDEX_ID`、`UV0` → `UV0`），
    数字不单独成词（`UV0` 不拆成 `UV_0`）。
    """
    out: list[str] = []
    for i, ch in enumerate(name):
        if ch.isupper() and i > 0:
            prev = name[i - 1]
            nxt = name[i + 1] if i + 1 < len(name) else ""
            # 前一个小写/数字 → 新词；前一个大写但下一个小写 → 新词（IDFoo → ID_Foo）
            if prev.islower() or prev.isdigit() or (prev.isupper() and nxt.islower()):
                out.append("_")
        out.append(ch)
    return "".join(out).upper()


def make_glsl_name(cpp_name: str, style: str, prefix: str, suffix: str) -> str:
    base = cpp_name if style == "as_is" else upper_snake(cpp_name)
    return f"{prefix}{base}{suffix}"


def format_value(value: int, style: str, value_suffix: str) -> str:
    if style == "hex":
        return f"0x{value:X}u" if value <= 0xFFFFFFFF else f"0x{value:X}ull"
    if style == "float":
        return f"{value}.0"
    if style == "int":
        return str(value)
    # 默认 uint
    return f"{value}{value_suffix}"


# ── Clang 调用 ──────────────────────────────────────────────────────────────

class Clang:
    def __init__(self, exe: str, include_dirs: list[str]):
        self.exe = exe
        self.include_dirs = include_dirs

    def ast_json(self, tu: Path, name_filter: str) -> list[dict]:
        cmd = [self.exe, "-x", "c++", "-std=c++20", "-fsyntax-only",
               "-D_CRT_SECURE_NO_WARNINGS", "-w"]
        for inc in self.include_dirs:
            cmd += ["-I", inc]
        cmd += ["-Xclang", "-ast-dump=json",
                "-Xclang", f"-ast-dump-filter={name_filter}",
                str(tu)]
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              encoding="utf-8", errors="replace")
        if proc.returncode != 0:
            raise GenError(
                f"clang AST 失败（filter={name_filter}）：{proc.stderr.strip()[:900]}")

        decoder = json.JSONDecoder()
        text = proc.stdout
        nodes: list[dict] = []
        pos = 0
        while pos < len(text):
            while pos < len(text) and text[pos] in " \t\r\n":
                pos += 1
            if pos >= len(text):
                break
            node, pos = decoder.raw_decode(text, pos)
            nodes.append(node)
        return nodes


def find_enum(nodes: list[dict], name: str) -> dict:
    """在 AST 顶层节点里找简单名匹配的 EnumDecl（filter 已保证命名空间匹配）。"""
    hits = [n for n in nodes if n.get("kind") == "EnumDecl" and n.get("name") == name]
    if not hits:
        raise GenError(f"Clang AST 里找不到枚举 `{name}`（契约头是否已 include？）")
    if len(hits) > 1:
        raise GenError(
            f"枚举 `{name}` 在契约命名空间下重名（命中 {len(hits)} 处）——"
            "契约枚举名必须全局唯一，否则 AST 无法确定归属")
    return hits[0]


# ── 配置解析 ────────────────────────────────────────────────────────────────

def load_config(path: Path) -> dict:
    with path.open("rb") as fp:
        return tomllib.load(fp)


def build_glsl_files(cfg: dict, ast_nodes: list[dict],
                     annotated: set[str]) -> tuple[list[GlslFile], dict[str, EnumBlock]]:
    """把配置 + Clang AST 展开成「待写盘的文件」与「已知契约枚举表」。

    两类条目都会进 `enums`：`[[file.enum]]`（要写盘）与顶层 `[[enum]]`
    （只声明命名策略、仅供 [[verify_glsl]] / [[assert]] 引用，不产出 GLSL 文件）。
    """
    files: list[GlslFile] = []
    registry: dict[str, EnumBlock] = {}

    def make_block(ecfg: dict) -> EnumBlock:
        cpp = ecfg["cpp"]
        simple = cpp.rsplit("::", 1)[-1]
        if simple not in annotated:
            raise GenError(
                f"配置引用了 `{cpp}`，但 {CONTRACT_DIR} 下没有 "
                f"`enum class HGL_SHADER_ENUM {simple}` 注解")
        if cpp in registry:
            raise GenError(f"枚举 `{cpp}` 在配置里出现了两次（命名策略必须唯一）")

        node = find_enum(ast_nodes, simple)
        style = ecfg.get("name_style", "as_is")
        prefix = ecfg.get("prefix", "")
        suffix = ecfg.get("suffix", "")

        overrides = {item_cfg["cpp_name"]: item_cfg for item_cfg in ecfg.get("item", [])}

        block = EnumBlock(
            cpp=cpp,
            name=simple,
            value_style=ecfg.get("value_style", "uint"),
            value_suffix=ecfg.get("value_suffix", "u"),
            section=list(ecfg.get("section", [])),
            footer=list(ecfg.get("footer", [])),
        )

        constants = [c for c in node.get("inner", [])
                     if c.get("kind") == "EnumConstantDecl"]
        if not constants:
            raise GenError(f"枚举 `{simple}` 没有任何枚举项")

        unknown = set(overrides) - {c.get("name") for c in constants}
        if unknown:
            raise GenError(
                f"配置里 `{simple}.item` 引用了不存在的枚举项：{sorted(unknown)}")

        for const in constants:
            cpp_name = const["name"]
            inner = const.get("inner") or []
            if not inner or "value" not in inner[0]:
                raise GenError(
                    f"枚举项 `{simple}::{cpp_name}` 没有显式赋值 —— "
                    "契约枚举要求逐项显式赋值（隐式递增会静默挪动后续所有项）")

            item_override = overrides.get(cpp_name) or {}
            if item_override.get("exclude", False):
                continue

            block.items.append(EnumItem(
                cpp_name=cpp_name,
                value=int(inner[0]["value"]),
                glsl_name=(item_override.get("glsl_name")
                           or make_glsl_name(cpp_name, style, prefix, suffix)),
                comment=item_override.get("comment"),
            ))

        registry[cpp] = block
        return block

    for ecfg in cfg.get("enum", []):
        make_block(ecfg)

    for fcfg in cfg.get("file", []):
        gfile = GlslFile(
            name=fcfg["name"],
            guard=fcfg["guard"],
            doc=list(fcfg.get("doc", [])),
        )
        for ecfg in fcfg.get("enum", []):
            gfile.blocks.append(make_block(ecfg))
        files.append(gfile)

    # 注解了但没进配置 = 忘了声明命名策略 → 直接报错，避免"以为导出了其实没有"
    configured = {c.rsplit("::", 1)[-1] for c in registry}
    missing = sorted(annotated - configured)
    if missing:
        raise GenError(
            "以下枚举带 HGL_SHADER_ENUM 注解，但没有在 shader_const_gen.toml 里"
            "声明命名策略（[[file.enum]] 或 [[enum]]）：\n  " + "\n  ".join(missing))

    return files, registry



# ── 渲染 ────────────────────────────────────────────────────────────────────

BANNER = [
    "// GENERATED by ShaderConstGen — DO NOT EDIT",
    "// 数值真源：inc/hgl/shader/contract/*.h（HGL_SHADER_ENUM 注解枚举）",
    "// 重新生成：python src/Tools/ShaderConstGen/shader_const_gen.py --repo-root . --write",
]


def render_glsl(gfile: GlslFile) -> str:
    lines: list[str] = list(BANNER)
    lines.extend(f"// {text}" if text else "//" for text in gfile.doc)
    lines.append("")
    lines.append(f"#ifndef {gfile.guard}")
    lines.append(f"#define {gfile.guard}")

    for block in gfile.blocks:
        lines.append("")
        for text in block.section:
            lines.append(f"// {text}" if text else "//")
        lines.append(f"// 真源：{block.cpp}")
        lines.append("")
        for item in block.items:
            if item.comment:
                lines.append(f"// {item.comment}")
            lines.append(f"#ifndef {item.glsl_name}")
            lines.append(f"#define {item.glsl_name} {block.glsl_value(item.value)}")
            lines.append("#endif")
        for text in block.footer:
            lines.append(f"// {text}" if text else "//")

    lines.append("")
    lines.append(f"#endif // {gfile.guard}")
    return "\n".join(lines) + "\n"


def render_parity(cfg: dict, enums: dict[str, EnumBlock]) -> str:
    pcfg = cfg.get("parity", {})
    guard = pcfg.get("guard", "HGL_SHADER_CONSTANTS_PARITY_GEN_H")
    lines: list[str] = [
        "// GENERATED by ShaderConstGen — DO NOT EDIT",
        "// 编译期对拍：契约枚举（inc/hgl/shader/contract/）与本仓遗留 C++ 常量必须相等。",
        "// 重新生成：python src/Tools/ShaderConstGen/shader_const_gen.py --repo-root . --write",
        "",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
    ]
    for inc in pcfg.get("include", []):
        lines.append(f"#include <{inc}>")
    lines.append("")

    if pcfg.get("namespace"):
        lines.append(f"namespace {pcfg['namespace']}")
        lines.append("{")

    for entry in cfg.get("assert", []):
        contract = entry["contract"]
        legacy = entry["legacy"]
        enum_cpp, item_name = contract.rsplit("::", 1)
        block = enums.get(enum_cpp)
        if block is None:
            raise GenError(f"[[assert]] 引用了未导出的枚举 `{enum_cpp}`")
        if not any(i.cpp_name == item_name for i in block.items):
            raise GenError(f"[[assert]] 引用了 `{enum_cpp}` 里不存在的枚举项 `{item_name}`")
        msg = entry.get("message", f"{contract} 与 {legacy} 漂移")
        lines.append(f"static_assert(static_cast<int64_t>({contract}) == "
                     f"static_cast<int64_t>({legacy}),")
        lines.append(f"              \"{msg}\");")

    if pcfg.get("namespace"):
        lines.append("}")
    lines.append("")
    lines.append(f"#endif // {guard}")
    return "\n".join(lines) + "\n"


# ── 只读校验 ────────────────────────────────────────────────────────────────

def parse_glsl_defines(path: Path, kind: str = "define") -> dict[str, str]:
    """抓取 GLSL 里可对拍的常量：`#define NAME VALUE` 或 `const <T> NAME = VALUE;`。"""
    pattern = GLSL_CONST_DECL_RE if kind == "const_decl" else GLSL_DEFINE_RE
    found: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if raw.strip().startswith("//"):
            continue
        m = pattern.match(raw)
        if m:
            found[m.group(1)] = m.group(2).strip()
    return found


def _int_of(text: str) -> int | None:
    text = text.strip()
    m = INT_LITERAL_RE.match(text)
    if not m:
        return None
    return int(m.group(1), 0)


def verify_glsl(entry: dict, repo_root: Path, enums: dict[str, EnumBlock]) -> list[str]:
    rel = entry["file"]
    path = repo_root / rel
    if not path.exists():
        return [f"{rel}: 文件不存在（[[verify_glsl]] 目标缺失）"]

    defines = parse_glsl_defines(path, entry.get("kind", "define"))
    problems: list[str] = []

    for check in entry.get("check", []):
        enum_cpp = check["enum"]
        block = enums.get(enum_cpp)
        if block is None:
            problems.append(f"{rel}: [[verify_glsl]] 引用了未导出的枚举 `{enum_cpp}`")
            continue

        wanted = check.get("items")
        name_overrides = check.get("glsl_name", {})
        prefix = check.get("prefix", "")
        suffix = check.get("suffix", "")
        style = check.get("name_style")
        for item in block.items:
            if wanted and item.cpp_name not in wanted:
                continue
            glsl_name = name_overrides.get(item.cpp_name)
            if not glsl_name:
                glsl_name = (make_glsl_name(item.cpp_name, style, prefix, suffix)
                             if style else item.glsl_name)
            if glsl_name not in defines:
                problems.append(
                    f"{rel}: 缺少 `{glsl_name}` 的声明（真源 {enum_cpp}::{item.cpp_name}）")
                continue
            got = defines[glsl_name]
            got_int = _int_of(got)
            expected = block.glsl_value(item.value)
            if got_int is not None:
                if got_int != item.value:
                    problems.append(
                        f"{rel}: `{glsl_name} = {got}` ≠ 真源 {enum_cpp}::"
                        f"{item.cpp_name} = {item.value}（期望 {expected}）")
            elif got != expected:
                problems.append(
                    f"{rel}: `{glsl_name} = {got}` ≠ 期望文本 `{expected}`")
    return problems


SAMPLER_TABLE_HEADER_RE = re.compile(r"^\s*\[\[\s*samplers\s*\]\]\s*$")
SAMPLER_NAME_RE = re.compile(r"^\s*name\s*=\s*\"([^\"]+)\"\s*$")


def parse_sampler_toml(path: Path) -> list[str]:
    names: list[str] = []
    current: dict[str, str] | None = None
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if SAMPLER_TABLE_HEADER_RE.match(raw):
            if current is not None and "name" in current:
                names.append(current["name"])
            current = {}
            continue
        if current is None:
            continue
        m = SAMPLER_NAME_RE.match(raw)
        if m:
            current["name"] = m.group(1)
    if current is not None and "name" in current:
        names.append(current["name"])
    return names


def verify_require_include(entry: dict, repo_root: Path) -> list[str]:
    rel = entry["file"]
    include = entry["include"]
    path = repo_root / rel
    if not path.exists():
        return [f"{rel}: 文件不存在（[[require_include]] 目标缺失）"]

    text = path.read_text(encoding="utf-8", errors="replace")
    if f'#include "{include}"' not in text and f"#include <{include}>" not in text:
        return [
            f"{rel}: 缺少 `#include \"{include}\"` —— "
            "本文件的契约常量必须取自 ShaderConstGen 生成物，不得再内联裸数字"]
    return []


def verify_sampler_registry(entry: dict, repo_root: Path,
                            enums: dict[str, EnumBlock]) -> list[str]:
    rel = entry["file"]
    path = repo_root / rel
    if not path.exists():
        return [f"{rel}: 采样器注册表不存在"]

    enum_cpp = entry["enum"]
    block = enums.get(enum_cpp)
    if block is None:
        return [f"[[sampler_registry]] 引用了未导出的枚举 `{enum_cpp}`"]

    names = parse_sampler_toml(path)
    expected = [entry.get("item_name", {}).get(i.cpp_name, i.cpp_name) for i in block.items]

    problems: list[str] = []
    if len(names) != len(expected):
        problems.append(
            f"{rel}: 共 {len(names)} 个预设，但 {enum_cpp} 有 {len(expected)} 项 —— "
            "采样器序号会整体错位")
    for idx, (got, want) in enumerate(zip(names, expected)):
        if got != want:
            problems.append(
                f"{rel}: 第 {idx} 项是 `{got}`，但真源 {enum_cpp} 的第 {idx} 项是 "
                f"`{want}`（采样器序号错位 ⇒ 静默降级为 Nearest）")
            break
    return problems


# ── 主流程 ──────────────────────────────────────────────────────────────────

def discover_annotations(contract_dir: Path) -> set[str]:
    names: set[str] = set()
    for header in sorted(contract_dir.glob("*.h")):
        text = header.read_text(encoding="utf-8", errors="replace")
        for m in ANNOT_ENUM_RE.finditer(text):
            names.add(m.group(1))
    if not names:
        raise GenError(f"{contract_dir} 下没有任何 HGL_SHADER_ENUM 注解")
    return names


def write_or_check(repo_root: Path, rel: str, content: str, mode: str) -> str | None:
    path = repo_root / rel
    old = path.read_text(encoding="utf-8", errors="replace") if path.exists() else None
    if mode == "check":
        if old is None:
            return f"{rel}: 生成物不存在"
        if old.replace("\r\n", "\n") != content:
            return diff_hint(rel, old.replace("\r\n", "\n"), content)
        return None
    if old is not None and old.replace("\r\n", "\n") == content:
        return None            # 内容未变 → 不写盘，保持 mtime（增量构建友好）
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")
    return f"{rel}: 已刷新"


def diff_hint(rel: str, old: str, new: str) -> str:
    old_lines = old.splitlines()
    new_lines = new.splitlines()
    for idx in range(max(len(old_lines), len(new_lines))):
        a = old_lines[idx] if idx < len(old_lines) else "<EOF>"
        b = new_lines[idx] if idx < len(new_lines) else "<EOF>"
        if a != b:
            return f"{rel}: 第 {idx + 1} 行漂移\n  文件: {a}\n  生成: {b}"
    return f"{rel}: 内容漂移（仅行尾差异）"


def main(argv: list[str]) -> int:
    # 消息含中文：控制台若是 GBK（MSBuild / 传统 cmd 的默认），无法编码的字符不该炸掉
    # 生成过程，退化成 '?' 即可。
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(errors="replace")
        except (AttributeError, ValueError):
            pass

    ap = argparse.ArgumentParser(description="C++ ↔ GLSL 数值契约生成器")
    ap.add_argument("--repo-root", default=".")
    ap.add_argument("--config", default=DEFAULT_CONFIG)
    ap.add_argument("--clang", default=None)
    ap.add_argument("--include-dir", action="append", default=[])
    ap.add_argument("--work-dir", default=None,
                    help="探针 TU 的落盘目录（默认系统临时目录）")
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--write", action="store_true")
    group.add_argument("--check", action="store_true")
    args = ap.parse_args(argv)

    repo_root = Path(args.repo_root).resolve()
    config_path = Path(args.config)
    if not config_path.is_absolute():
        config_path = repo_root / config_path
    cfg = load_config(config_path)

    include_dirs = [str(repo_root / d) for d in (args.include_dir or DEFAULT_INCLUDE_DIRS)]

    clang_exe = args.clang or cfg.get("clang") or "clang++"
    clang = Clang(clang_exe, include_dirs)

    contract_dir = repo_root / CONTRACT_DIR
    annotated = discover_annotations(contract_dir)

    # 探针 TU：只 include 契约头（自包含，无需引擎其它模块）。
    # 落在临时目录，避免往仓库里塞中间产物（--work-dir 可指定便于调试）。
    work_dir = Path(args.work_dir) if args.work_dir else Path(tempfile.mkdtemp(
        prefix="shader_const_gen_"))
    work_dir.mkdir(parents=True, exist_ok=True)
    headers = sorted(p.name for p in contract_dir.glob("*.h")
                     if p.name != "ShaderContractMacros.h")
    probe = work_dir / "shader_const_probe.cpp"
    probe.write_text("".join(f"#include <hgl/shader/contract/{h}>\n" for h in headers),
                     encoding="utf-8", newline="\n")

    ast_nodes = clang.ast_json(probe, cfg.get("ast_filter", "hgl::shader::contract::"))

    glsl_files, enums = build_glsl_files(cfg, ast_nodes, annotated)

    glsl_dir = cfg.get("output", {}).get("glsl_dir", "ShaderLibrary/contract")

    reports: list[str] = []
    problems: list[str] = []

    for gfile in glsl_files:
        rel = f"{glsl_dir}/{gfile.name}"
        msg = write_or_check(repo_root, rel, render_glsl(gfile),
                             "check" if args.check else "write")
        if msg:
            (problems if args.check else reports).append(msg)

    parity_rel = cfg.get("output", {}).get(
        "parity_header", "inc/hgl/shader/generated/ShaderConstantsParity.gen.h")
    msg = write_or_check(repo_root, parity_rel, render_parity(cfg, enums),
                         "check" if args.check else "write")
    if msg:
        (problems if args.check else reports).append(msg)

    # 只读校验（--write 也要做：真源漂移必须立刻暴露，而不是等 CI）
    for entry in cfg.get("verify_glsl", []):
        problems.extend(verify_glsl(entry, repo_root, enums))
    for entry in cfg.get("sampler_registry", []):
        problems.extend(verify_sampler_registry(entry, repo_root, enums))
    for entry in cfg.get("require_include", []):
        problems.extend(verify_require_include(entry, repo_root))

    for line in reports:
        print(f"[ShaderConstGen] {line}")
    if problems:
        for line in problems:
            print(f"[ShaderConstGen] 错误：{line}", file=sys.stderr)
        return 1

    print(f"[ShaderConstGen] OK：{len(enums)} 个契约枚举，"
          f"{sum(len(b.items) for b in enums.values())} 个常量")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
