#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DomainSlotGen —— 材质领域（MaterialDomain）纹理槽派生代码生成器

唯一真源：`ShaderLibrary/contract/material_domain_slots.toml`
本工具解析 TOML 声明式契约，自动派生：
  1. C++ 契约头文件: `inc/hgl/mtl/generated/MaterialDomainSlots.gen.h`
  2. GLSL 绑定片段:
       - `ShaderLibrary/domain/generated/<domain_snake>_texture_bindings.glsl`

用法
----
    python src/Tools/DomainSlotGen/domain_slot_gen.py --repo-root . --write
    python src/Tools/DomainSlotGen/domain_slot_gen.py --repo-root . --check

`--check` 只比对不落盘：若磁盘内容与生成物不一致，打印首个差异并返回 1。
"""

from __future__ import annotations

import argparse
import difflib
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - py<3.11
    import tomli as tomllib  # type: ignore

CONTRACT_TOML = "ShaderLibrary/contract/material_domain_slots.toml"
CPP_HEADER_OUTPUT = "inc/hgl/mtl/generated/MaterialDomainSlots.gen.h"
GLSL_OUTPUT_DIR = "ShaderLibrary/domain/generated"


DOMAIN_FILENAME_OVERRIDE = {
    "UI2D": "ui_2d",
    "Text2D": "text_2d",
    "UnlitTexture": "unlit_texture",
    "UnlitTexture3D": "unlit_texture_3d",
}


def domain_to_snake(domain: str) -> str:
    if domain in DOMAIN_FILENAME_OVERRIDE:
        return DOMAIN_FILENAME_OVERRIDE[domain]
    s1 = re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1_\2', domain)
    s2 = re.sub(r'([a-z\d])([A-Z])', r'\1_\2', s1).lower()
    return s2


def camel_to_upper_snake(name: str) -> str:
    return domain_to_snake(name).upper()


@dataclass
class SlotDef:
    name: str
    role: str
    must_declare: bool
    sampler: str
    binding_required: bool
    channels: int


@dataclass
class DomainSlotsDef:
    domain: str
    description: str
    slots: list[SlotDef] = field(default_factory=list)
    ref_slots: str | None = None

    @property
    def slot_count(self) -> int:
        return len(self.slots)

    @property
    def row_stride(self) -> int:
        if self.slot_count == 0:
            return 0
        # 每个槽为 uvec2 = 8 字节，按 16 字节向上取整对齐
        raw_size = self.slot_count * 8
        return (raw_size + 15) & ~15


def load_contracts(toml_path: Path) -> list[DomainSlotsDef]:
    with open(toml_path, "rb") as f:
        data = tomllib.load(f)

    domain_map: dict[str, DomainSlotsDef] = {}
    domains_list: list[DomainSlotsDef] = []

    for entry in data.get("domains", []):
        domain_name = entry["domain"]
        desc = entry.get("description", "")
        ref = entry.get("ref_slots")

        slots: list[SlotDef] = []
        for s in entry.get("slots", []):
            slots.append(
                SlotDef(
                    name=s["name"],
                    role=s["role"],
                    must_declare=bool(s.get("must_declare", False)),
                    sampler=s["sampler"],
                    binding_required=bool(s.get("binding_required", False)),
                    channels=int(s.get("channels", 0)),
                )
            )

        item = DomainSlotsDef(
            domain=domain_name,
            description=desc,
            slots=slots,
            ref_slots=ref,
        )
        domain_map[domain_name] = item
        domains_list.append(item)

    # 解析 ref_slots 引用
    for item in domains_list:
        if item.ref_slots:
            target = domain_map.get(item.ref_slots)
            if not target:
                raise ValueError(
                    f"Domain {item.domain} references unknown ref_slots '{item.ref_slots}'"
                )
            item.slots = list(target.slots)

    return domains_list


def generate_cpp_header(domains: list[DomainSlotsDef]) -> str:
    lines: list[str] = [
        "// ============================================================================",
        "// MaterialDomainSlots.gen.h —— 材质领域纹理槽规范契约（自动生成代码）",
        "//",
        "// 唯一真源：ShaderLibrary/contract/material_domain_slots.toml",
        "// 由 src/Tools/DomainSlotGen/domain_slot_gen.py 自动生成，请勿手动修改！",
        "// ============================================================================",
        "#pragma once",
        "",
        "#include <hgl/mtl/MaterialDomain.h>",
        "#include <hgl/common/TextureSamplerTypeDef.h>",
        "",
        "namespace hgl::graph::mtl",
        "{",
        "    namespace generated",
        "    {",
    ]

    # 生成各个独立数组定义
    for d in domains:
        if d.ref_slots:
            continue
        if d.slot_count == 0:
            continue

        lines.append(f"        // {d.domain} —— {d.description}")
        lines.append(f"        // 槽数: {d.slot_count}, 行跨度(stride): {d.row_stride} 字节 (16字节对齐)")
        lines.append(f"        constexpr MaterialDomainTextureSlot {d.domain}Slots[] = {{")
        for s in d.slots:
            must_dec = "true" if s.must_declare else "false"
            bind_req = "true" if s.binding_required else "false"
            lines.append(
                f'            {{ "{s.name}", MaterialDomainTextureRole::{s.role}, {must_dec}, '
                f"SamplerType::{s.sampler}, {bind_req}, {s.channels} }},"
            )
        lines.append("        };")
        lines.append(f"        constexpr int {d.domain}SlotCount = static_cast<int>(sizeof({d.domain}Slots) / sizeof({d.domain}Slots[0]));")
        lines.append(f"        constexpr uint32_t {d.domain}RowStride = {d.row_stride}u;")
        lines.append("")

    # 针对 ref_slots 领域生成别名
    for d in domains:
        if d.ref_slots:
            ref_domain = d.ref_slots
            lines.append(f"        // {d.domain} —— 复用 {ref_domain} 的纹理槽")
            lines.append(f"        constexpr const MaterialDomainTextureSlot *{d.domain}Slots = {ref_domain}Slots;")
            lines.append(f"        constexpr int {d.domain}SlotCount = {ref_domain}SlotCount;")
            lines.append(f"        constexpr uint32_t {d.domain}RowStride = {ref_domain}RowStride;")
            lines.append("")

    lines.append("        struct DomainSlotInfo")
    lines.append("        {")
    lines.append("            const MaterialDomainTextureSlot *slots;")
    lines.append("            int count;")
    lines.append("            uint32_t row_stride;")
    lines.append("        };")
    lines.append("")
    lines.append("        constexpr DomainSlotInfo GetDomainSlotInfo(MaterialDomain domain) noexcept")
    lines.append("        {")
    lines.append("            switch (domain)")
    lines.append("            {")
    for d in domains:
        if d.slot_count > 0:
            lines.append(f"            case MaterialDomain::{d.domain}:")
            lines.append(f"                return {{ {d.domain}Slots, {d.domain}SlotCount, {d.domain}RowStride }};")
    lines.append("            default:")
    lines.append("                return { nullptr, 0, 0u };")
    lines.append("            }")
    lines.append("        }")
    lines.append("")
    lines.append("    } // namespace generated")
    lines.append("} // namespace hgl::graph::mtl")
    lines.append("")

    return "\n".join(lines)


def generate_glsl_binding(d: DomainSlotsDef) -> str:
    snake = domain_to_snake(d.domain)
    upper_snake = camel_to_upper_snake(d.domain)
    guard = f"HGL_DOMAIN_{upper_snake}_TEXTURE_BINDINGS_GLSL"

    lines: list[str] = [
        f"// ============================================================================",
        f"// {snake}_texture_bindings.glsl —— {d.domain} 纹理槽引用布局（自动生成代码）",
        f"//",
        f"// 唯一真源：ShaderLibrary/contract/material_domain_slots.toml",
        f"// 由 src/Tools/DomainSlotGen/domain_slot_gen.py 自动生成，请勿手动修改！",
        f"// ============================================================================",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        f"#define HGL_{upper_snake}_TEXTURE_SLOT_COUNT {d.slot_count}",
        f"#define HGL_{upper_snake}_TEXTURE_ROW_STRIDE {d.row_stride}",
        "",
    ]

    if d.slot_count > 0:
        lines.append("// ── 材质贴图表（Buffer Reference） ──")
        lines.append(f"// 槽数: {d.slot_count}, 行跨度: {d.row_stride} 字节 (16字节对齐)")
        lines.append("layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialTextureReferencesRef")
        lines.append("{")
        for s in d.slots:
            lines.append(f"    uvec2 tex_{s.name};")
        lines.append("};")
        lines.append("")
        has_channel_macro = False
        for s in d.slots:
            if s.channels > 0:
                slot_upper = s.name.upper()
                lines.append(f"#ifndef MTL_TEX_{slot_upper}_CHANNELS")
                lines.append(f"#define MTL_TEX_{slot_upper}_CHANNELS {s.channels}")
                lines.append(f"#endif")
                has_channel_macro = True
        if has_channel_macro:
            lines.append("")
        lines.append(
            f"#define MTL_TEX_{upper_snake}(i) MaterialTextureReferencesRef("
            f"pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t({d.row_stride}))"
        )
        lines.append("")
        lines.append("#ifndef MTL_TEX")
        lines.append(f"#define MTL_TEX(i) MTL_TEX_{upper_snake}(i)")
        lines.append("#endif")
        lines.append("")

    lines.append(f"#endif // {guard}")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path("."),
        help="ULRE 仓库根目录路径",
    )
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--write",
        action="store_true",
        help="将生成内容写入目标文件",
    )
    group.add_argument(
        "--check",
        action="store_true",
        help="仅校验磁盘文件是否与生成内容一致（CI / 门禁用）",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    toml_path = repo_root / CONTRACT_TOML
    if not toml_path.exists():
        print(f"Error: Contract file not found: {toml_path}", file=sys.stderr)
        return 1

    try:
        domains = load_contracts(toml_path)
    except Exception as e:
        print(f"Error parsing {toml_path}: {e}", file=sys.stderr)
        return 1

    # 1. 生成 C++ 头文件
    cpp_content = generate_cpp_header(domains)
    cpp_out_path = repo_root / CPP_HEADER_OUTPUT

    # 2. 生成各领域 GLSL 绑定片段
    outputs: dict[Path, str] = {
        cpp_out_path: cpp_content,
    }

    glsl_dir = repo_root / GLSL_OUTPUT_DIR
    for d in domains:
        if d.slot_count > 0 and not d.ref_slots:
            snake = domain_to_snake(d.domain)
            glsl_path = glsl_dir / f"{snake}_texture_bindings.glsl"
            outputs[glsl_path] = generate_glsl_binding(d)

    # 校验或写入
    any_diff = False
    for path, content in outputs.items():
        existing = path.read_text(encoding="utf-8") if path.exists() else ""
        if existing != content:
            any_diff = True
            if args.check:
                print(f"[Diff detected] {path}")
                diff = difflib.unified_diff(
                    existing.splitlines(keepends=True),
                    content.splitlines(keepends=True),
                    fromfile=f"a/{path.relative_to(repo_root)}",
                    tofile=f"b/{path.relative_to(repo_root)}",
                )
                sys.stdout.writelines(diff)
            elif args.write:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding="utf-8")
                print(f"[Generated] {path.relative_to(repo_root)}")
            else:
                print(f"[Pending update] {path.relative_to(repo_root)} (use --write to apply)")

    if args.check and any_diff:
        return 1

    if not any_diff:
        print("All domain slot generated files are up to date.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
