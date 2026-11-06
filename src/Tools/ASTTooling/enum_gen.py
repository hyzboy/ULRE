#!/usr/bin/env python3
"""EnumGen —— 由 Clang AST 派生的枚举注册表代码生成器。

设计原则
========
* **枚举声明是唯一真源**：成员名称、成员数量一律从 Clang AST 读取，
  本工具与配置文件都不允许再手工罗列一遍成员。
* **配置只承载 AST 无法推断的信息**，并且必须以枚举项名称为键（不是数组下标），
  因此错位、漏项、多项都会在校验阶段直接失败，而不是悄悄生成一张对不齐的表。
* **产物提交进仓库**：MSVC 构建不需要 Clang/Python，`--check` 用于 CI 与
  本地校验“提交的产物是否仍与源码一致”。

产物形态（每个 target 一份）
============================
    constexpr size_t          k<Enum>Count;
    constexpr const char *const k<Enum>Names[k<Enum>Count];
    enum <Enum>Index : size_t { <ENUM>_INDEX_<Member> = 0, ... };
    static_assert(size_t(<Qualified>::RANGE_SIZE) == k<Enum>Count, ...);

用法
====
    python src/Tools/ASTTooling/enum_gen.py                # 生成并写盘
    python src/Tools/ASTTooling/enum_gen.py --check        # 只校验，不写盘
    python src/Tools/ASTTooling/enum_gen.py --only HashAlgorithm
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_CONFIG = Path(__file__).resolve().parent / "enum_gen.toml"

SENTINEL_NAMES = {"BEGIN_RANGE", "END_RANGE", "RANGE_SIZE"}


class GenError(RuntimeError):
    pass


# ── Clang ────────────────────────────────────────────────────────────────────
def find_clang(explicit: str = "") -> str:
    if explicit:
        return explicit

    env = os.environ.get("HGL_CLANG", "")
    if env:
        return env

    found = shutil.which("clang++") or shutil.which("clang")
    if found:
        return found

    for cand in (
        r"C:\Program Files\LLVM\bin\clang++.exe",
        r"C:\LLVM\bin\clang++.exe",
        r"D:\LLVM\bin\clang++.exe",
        "/usr/bin/clang++",
        "/usr/local/bin/clang++",
        "/opt/homebrew/opt/llvm/bin/clang++",
    ):
        if Path(cand).is_file():
            return cand

    raise GenError("找不到 clang++：请用 --clang 指定或设置 HGL_CLANG 环境变量")


def dump_enum(clang: str, header: Path, enum_name: str, include_dirs: list[Path]) -> dict:
    cmd = [clang, "-x", "c++", "-std=c++20", "-fsyntax-only", "-w"]
    for inc in include_dirs:
        cmd += ["-I", str(inc)]
    cmd += ["-Xclang", "-ast-dump=json", "-Xclang", f"-ast-dump-filter={enum_name}"]
    cmd.append(str(header))

    proc = subprocess.run(cmd, capture_output=True, text=True,
                          encoding="utf-8", errors="replace")
    if proc.returncode != 0:
        raise GenError(f"clang AST 失败（{header.name}:{enum_name}）：\n{proc.stderr.strip()[:1500]}")

    decoder = json.JSONDecoder()
    text = proc.stdout
    pos = 0
    while pos < len(text):
        while pos < len(text) and text[pos] in " \t\r\n":
            pos += 1
        if pos >= len(text):
            break
        node, pos = decoder.raw_decode(text, pos)
        enum_decl = _find_enum(node, enum_name)
        if enum_decl is not None:
            return enum_decl

    raise GenError(f"在 {header} 中找不到 enum {enum_name} 的完整定义")


def _find_enum(node, name: str) -> dict | None:
    if not isinstance(node, dict):
        return None
    if node.get("kind") == "EnumDecl" and node.get("name") == name:
        return node
    for child in node.get("inner", []) or []:
        found = _find_enum(child, name)
        if found is not None:
            return found
    return None


def enum_members(enum_decl: dict) -> list[str]:
    """按声明顺序返回真实成员名（剔除 BEGIN_RANGE/END_RANGE/RANGE_SIZE）。"""
    members: list[str] = []
    for child in enum_decl.get("inner", []) or []:
        if child.get("kind") != "EnumConstantDecl":
            continue
        name = child.get("name", "")
        if not name:
            raise GenError("枚举中出现匿名成员（无法派生命名）")
        if name in SENTINEL_NAMES:
            continue
        members.append(name)

    if not members:
        raise GenError(f"enum {enum_decl.get('name')} 没有任何真实成员")

    dup = {n for n in members if members.count(n) > 1}
    if dup:
        raise GenError(f"enum {enum_decl.get('name')} 存在重复成员：{sorted(dup)}")

    return members


# ── 配置 ─────────────────────────────────────────────────────────────────────
@dataclass
class Target:
    name: str
    header: str
    enum: str
    namespace: str
    output: str
    include_header: str = ""
    display: dict[str, str] = field(default_factory=dict)
    extra_includes: list[str] = field(default_factory=list)


def load_config(path: Path) -> list[Target]:
    if not path.is_file():
        raise GenError(f"找不到配置文件：{path}")

    with path.open("rb") as fp:
        data = tomllib.load(fp)

    targets: list[Target] = []
    for raw in data.get("target", []):
        required = ("name", "header", "enum", "namespace", "output")
        missing = [k for k in required if not raw.get(k)]
        if missing:
            raise GenError(f"[{raw.get('name', '?')}] 缺少配置项：{', '.join(missing)}")

        display = {}
        for item in raw.get("display", []):
            key, value = item.get("member"), item.get("name")
            if not key or not value:
                raise GenError(f"[{raw['name']}] display 项必须同时给出 member 与 name")
            if key in display:
                raise GenError(f"[{raw['name']}] display 中 member 重复：{key}")
            display[key] = value

        targets.append(Target(
            name=raw["name"],
            header=raw["header"],
            enum=raw["enum"],
            namespace=raw["namespace"],
            output=raw["output"],
            include_header=raw.get("include_header", ""),
            display=display,
            extra_includes=raw.get("extra_includes", []),
        ))

    if not targets:
        raise GenError("配置里没有任何 [[target]]")

    return targets


# ── 渲染 ─────────────────────────────────────────────────────────────────────
def render(target: Target, members: list[str], tool_rel: str) -> str:
    unknown = sorted(set(target.display) - set(members))
    if unknown:
        raise GenError(
            f"[{target.name}] display 配置了不存在的枚举项：{', '.join(unknown)}"
            "（枚举改名后必须同步配置，否则会出现静默失效的键）")

    enum_short = target.enum
    count_name = f"k{enum_short}Count"
    names_name = f"k{enum_short}Names"
    index_name = f"{enum_short}Index"
    index_prefix = re.sub(r"(?<!^)(?=[A-Z])", "_", enum_short).upper()

    lines: list[str] = []
    lines.append("// 本文件由 Clang AST 自动生成，请勿手工编辑。")
    lines.append("//")
    lines.append(f"//   生成器：{tool_rel}")
    lines.append("//   重新生成：scripts/run-hashgen.ps1")
    lines.append("//   校验一致：scripts/run-hashgen.ps1 -Check")
    lines.append("//")
    lines.append(f"// 真源：{target.header} 中的 enum {target.enum}")
    lines.append("// 成员列表、数量与顺序全部直接来自 AST，不存在第二份手工清单。")
    lines.append("#pragma once")
    lines.append("")

    for inc in ["hgl/CoreType.h", target.include_header, *target.extra_includes]:
        if inc:
            lines.append(f"#include<{inc}>")
    lines.append("")

    lines.append(f"namespace {target.namespace}")
    lines.append("{")
    lines.append("    namespace generated")
    lines.append("    {")
    lines.append(f"        /// enum {target.enum} 的真实成员数量（已剔除 RANGE 哨兵）")
    lines.append(f"        constexpr size_t {count_name} = {len(members)};")
    lines.append("")
    lines.append("        /// 与枚举声明顺序一一对应的规范名称")
    lines.append(f"        constexpr const char *const {names_name}[{count_name}] =")
    lines.append("        {")
    for member in members:
        display = target.display.get(member, member)
        escaped = display.replace("\\", "\\\\").replace('"', '\\"')
        lines.append(f'            "{escaped}",')
    lines.append("        };")
    lines.append("")
    lines.append("        /// 每个枚举项的稳定索引，供按索引初始化的信息表使用")
    lines.append(f"        enum {index_name} : size_t")
    lines.append("        {")
    for i, member in enumerate(members):
        lines.append(f"            {index_prefix}_INDEX_{member} = {i},")
    lines.append("        };")
    lines.append("    }//namespace generated")
    lines.append("")
    lines.append(f"    static_assert(size_t({enum_short}::RANGE_SIZE) == generated::{count_name},")
    lines.append(f'                  "{enum_short} 的 RANGE_SIZE 与 Clang AST 扫描到的成员数量不一致：'
                 '枚举声明与生成物已经脱节，请重新运行 scripts/run-hashgen.ps1");')
    lines.append("")
    lines.append(f"}}//namespace {target.namespace}")
    lines.append("")

    return "\n".join(lines)


# ── 主流程 ───────────────────────────────────────────────────────────────────
def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Clang AST 驱动的枚举注册表生成器")
    parser.add_argument("--check", action="store_true", help="只校验不写盘")
    parser.add_argument("--only", default="", help="只处理指定 target")
    parser.add_argument("--clang", default="", help="clang++ 路径")
    parser.add_argument("--config", default=str(DEFAULT_CONFIG), help="配置文件")
    parser.add_argument("-I", "--include-dir", action="append", default=[], help="额外 include 目录")
    args = parser.parse_args(argv)

    targets = load_config(Path(args.config))
    if args.only:
        targets = [t for t in targets if t.name == args.only]
        if not targets:
            raise GenError(f"没有名为 {args.only} 的 target")

    clang = find_clang(args.clang)

    includes = [REPO_ROOT / "CMCoreType" / "inc"] + [Path(p) for p in args.include_dir]
    tool_rel = Path(__file__).resolve().relative_to(REPO_ROOT).as_posix()

    stale: list[Path] = []

    for target in targets:
        header = REPO_ROOT / target.header
        if not header.is_file():
            raise GenError(f"[{target.name}] 找不到头文件：{header}")

        decl = dump_enum(clang, header, target.enum, includes)
        members = enum_members(decl)
        text = render(target, members, tool_rel)

        out_path = REPO_ROOT / target.output
        out_path.parent.mkdir(parents=True, exist_ok=True)

        if args.check:
            existing = out_path.read_text(encoding="utf-8") if out_path.is_file() else ""
            if existing.replace("\r\n", "\n") != text:
                stale.append(out_path)
                print(f"[EnumGen] 过期：{target.output}", file=sys.stderr)
            else:
                print(f"[EnumGen] 一致：{target.output} ({len(members)} 项)")
        else:
            with out_path.open("w", encoding="utf-8", newline="\n") as fp:
                fp.write(text)
            print(f"[EnumGen] 已生成：{target.output} ({len(members)} 项)")

    if stale:
        print("[EnumGen] 生成物与源码不一致，请运行 scripts/run-hashgen.ps1", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except GenError as exc:
        print(f"[EnumGen] 错误：{exc}", file=sys.stderr)
        sys.exit(2)
