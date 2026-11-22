import os
import struct
import tempfile

import analyzer


def write_rgba(path, width, height, value):
    pixel = bytes((value[0], value[1], value[2], 255))
    with open(path, "wb") as stream:
        stream.write(pixel * (width * height))


def write_depth(path, width, height, value):
    with open(path, "wb") as stream:
        stream.write(struct.pack("<" + "f" * (width * height), *([value] * (width * height))))


def check(name, condition, detail):
    if not condition:
        raise AssertionError(name + ": " + detail)
    print("[PASS] " + name)


def main():
    with tempfile.TemporaryDirectory(prefix="ulre_mcp_") as directory:
        frame_a = os.path.join(directory, "frame_a.raw")
        frame_b = os.path.join(directory, "frame_b.raw")
        depth = os.path.join(directory, "depth.raw")

        write_rgba(frame_a, 8, 8, (128, 128, 128))
        write_rgba(frame_b, 8, 8, (129, 128, 128))
        write_depth(depth, 8, 8, 0.5)

        luminance = analyzer.analyze_luminance(frame_a, 8, 8, "rgba8")
        check("luminance mean", luminance["valid"] and luminance["mean"] > 0.49,
              "unexpected luminance result")

        normals = analyzer.analyze_normals(frame_a, 8, 8, "rgba8")
        check("normal zero-vector detection",
              normals["zero_normal_ratio"] > 0.99,
              "gray debug normal was not classified as zero")

        surface = analyzer.analyze_surface(frame_a, 8, 8, "rgba8")
        check("surface statistics",
              surface["valid"] and surface["roughness"]["mean"] > 0.49,
              "unexpected surface statistics")

        depth_stats = analyzer.analyze_depth(depth, 8, 8, True)
        check("depth effective ratio",
              depth_stats["effective_depth_ratio"] == 1.0,
              "depth capture was not fully effective")

        inverse_vp = [
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0,
        ]
        world = analyzer.reconstruct_world_pos(depth, 8, 8, inverse_vp, True, 8)
        check("world-position reconstruction",
              world["valid"] and world["point_count"] > 0,
              "world-position reconstruction returned no points")

        diff = analyzer.compare_frames(frame_a, frame_b, 8, 8, "rgba8", 0.001)
        check("frame diff output",
              diff["valid"] and diff["changed_ratio"] > 0.0 and diff["heatmap_path"].endswith(".pgm"),
              "frame diff did not produce a changed-pixel result")

    print("[PASS] ULRE MCP selftest complete")


if __name__ == "__main__":
    main()
