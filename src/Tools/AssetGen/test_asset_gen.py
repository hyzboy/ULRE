#!/usr/bin/env python3
"""Focused tests for AssetGen policy and input validation."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path

sys.dont_write_bytecode = True

MODULE_PATH = Path(__file__).with_name("asset_gen.py")
SPEC = importlib.util.spec_from_file_location("asset_gen_under_test", MODULE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load AssetGen from {MODULE_PATH}")
asset_gen = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = asset_gen
SPEC.loader.exec_module(asset_gen)


def make_property() -> asset_gen.PropertyInfo:
    return asset_gen.PropertyInfo(
        name="value",
        toml_key="value",
        display_name="value",
        category="Default",
        tooltip="",
        cpp_type="uint32_t",
        offset=0,
        size=4,
        type=asset_gen.TypeDesc(category="scalar"),
    )


class AssetGenTests(unittest.TestCase):
    def test_edit_only_is_toml_only(self) -> None:
        prop = make_property()
        asset_gen.apply_property_tags(prop, ["EditOnly"], "")

        self.assertTrue(prop.serialized_toml)
        self.assertFalse(prop.serialized_flatbuffers)

    def test_transient_is_not_serialized(self) -> None:
        prop = make_property()
        asset_gen.apply_property_tags(prop, ["Transient"], "")

        self.assertFalse(prop.serialized_toml)
        self.assertFalse(prop.serialized_flatbuffers)

    def test_invalid_editor_metadata_fails_clearly(self) -> None:
        prop = make_property()
        with self.assertRaises(asset_gen.GenError):
            asset_gen.apply_property_tags(prop, ["Range(4, 1)"], "")

        with self.assertRaises(asset_gen.GenError):
            asset_gen.apply_property_tags(prop, ["Step = nan"], "")

    def test_fixed_array_requires_literal_positive_length(self) -> None:
        registry = asset_gen.TypeRegistry()
        with self.assertRaises(asset_gen.GenError):
            registry.classify("float[Count]")
        with self.assertRaises(asset_gen.GenError):
            registry.classify("float[0]")

    def test_codec_header_splits_into_toml_and_fbs(self) -> None:
        toml_header, fbs_header = asset_gen._codec_split_paths(
            "inc/hgl/asset/generated/FooCodec.gen.h")
        self.assertEqual(toml_header, "inc/hgl/asset/generated/FooToml.gen.h")
        self.assertEqual(fbs_header, "inc/hgl/asset/generated/FooFbs.gen.h")

    def test_codec_header_requires_expected_suffix(self) -> None:
        with self.assertRaises(asset_gen.GenError):
            asset_gen._codec_split_paths("inc/hgl/asset/generated/Foo.gen.h")

    def test_toml_facade_has_no_flatbuffers_dependency(self) -> None:
        generated = Path(__file__).resolve().parents[3] / "inc" / "hgl" / "asset" / "generated"
        toml_headers = sorted(generated.glob("*Toml.gen.h"))
        if not toml_headers:
            self.skipTest("生成产物尚未落盘")

        for header in toml_headers:
            text = header.read_text(encoding="utf-8")
            for token in ("flatbuffers", "FlatCodec", "FbType", "Offset<"):
                self.assertNotIn(token, text, f"{header.name} 不应依赖 {token}")


if __name__ == "__main__":
    unittest.main()
