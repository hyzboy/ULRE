import unittest

import render_regression


class FixtureAnalysisTests(unittest.TestCase):
    def test_normal_and_surface_assertions(self):
        normal = {"sample_count": 1, "nan_count": 0, "inf_count": 0,
                  "zero_normal_ratio": 0, "length_error_ratio": 0}
        render_regression._check_fixture_analysis(normal, "world_normal")
        with self.assertRaisesRegex(RuntimeError, "normal_length"):
            render_regression._check_fixture_analysis(
                dict(normal, zero_normal_ratio=0.1), "world_normal")
        with self.assertRaisesRegex(RuntimeError, "no_finite_samples"):
            render_regression._check_fixture_analysis(
                dict(normal, inf_count=1), "world_normal")
        surface = {"sample_count": 1, "metallic": {"mean": 0.08}}
        render_regression._check_fixture_analysis(surface, "surface")
        with self.assertRaisesRegex(RuntimeError, "metallic"):
            render_regression._check_fixture_analysis(
                dict(surface, metallic={"mean": 0.3}), "surface")

    def test_position_requires_crosscheck_samples(self):
        with self.assertRaisesRegex(RuntimeError, "no_samples"):
            render_regression._check_fixture_analysis(
                {"crosscheck": {"point_count": 0}}, "world_position")


if __name__ == "__main__":
    unittest.main()
