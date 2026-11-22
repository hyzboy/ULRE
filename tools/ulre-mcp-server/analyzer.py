import math
import struct
import tempfile
from pathlib import Path


def _read_raw(path):
    data = Path(path).read_bytes()
    if not data:
        raise ValueError("Capture file is empty")
    return data


def _float32_channels(data, width, height, pixel_format):
    if not isinstance(width, int) or isinstance(width, bool) or width <= 0:
        raise ValueError("width must be a positive integer")
    if not isinstance(height, int) or isinstance(height, bool) or height <= 0:
        raise ValueError("height must be a positive integer")
    count = width * height
    sizes = {"rgba8": 4, "bgra8": 4, "rgb8": 3,
             "a2bgr10": 4, "rgb10a2": 4, "a2r10g10b10": 4,
             "r32f": 4}
    if pixel_format not in sizes:
        raise ValueError(f"Unsupported pixel format: {pixel_format}")
    pixel_size = sizes[pixel_format]
    if len(data) != count * pixel_size:
        raise ValueError("Capture byte count does not match dimensions and format")
    if pixel_format == "r32f":
        values = struct.unpack("<" + "f" * count, data)
        return [(v,) for v in values]

    channels = []
    for offset in range(0, count * pixel_size, pixel_size):
        if pixel_format in ("rgba8", "rgb8"):
            channels.append(tuple(data[offset + i] / 255.0 for i in range(3)))
        elif pixel_format == "bgra8":
            channels.append(tuple(data[offset + i] / 255.0 for i in (2, 1, 0)))
        else:
            packed = struct.unpack_from("<I", data, offset)[0]
            r = ((packed >> 0) & 0x3FF) / 1023.0
            g = ((packed >> 10) & 0x3FF) / 1023.0
            b = ((packed >> 20) & 0x3FF) / 1023.0
            if pixel_format == "a2r10g10b10":
                r, b = b, r
            channels.append((r, g, b))
    return channels


def _percentile(values, percentile):
    if not values:
        return 0.0
    ordered = sorted(values)
    position = (len(ordered) - 1) * percentile
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def analyze_luminance(path, width, height, pixel_format="rgba8", color_space="linear"):
    if color_space not in ("linear", "srgb"):
        raise ValueError(f"Unsupported color space: {color_space}")
    if pixel_format == "r32f" and color_space != "linear":
        raise ValueError("R32F luminance must be linear")
    data = _read_raw(path)
    channels = _float32_channels(data, width, height, pixel_format)
    luminance = []
    nan_count = 0
    inf_count = 0
    negative_count = 0
    for channel in channels:
        if pixel_format == "r32f":
            value = channel[0]
            if math.isnan(value):
                nan_count += 1
                continue
            if math.isinf(value):
                inf_count += 1
                continue
            if value < 0.0:
                negative_count += 1
        else:
            if color_space == "srgb":
                channel = tuple(c / 12.92 if c <= 0.04045 else
                                ((c + 0.055) / 1.055) ** 2.4 for c in channel)
            value = 0.2126 * channel[0] + 0.7152 * channel[1] + 0.0722 * channel[2]
        luminance.append(value)

    finite = [v for v in luminance if math.isfinite(v)]
    if not finite:
        raise ValueError("Capture has no finite luminance samples")
    mean = sum(finite) / len(finite)
    variance = sum((v - mean) ** 2 for v in finite) / len(finite)
    return {
        "valid": True,
        "width": width,
        "height": height,
        "pixel_format": pixel_format,
        "color_space": color_space,
        "mean": mean,
        "stddev": math.sqrt(variance),
        "p05": _percentile(finite, 0.05),
        "p50": _percentile(finite, 0.50),
        "p95": _percentile(finite, 0.95),
        "underexposed_ratio": sum(v < 0.005 for v in finite) / max(1, width * height),
        "overexposed_ratio": sum(v > 0.995 for v in finite) / max(1, width * height),
        "nan_count": nan_count,
        "inf_count": inf_count,
        "negative_count": negative_count,
    }


def analyze_normals(path, width, height, pixel_format="rgba8"):
    data = _read_raw(path)
    channels = _float32_channels(data, width, height, pixel_format)
    lengths = []
    zero_count = 0
    high_error_count = 0
    positive_z = 0
    negative_z = 0
    if pixel_format == "r32f":
        raise ValueError("Normal analysis requires three color channels")
    for r, g, b in channels:
        nx = r * 2.0 - 1.0
        ny = g * 2.0 - 1.0
        nz = b * 2.0 - 1.0
        length = math.sqrt(nx * nx + ny * ny + nz * nz)
        lengths.append(length)
        if length < 0.1:
            zero_count += 1
        if abs(length - 1.0) > 0.05:
            high_error_count += 1
        if nz > 0.0:
            positive_z += 1
        else:
            negative_z += 1

    total = max(1, width * height)
    mean_length_error = sum(abs(length - 1.0) for length in lengths) / len(lengths) if lengths else 0.0
    return {
        "valid": True,
        "mean_length_error": mean_length_error,
        "zero_normal_ratio": zero_count / total,
        "length_error_ratio": high_error_count / total,
        "positive_z_ratio": positive_z / total,
        "negative_z_ratio": negative_z / total,
    }


def analyze_surface(path, width, height, pixel_format="rgba8"):
    data = _read_raw(path)
    channels = _float32_channels(data, width, height, pixel_format)
    if pixel_format == "r32f":
        raise ValueError("Surface analysis requires three color channels")
    roughness = [c[0] for c in channels]
    metallic = [c[1] for c in channels]
    ao = [c[2] for c in channels]

    def stats(values):
        mean = sum(values) / len(values) if values else 0.0
        return {
            "mean": mean,
            "p05": _percentile(values, 0.05),
            "p50": _percentile(values, 0.50),
            "p95": _percentile(values, 0.95),
        }

    return {
        "valid": True,
        "roughness": stats(roughness),
        "metallic": stats(metallic),
        "ao": stats(ao),
        "dead_roughness_ratio": sum(v < 0.001 or v > 0.999 for v in roughness) / max(1, width * height),
        "dead_metallic_ratio": sum(v < 0.001 or v > 0.999 for v in metallic) / max(1, width * height),
    }


def analyze_depth(path, width, height, reversed_z=False):
    data = _read_raw(path)
    channels = _float32_channels(data, width, height, "r32f")
    count = width * height
    values = [channel[0] for channel in channels]
    finite = [v for v in values if math.isfinite(v) and 0.0 <= v <= 1.0]
    histogram = [0] * 10
    for value in finite:
        bucket = min(9, max(0, int(value * 10.0)))
        histogram[bucket] += 1

    invalid = sum(not math.isfinite(v) or v < 0.0 or v > 1.0 for v in values)
    clear_depth = 0.0 if reversed_z else 1.0
    return {
        "valid": True,
        "reversed_z": reversed_z,
        "min": min(finite) if finite else None,
        "max": max(finite) if finite else None,
        "mean": sum(finite) / len(finite) if finite else None,
        "effective_depth_ratio": sum(v != clear_depth for v in finite) / count,
        "clear_depth": clear_depth,
        "valid_depth_count": len(finite),
        "invalid_depth_ratio": invalid / max(1, count),
        "histogram_0_1": histogram,
    }


def reconstruct_world_pos(depth_path, width, height, inverse_vp, reversed_z=False, sample_step=16):
    depth = _read_raw(depth_path)
    values = _float32_channels(depth, width, height, "r32f")

    if (len(inverse_vp) != 16 or
            not all(isinstance(value, (int, float)) and math.isfinite(value)
                    for value in inverse_vp)):
        raise ValueError("inverse_vp must contain 16 finite numbers")
    if not isinstance(sample_step, int) or isinstance(sample_step, bool) or sample_step <= 0:
        raise ValueError("sample_step must be a positive integer")

    def multiply(row, x, y, z, w):
        return (inverse_vp[row] * x + inverse_vp[row + 4] * y +
                inverse_vp[row + 8] * z + inverse_vp[row + 12] * w)

    points = []
    for y in range(0, height, sample_step):
        for x in range(0, width, sample_step):
            d = values[y * width + x][0]
            clear_depth = 0.0 if reversed_z else 1.0
            if not math.isfinite(d) or d < 0.0 or d > 1.0 or d == clear_depth:
                continue
            u = (x + 0.5) / width
            v = (y + 0.5) / height
            ndc_x = u * 2.0 - 1.0
            ndc_y = v * 2.0 - 1.0
            clip = [
                multiply(0, ndc_x, ndc_y, d, 1.0),
                multiply(1, ndc_x, ndc_y, d, 1.0),
                multiply(2, ndc_x, ndc_y, d, 1.0),
                multiply(3, ndc_x, ndc_y, d, 1.0),
            ]
            if abs(clip[3]) < 1e-8:
                continue
            points.append([
                clip[0] / clip[3],
                clip[1] / clip[3],
                clip[2] / clip[3],
            ])

    if not points:
        return {"valid": True, "point_count": 0, "bounds": None}

    mins = [min(p[i] for p in points) for i in range(3)]
    maxs = [max(p[i] for p in points) for i in range(3)]
    return {
        "valid": True,
        "reversed_z": reversed_z,
        "point_count": len(points),
        "bounds": {"min": mins, "max": maxs},
    }


def compare_frames(path_a, path_b, width, height, pixel_format="rgba8", threshold=0.01,
                   heatmap_dir=".ulre-analysis"):
    a = _read_raw(path_a)
    b = _read_raw(path_b)
    channels_a = _float32_channels(a, width, height, pixel_format)
    channels_b = _float32_channels(b, width, height, pixel_format)
    if len(channels_a) != len(channels_b):
        raise ValueError("Frame captures have different pixel counts")

    if not math.isfinite(threshold) or threshold < 0:
        raise ValueError("threshold must be finite and non-negative")
    mae_sum = 0.0
    mse_sum = 0.0
    changed = 0
    heatmap = bytearray(width * height)
    for index, (ca, cb) in enumerate(zip(channels_a, channels_b)):
        errors = [abs(x - y) for x, y in zip(ca, cb)]
        if not all(math.isfinite(error) for error in errors):
            raise ValueError("Cannot compare non-finite pixel values")
        error = max(errors)
        mae_sum += sum(errors)
        mse_sum += sum(value * value for value in errors)
        if error > threshold:
            changed += 1
        heatmap[index] = min(255, int(error * 255.0))

    count = max(1, len(channels_a))
    sample_count = count * len(channels_a[0])
    rmse = math.sqrt(mse_sum / sample_count)
    mae = mae_sum / sample_count
    psnr = 20.0 * math.log10(1.0 / rmse) if rmse > 0 else None

    Path(heatmap_dir).mkdir(parents=True, exist_ok=True)
    header = f"P5\n{width} {height}\n255\n".encode("ascii")
    with tempfile.NamedTemporaryFile(prefix="frame_diff_", suffix=".pgm",
                                     dir=heatmap_dir, delete=False) as output:
        output.write(header + heatmap)
        heatmap_path = output.name

    return {
        "valid": True,
        "mae": mae,
        "rmse": rmse,
        "psnr": psnr,
        "psnr_infinite": rmse == 0,
        "changed_ratio": changed / count,
        "threshold": threshold,
        "heatmap_path": heatmap_path,
    }
