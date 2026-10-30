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


if __name__ == "__main__":
    unittest.main()
