#!/usr/bin/env python3
"""EnumGen 的单元测试（不依赖 clang）。

覆盖的是「配置 ↔ AST」之间的约束：
  · RANGE 哨兵必须被剔除；
  · display 键必须真实存在（枚举改名后必须同步配置，不允许静默失效）；
  · 生成的名称表、下标枚举、数量断言与成员一一对应。

运行：
    python src/Tools/ASTTooling/test_enum_gen.py
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import enum_gen as eg  # noqa: E402


def make_enum(name: str, members: list[str]) -> dict:
    return {
        "kind": "EnumDecl",
        "name": name,
        "inner": [{"kind": "EnumConstantDecl", "name": m} for m in members],
    }


class TestEnumMembers(unittest.TestCase):
    def test_strips_range_sentinels(self):
        decl = make_enum("Algorithm", ["A", "B", "BEGIN_RANGE", "END_RANGE", "RANGE_SIZE"])
        self.assertEqual(eg.enum_members(decl), ["A", "B"])

    def test_preserves_declaration_order(self):
        decl = make_enum("E", ["Zeta", "Alpha", "Mu"])
        self.assertEqual(eg.enum_members(decl), ["Zeta", "Alpha", "Mu"])

    def test_rejects_empty(self):
        with self.assertRaises(eg.GenError):
            eg.enum_members(make_enum("E", ["BEGIN_RANGE", "END_RANGE", "RANGE_SIZE"]))

    def test_rejects_duplicates(self):
        with self.assertRaises(eg.GenError):
            eg.enum_members(make_enum("E", ["A", "A"]))

    def test_rejects_anonymous(self):
        decl = {"kind": "EnumDecl", "name": "E",
                "inner": [{"kind": "EnumConstantDecl", "name": ""}]}
        with self.assertRaises(eg.GenError):
            eg.enum_members(decl)


class TestRender(unittest.TestCase):
    def target(self, display=None) -> eg.Target:
        return eg.Target(
            name="HashAlgorithm",
            header="CMHash/inc/hgl/util/hash/Algorithm.h",
            enum="Algorithm",
            namespace="hgl::util::hash",
            output="CMHash/inc/hgl/util/hash/generated/AlgorithmRegistry.gen.h",
            include_header="hgl/util/hash/Algorithm.h",
            display=display or {},
        )

    def test_names_and_indices(self):
        text = eg.render(self.target(), ["Adler32", "MD5"], "tool.py")
        self.assertIn("constexpr size_t kAlgorithmCount = 2;", text)
        self.assertIn('"Adler32",', text)
        self.assertIn('"MD5",', text)
        self.assertIn("ALGORITHM_INDEX_Adler32 = 0,", text)
        self.assertIn("ALGORITHM_INDEX_MD5 = 1,", text)
        self.assertIn("size_t(Algorithm::RANGE_SIZE) == generated::kAlgorithmCount", text)

    def test_display_override(self):
        text = eg.render(self.target({"XXH3_64": "XXH3-64"}), ["XXH3_64"], "tool.py")
        self.assertIn('"XXH3-64",', text)
        self.assertNotIn('"XXH3_64",', text)

    def test_unknown_display_key_is_rejected(self):
        # 枚举项改名后配置没有同步 —— 必须失败，而不是生成一张对不齐的表
        with self.assertRaises(eg.GenError):
            eg.render(self.target({"RemovedItem": "X"}), ["StillHere"], "tool.py")

    def test_output_is_deterministic(self):
        members = ["B", "A", "C"]
        t = self.target()
        self.assertEqual(eg.render(t, members, "tool.py"), eg.render(t, members, "tool.py"))


class TestConfig(unittest.TestCase):
    def test_repository_config_parses(self):
        targets = eg.load_config(eg.DEFAULT_CONFIG)
        self.assertTrue(targets)
        hash_target = next(t for t in targets if t.name == "HashAlgorithm")
        self.assertEqual(hash_target.enum, "Algorithm")
        self.assertEqual(hash_target.display["XXH3_64"], "XXH3-64")


if __name__ == "__main__":
    unittest.main(verbosity=2)
