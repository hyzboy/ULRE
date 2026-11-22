import math
import struct
import tempfile
from pathlib import Path

MAX_ANALYSIS_BYTES = 64 * 1024 * 1024
MAX_ANALYSIS_PIXELS = 2 * 1024 * 1024
MAX_COMPONENT_ANALYSIS_PIXELS = 262144
MAX_SIGNAL_PEAK = 3.4028235e38


def _read_raw(path):
    file_path = Path(path)
    if file_path.stat().st_size > MAX_ANALYSIS_BYTES:
        raise ValueError("Capture exceeds the analysis byte limit")
    with file_path.open("rb") as source:
        data = source.read(MAX_ANALYSIS_BYTES + 1)
    if len(data) > MAX_ANALYSIS_BYTES:
        raise ValueError("Capture exceeds the analysis byte limit")
    if not data:
        raise ValueError("Capture file is empty")
    return data


def _image_layout(width, height, pixel_format, row_pitch=None):
    if not isinstance(width, int) or isinstance(width, bool) or width <= 0:
        raise ValueError("width must be a positive integer")
    if not isinstance(height, int) or isinstance(height, bool) or height <= 0:
        raise ValueError("height must be a positive integer")
    count = width * height
    if count > MAX_ANALYSIS_PIXELS:
        raise ValueError("Capture exceeds the analysis pixel limit")
    sizes = {"rgba8": 4, "bgra8": 4, "rgb8": 3,
             "a2bgr10": 4, "rgb10a2": 4, "a2r10g10b10": 4,
             "r32f": 4, "rgba32f": 16}
    if pixel_format not in sizes:
        raise ValueError(f"Unsupported pixel format: {pixel_format}")
    pixel_size = sizes[pixel_format]
    packed_row = width * pixel_size
    if row_pitch is None:
        row_pitch = packed_row
    if not isinstance(row_pitch, int) or isinstance(row_pitch, bool) or row_pitch < packed_row:
        raise ValueError("row_pitch must be an integer at least as large as one pixel row")
    return count, pixel_size, row_pitch


def _iter_channels(data, width, height, pixel_format, row_pitch=None, mask_alpha=False):
    count, pixel_size, row_pitch = _image_layout(width, height, pixel_format, row_pitch)
    if len(data) != height * row_pitch:
        raise ValueError("Capture byte count does not match dimensions and format")
    if mask_alpha and pixel_format not in ("rgba8", "bgra8", "rgba32f"):
        raise ValueError("Alpha mask requires a supported four-channel image")
    for index in range(count):
        offset = (index // width) * row_pitch + (index % width) * pixel_size
        if mask_alpha and pixel_format == "rgba32f":
            alpha = struct.unpack_from("<f", data, offset + 12)[0]
            if alpha not in (0.0, 1.0):
                raise ValueError("Debug view mask contains non-binary alpha")
            if alpha == 0.0:
                continue
        elif mask_alpha:
            alpha = data[offset + 3]
            if alpha not in (0, 255):
                raise ValueError("Debug view mask contains non-binary alpha")
            if alpha == 0:
                continue
        if pixel_format == "r32f":
            yield (struct.unpack_from("<f", data, offset)[0],)
        else:
            yield _decode_rgb(data, offset, pixel_format)


def _float32_channels(data, width, height, pixel_format, row_pitch=None, mask_alpha=False):
    channels = list(_iter_channels(data, width, height, pixel_format, row_pitch, mask_alpha))
    if not channels:
        raise ValueError("Capture has no valid masked pixels")
    return channels


def _decode_rgb(data, offset, pixel_format):
    if pixel_format == "rgba32f":
        return struct.unpack_from("<3f", data, offset)
    if pixel_format in ("rgba8", "rgb8"):
        return tuple(data[offset + i] / 255.0 for i in range(3))
    if pixel_format == "bgra8":
        return tuple(data[offset + i] / 255.0 for i in (2, 1, 0))
    if pixel_format in ("a2bgr10", "rgb10a2", "a2r10g10b10"):
        packed = struct.unpack_from("<I", data, offset)[0]
        r = ((packed >> 0) & 0x3FF) / 1023.0
        g = ((packed >> 10) & 0x3FF) / 1023.0
        b = ((packed >> 20) & 0x3FF) / 1023.0
        if pixel_format == "a2r10g10b10":
            r, b = b, r
        return r, g, b
    raise ValueError(f"Unsupported color pixel format: {pixel_format}")


def _unproject_pixel(x, y, depth, inverse_vp, viewport):
    vx, vy = viewport["x"], viewport["y"]
    vw, vh = viewport["width"], viewport["height"]
    vmin, vmax = viewport["min_depth"], viewport["max_depth"]
    u = (x + 0.5 - vx) / vw
    v = (y + 0.5 - vy) / vh
    ndc_x = u * 2.0 - 1.0
    # Positive-height Vulkan viewports map framebuffer top-to-bottom to NDC -1..1.
    ndc_y = v * 2.0 - 1.0
    ndc_z = (depth - vmin) / (vmax - vmin)

    def multiply(row):
        return (inverse_vp[row] * ndc_x + inverse_vp[row + 4] * ndc_y +
                inverse_vp[row + 8] * ndc_z + inverse_vp[row + 12])

    clip = [multiply(row) for row in range(4)]
    if not all(math.isfinite(value) for value in clip) or abs(clip[3]) < 1e-8:
        return None
    point = tuple(clip[index] / clip[3] for index in range(3))
    return point if all(math.isfinite(value) for value in point) else None


def _position_pixels(data, width, height, row_pitch):
    if not isinstance(width, int) or isinstance(width, bool) or width <= 0 or \
            not isinstance(height, int) or isinstance(height, bool) or height <= 0:
        raise ValueError("Position RT dimensions must be positive integers")
    if width * height > MAX_ANALYSIS_PIXELS:
        raise ValueError("Capture exceeds the analysis pixel limit")
    if (not isinstance(row_pitch, int) or isinstance(row_pitch, bool) or
            row_pitch < width * 16 or len(data) != height * row_pitch):
        raise ValueError("Position RT byte count or row pitch is invalid")
    for y in range(height):
        for x in range(width):
            offset = y * row_pitch + x * 16
            position = struct.unpack_from("<3f", data, offset)
            alpha = struct.unpack_from("<f", data, offset + 12)[0]
            if alpha not in (0.0, 1.0):
                raise ValueError("Position RT mask contains non-binary alpha")
            yield x, y, position, alpha == 1.0


def _error_summary(values):
    if not values:
        return {"sample_count": 0, "mean": None, "p50": None, "p95": None, "max": None}
    ordered = sorted(values)
    return {
        "sample_count": len(ordered),
        "mean": sum(ordered) / len(ordered),
        "p50": _percentile(ordered, 0.5),
        "p95": _percentile(ordered, 0.95),
        "max": ordered[-1],
    }


def analyze_position_rt(path, width, height, row_pitch=None):
    data = _read_raw(path)
    if row_pitch is None:
        row_pitch = width * 16
    count = 0
    mins = [math.inf, math.inf, math.inf]
    maxs = [-math.inf, -math.inf, -math.inf]
    invalid_count = 0
    mask_sample_count = 0
    for _, _, position, valid in _position_pixels(data, width, height, row_pitch):
        if not valid:
            continue
        mask_sample_count += 1
        if not all(math.isfinite(value) for value in position):
            invalid_count += 1
            continue
        count += 1
        for axis in range(3):
            mins[axis] = min(mins[axis], position[axis])
            maxs[axis] = max(maxs[axis], position[axis])
    if not count:
        return {"valid": False, "error": "no_valid_position_samples",
                "point_count": 0, "mask_sample_count": mask_sample_count,
                "invalid_position_count": invalid_count, "bounds": None}
    return {
        "valid": True,
        "point_count": count,
        "mask_sample_count": mask_sample_count,
        "invalid_position_count": invalid_count,
        "bounds": {"min": mins, "max": maxs},
        "position_space": "world_absolute",
        "representation": "ieee754_binary32",
        "precision_note": "measured values only; no fixed physical error guarantee",
    }


def cross_validate_position_rt(position_path, depth_path, width, height, inverse_vp,
                               reversed_z, sample_step, clear_depth, viewport, row_pitch,
                               position_row_pitch, camera_world_pos):
    if not isinstance(sample_step, int) or isinstance(sample_step, bool) or sample_step <= 0:
        raise ValueError("sample_step must be a positive integer")
    sampled_width = (width + sample_step - 1) // sample_step
    sampled_height = (height + sample_step - 1) // sample_step
    if sampled_width * sampled_height > 262144:
        raise ValueError("Position cross-check exceeds the sample limit; increase sample_step")
    if len(inverse_vp) != 16 or not all(
            isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)
            for value in inverse_vp):
        raise ValueError("inverse_vp must contain 16 finite numbers")
    if len(camera_world_pos) != 3 or not all(
            isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)
            for value in camera_world_pos):
        raise ValueError("camera_world_pos must contain three finite numbers")
    depth_data = _read_raw(depth_path)
    position_data = _read_raw(position_path)
    if not isinstance(width, int) or isinstance(width, bool) or width <= 0 or \
            not isinstance(height, int) or isinstance(height, bool) or height <= 0 or \
            width * height > MAX_ANALYSIS_PIXELS:
        raise ValueError("Capture dimensions are invalid or exceed the analysis pixel limit")
    if not isinstance(row_pitch, int) or isinstance(row_pitch, bool) or row_pitch < width * 4 or \
            len(depth_data) != height * row_pitch:
        raise ValueError("Depth byte count or row pitch is invalid")
    if not isinstance(position_row_pitch, int) or isinstance(position_row_pitch, bool) or \
            position_row_pitch < width * 16 or len(position_data) != height * position_row_pitch:
        raise ValueError("Position RT byte count or row pitch is invalid")
    if not isinstance(reversed_z, bool) or isinstance(clear_depth, bool) or \
            not isinstance(clear_depth, (int, float)) or not math.isfinite(clear_depth) or \
            not 0 <= clear_depth <= 1:
        raise ValueError("Depth convention or clear value is invalid")
    if not isinstance(viewport, dict) or any(
            isinstance(viewport.get(key), bool) or
            not isinstance(viewport.get(key), (int, float)) or
            not math.isfinite(viewport[key])
            for key in ("x", "y", "width", "height", "min_depth", "max_depth")):
        raise ValueError("Capture viewport is missing or invalid")
    vx, vy = viewport["x"], viewport["y"]
    vw, vh = viewport["width"], viewport["height"]
    if vx < 0 or vy < 0 or vw <= 0 or vh <= 0 or vx + vw > width or vy + vh > height or \
            not 0 <= viewport["min_depth"] < viewport["max_depth"] <= 1:
        raise ValueError("Capture viewport is outside the image or has an invalid depth range")
    errors = []
    comparisons = []
    invalid_position_count = 0
    for y in range(0, height, sample_step):
        for x in range(0, width, sample_step):
            if not (vx <= x + 0.5 < vx + vw and vy <= y + 0.5 < vy + vh):
                continue
            offset = y * position_row_pitch + x * 16
            position = struct.unpack_from("<3f", position_data, offset)
            alpha = struct.unpack_from("<f", position_data, offset + 12)[0]
            if alpha not in (0.0, 1.0):
                raise ValueError("Position RT mask contains non-binary alpha")
            if alpha == 0.0:
                continue
            depth = struct.unpack_from("<f", depth_data, y * row_pitch + x * 4)[0]
            if not math.isfinite(depth) or depth < viewport["min_depth"] or depth > viewport["max_depth"]:
                continue
            reconstructed = _unproject_pixel(x, y, depth, inverse_vp, viewport)
            if reconstructed is None:
                continue
            if not all(math.isfinite(value) for value in position):
                invalid_position_count += 1
                continue
            delta = [position[i] - reconstructed[i] for i in range(3)]
            error = math.sqrt(sum(value * value for value in delta))
            camera_distance = math.sqrt(sum((position[i] - camera_world_pos[i]) ** 2
                                            for i in range(3)))
            if math.isfinite(error) and math.isfinite(camera_distance):
                errors.append(error)
                comparisons.append((camera_distance, error))
    if not comparisons:
        return {"valid": False, "error": "no_comparable_position_samples",
                "point_count": 0, "invalid_position_count": invalid_position_count,
                "distance_bands": []}
    ordered = sorted(comparisons, key=lambda value: value[0])
    bands = []
    count = len(ordered)
    for index, name in enumerate(("near", "middle", "far")):
        start = index * count // 3
        end = (index + 1) * count // 3
        if start == end:
            continue
        values = ordered[start:end]
        summary = _error_summary([item[1] for item in values])
        summary["band"] = name
        summary["camera_distance"] = {
            "min": values[0][0],
            "max": values[-1][0],
        }
        bands.append(summary)
    return {
        "valid": True,
        "point_count": count,
        "invalid_position_count": invalid_position_count,
        "absolute_position_error": _error_summary(errors),
        "distance_bands": bands,
        "position_space": "world_absolute",
        "error_unit": "world_units",
        "precision_note": "measured same-frame difference; no fixed physical error guarantee",
    }


def _percentile(values, percentile):
    if not values:
        return 0.0
    ordered = sorted(values)
    return _percentile_sorted(ordered, percentile)


def _percentile_sorted(ordered, percentile):
    if not ordered:
        return 0.0
    position = (len(ordered) - 1) * percentile
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def analyze_luminance(path, width, height, pixel_format="rgba8", color_space="linear",
                      row_pitch=None, mask_alpha=False):
    if color_space not in ("linear", "srgb"):
        raise ValueError(f"Unsupported color space: {color_space}")
    if pixel_format == "r32f" and color_space != "linear":
        raise ValueError("R32F luminance must be linear")
    data = _read_raw(path)
    finite = []
    nan_count = 0
    inf_count = 0
    negative_count = 0
    input_sample_count = 0
    for channel in _iter_channels(data, width, height, pixel_format, row_pitch, mask_alpha):
        input_sample_count += 1
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
            if not all(math.isfinite(value) for value in channel):
                nan_count += any(math.isnan(value) for value in channel)
                inf_count += any(math.isinf(value) for value in channel)
                continue
            if color_space == "srgb":
                channel = tuple(c / 12.92 if c <= 0.04045 else
                                ((c + 0.055) / 1.055) ** 2.4 for c in channel)
            value = 0.2126 * channel[0] + 0.7152 * channel[1] + 0.0722 * channel[2]
        finite.append(value)

    if not input_sample_count:
        raise ValueError("Capture has no valid masked pixels")
    if not finite:
        raise ValueError("Capture has no finite luminance samples")
    mean = sum(finite) / len(finite)
    variance = sum((v - mean) ** 2 for v in finite) / len(finite)
    underexposed_count = sum(v < 0.005 for v in finite)
    overexposed_count = sum(v > 0.995 for v in finite)
    sample_count = len(finite)
    finite.sort()
    return {
        "valid": True,
        "width": width,
        "height": height,
        "pixel_format": pixel_format,
        "color_space": color_space,
        "mean": mean,
        "stddev": math.sqrt(variance),
        "p05": _percentile_sorted(finite, 0.05),
        "p50": _percentile_sorted(finite, 0.50),
        "p95": _percentile_sorted(finite, 0.95),
        "underexposed_ratio": underexposed_count / sample_count,
        "overexposed_ratio": overexposed_count / sample_count,
        "sample_count": sample_count,
        "input_sample_count": input_sample_count,
        "masked_out_count": width * height - input_sample_count,
        "nan_count": nan_count,
        "inf_count": inf_count,
        "negative_count": negative_count,
    }


def analyze_normals(path, width, height, pixel_format="rgba8", row_pitch=None, mask_alpha=False):
    data = _read_raw(path)
    input_sample_count = 0
    total = 0
    length_error_sum = 0.0
    nan_count = 0
    inf_count = 0
    zero_count = 0
    high_error_count = 0
    positive_z = 0
    negative_z = 0
    if pixel_format == "r32f":
        raise ValueError("Normal analysis requires three color channels")
    for channel in _iter_channels(data, width, height, pixel_format, row_pitch, mask_alpha):
        input_sample_count += 1
        if not all(math.isfinite(value) for value in channel):
            nan_count += any(math.isnan(value) for value in channel)
            inf_count += any(math.isinf(value) for value in channel)
            continue
        r, g, b = channel
        if pixel_format == "rgba32f":
            nx, ny, nz = r, g, b
        else:
            nx = r * 2.0 - 1.0
            ny = g * 2.0 - 1.0
            nz = b * 2.0 - 1.0
        length = math.sqrt(nx * nx + ny * ny + nz * nz)
        total += 1
        length_error_sum += abs(length - 1.0)
        if length < 0.1:
            zero_count += 1
        if abs(length - 1.0) > 0.05:
            high_error_count += 1
        if nz > 0.0:
            positive_z += 1
        else:
            negative_z += 1

    if not input_sample_count:
        raise ValueError("Capture has no valid masked pixels")
    if not total:
        raise ValueError("Capture has no finite normal samples")
    mean_length_error = length_error_sum / total
    return {
        "valid": True,
        "sample_count": total,
        "input_sample_count": input_sample_count,
        "masked_out_count": width * height - input_sample_count,
        "nan_count": nan_count,
        "inf_count": inf_count,
        "normal_encoding": "signed_xyz" if pixel_format == "rgba32f" else "unorm01_to_signed_xyz",
        "mean_length_error": mean_length_error,
        "zero_normal_ratio": zero_count / total,
        "length_error_ratio": high_error_count / total,
        "positive_z_ratio": positive_z / total,
        "negative_z_ratio": negative_z / total,
    }


def analyze_surface(path, width, height, pixel_format="rgba8", row_pitch=None, mask_alpha=False):
    data = _read_raw(path)
    if pixel_format == "r32f":
        raise ValueError("Surface analysis requires three color channels")
    roughness = []
    metallic = []
    ao = []
    input_sample_count = 0
    nan_count = 0
    inf_count = 0
    for channel in _iter_channels(data, width, height, pixel_format, row_pitch, mask_alpha):
        input_sample_count += 1
        if not all(math.isfinite(value) for value in channel):
            nan_count += any(math.isnan(value) for value in channel)
            inf_count += any(math.isinf(value) for value in channel)
            continue
        roughness.append(channel[0])
        metallic.append(channel[1])
        ao.append(channel[2])
    if not input_sample_count:
        raise ValueError("Capture has no valid masked pixels")
    if not roughness:
        raise ValueError("Capture has no finite surface samples")

    def stats(values):
        mean = sum(values) / len(values) if values else 0.0
        values.sort()
        return {
            "mean": mean,
            "p05": _percentile_sorted(values, 0.05),
            "p50": _percentile_sorted(values, 0.50),
            "p95": _percentile_sorted(values, 0.95),
        }

    return {
        "valid": True,
        "sample_count": len(roughness),
        "input_sample_count": input_sample_count,
        "masked_out_count": width * height - input_sample_count,
        "nan_count": nan_count,
        "inf_count": inf_count,
        "roughness": stats(roughness),
        "metallic": stats(metallic),
        "ao": stats(ao),
        "dead_roughness_ratio": sum(v < 0.001 or v > 0.999 for v in roughness) / len(roughness),
        "dead_metallic_ratio": sum(v < 0.001 or v > 0.999 for v in metallic) / len(metallic),
        "endpoint_ratios_are_descriptive_not_failures": True,
    }


def analyze_depth(path, width, height, reversed_z=False, clear_depth=None, row_pitch=None,
                  valid_mask=None):
    if not isinstance(reversed_z, bool):
        raise ValueError("reversed_z must be boolean")
    data = _read_raw(path)
    count, _, row_pitch = _image_layout(width, height, "r32f", row_pitch)
    if len(data) != height * row_pitch:
        raise ValueError("Capture byte count does not match dimensions and format")
    if valid_mask is not None and (len(valid_mask) != count or not any(valid_mask)):
        raise ValueError("Depth mask is empty or has the wrong size")
    mask_sample_count = sum(bool(value) for value in valid_mask) if valid_mask is not None else count
    if clear_depth is None:
        clear_depth = 0.0 if reversed_z else 1.0
    if not isinstance(clear_depth, (int, float)) or isinstance(clear_depth, bool) or \
            not math.isfinite(clear_depth) or not 0.0 <= clear_depth <= 1.0:
        raise ValueError("clear_depth must be a finite number in [0, 1]")
    sample_count = 0
    finite_count = 0
    finite_sum = 0.0
    minimum = math.inf
    maximum = -math.inf
    histogram = [0] * 10
    nan_count = 0
    inf_count = 0
    out_of_range_count = 0
    clear_depth_count = 0
    for index in range(count):
        if valid_mask is not None and not valid_mask[index]:
            continue
        sample_count += 1
        value = struct.unpack_from("<f", data, (index // width) * row_pitch +
                                   (index % width) * 4)[0]
        if math.isnan(value):
            nan_count += 1
            continue
        if math.isinf(value):
            inf_count += 1
            continue
        if not 0.0 <= value <= 1.0:
            out_of_range_count += 1
            continue
        finite_count += 1
        finite_sum += value
        minimum = min(minimum, value)
        maximum = max(maximum, value)
        clear_depth_count += value == clear_depth
        bucket = min(9, max(0, int(value * 10.0)))
        histogram[bucket] += 1
    invalid = nan_count + inf_count + out_of_range_count
    non_clear_depth_count = finite_count - clear_depth_count
    result = {
        "valid": finite_count > 0,
        "reversed_z": reversed_z,
        "min": minimum if finite_count else None,
        "max": maximum if finite_count else None,
        "mean": finite_sum / finite_count if finite_count else None,
        "effective_depth_ratio": ((finite_count if valid_mask is not None
                                   else non_clear_depth_count) / count) if finite_count else None,
        "clear_depth": clear_depth,
        "valid_depth_count": finite_count,
        "invalid_depth_ratio": invalid / sample_count,
        "sample_count": sample_count,
        "nan_count": nan_count,
        "inf_count": inf_count,
        "out_of_range_count": out_of_range_count,
        "clear_depth_count": clear_depth_count,
        "non_clear_depth_count": non_clear_depth_count,
        "finite_depth_ratio": finite_count / mask_sample_count,
        "clear_depth_ratio": clear_depth_count / mask_sample_count,
        "non_clear_depth_ratio": non_clear_depth_count / mask_sample_count,
        "mask_sample_count": mask_sample_count,
        "ratio_denominator": "selected mask pixels" if valid_mask is not None else "full image",
        "effective_depth_ratio_denominator": "full image",
        "histogram_0_1": histogram,
    }
    if not finite_count:
        result["error"] = "no_valid_depth_samples"
    return result


def reconstruct_world_pos(depth_path, width, height, inverse_vp, reversed_z=False, sample_step=16,
                          clear_depth=None, viewport=None, row_pitch=None, valid_mask=None):
    depth = _read_raw(depth_path)
    _, _, row_pitch = _image_layout(width, height, "r32f", row_pitch)
    if len(depth) != height * row_pitch:
        raise ValueError("Capture byte count does not match dimensions and format")
    if valid_mask is not None and (len(valid_mask) != width * height or not any(valid_mask)):
        raise ValueError("Depth mask is empty or has the wrong size")

    if (len(inverse_vp) != 16 or
            not all(isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)
                    for value in inverse_vp)):
        raise ValueError("inverse_vp must contain 16 finite numbers")
    if not isinstance(sample_step, int) or isinstance(sample_step, bool) or sample_step <= 0:
        raise ValueError("sample_step must be a positive integer")
    if clear_depth is None:
        clear_depth = 0.0 if reversed_z else 1.0
    if isinstance(clear_depth, bool) or not isinstance(clear_depth, (int, float)) or \
            not math.isfinite(clear_depth) or not 0 <= clear_depth <= 1:
        raise ValueError("clear_depth must be a finite number in [0, 1]")
    if viewport is None:
        viewport = {"x": 0, "y": 0, "width": width, "height": height,
                    "min_depth": 0.0, "max_depth": 1.0}
    if not isinstance(viewport, dict):
        raise ValueError("viewport must be an object")
    names = ("x", "y", "width", "height", "min_depth", "max_depth")
    if any(isinstance(viewport.get(key), bool) or
           not isinstance(viewport.get(key), (int, float)) or
           not math.isfinite(viewport[key]) for key in names):
        raise ValueError("viewport fields must be finite numbers")
    vx, vy = viewport["x"], viewport["y"]
    vw, vh = viewport["width"], viewport["height"]
    vmin, vmax = viewport["min_depth"], viewport["max_depth"]
    if vx < 0 or vy < 0 or vw <= 0 or vh <= 0 or vx + vw > width or vy + vh > height \
            or not 0 <= vmin < vmax <= 1:
        raise ValueError("viewport is outside the capture or has an invalid depth range")

    def multiply(row, x, y, z, w):
        return (inverse_vp[row] * x + inverse_vp[row + 4] * y +
                inverse_vp[row + 8] * z + inverse_vp[row + 12] * w)

    point_count = 0
    mins = [math.inf, math.inf, math.inf]
    maxs = [-math.inf, -math.inf, -math.inf]
    for y in range(0, height, sample_step):
        for x in range(0, width, sample_step):
            d = struct.unpack_from("<f", depth, y * row_pitch + x * 4)[0]
            if valid_mask is not None and not valid_mask[y * width + x]:
                continue
            if not (vx <= x + 0.5 < vx + vw and vy <= y + 0.5 < vy + vh):
                continue
            if not math.isfinite(d) or d < vmin or d > vmax \
                    or (valid_mask is None and d == clear_depth):
                continue
            u = (x + 0.5 - vx) / vw
            v = (y + 0.5 - vy) / vh
            ndc_x = u * 2.0 - 1.0
            ndc_y = v * 2.0 - 1.0
            ndc_z = (d - vmin) / (vmax - vmin)
            clip = [
                multiply(0, ndc_x, ndc_y, ndc_z, 1.0),
                multiply(1, ndc_x, ndc_y, ndc_z, 1.0),
                multiply(2, ndc_x, ndc_y, ndc_z, 1.0),
                multiply(3, ndc_x, ndc_y, ndc_z, 1.0),
            ]
            if not all(math.isfinite(value) for value in clip) or abs(clip[3]) < 1e-8:
                continue
            point = [
                clip[0] / clip[3],
                clip[1] / clip[3],
                clip[2] / clip[3],
            ]
            if all(math.isfinite(value) for value in point):
                point_count += 1
                for axis in range(3):
                    mins[axis] = min(mins[axis], point[axis])
                    maxs[axis] = max(maxs[axis], point[axis])

    if not point_count:
        return {"valid": False, "error": "no_valid_non_clear_samples",
                "point_count": 0, "bounds": None}

    return {
        "valid": True,
        "reversed_z": reversed_z,
        "point_count": point_count,
        "bounds": {"min": mins, "max": maxs},
    }


def _validate_color_capture(data, width, height, pixel_format, row_pitch):
    if not isinstance(width, int) or isinstance(width, bool) or width <= 0 or \
            not isinstance(height, int) or isinstance(height, bool) or height <= 0:
        raise ValueError("width and height must be positive integers")
    if width * height > MAX_ANALYSIS_PIXELS:
        raise ValueError("Capture exceeds the analysis pixel limit")
    pixel_sizes = {"rgba8": 4, "bgra8": 4, "rgb8": 3,
                   "a2bgr10": 4, "rgb10a2": 4, "a2r10g10b10": 4,
                   "rgba32f": 16}
    if pixel_format not in pixel_sizes:
        raise ValueError(f"Unsupported color pixel format: {pixel_format}")
    pixel_size = pixel_sizes[pixel_format]
    packed_row = width * pixel_size
    if row_pitch is None:
        row_pitch = packed_row
    if not isinstance(row_pitch, int) or isinstance(row_pitch, bool) or row_pitch < packed_row:
        raise ValueError("row pitch must be an integer at least as large as one pixel row")
    if len(data) != height * row_pitch:
        raise ValueError("Capture byte count does not match dimensions and format")
    return pixel_size, row_pitch


def _capture_pixel_valid(data, offset, pixel_format, mask_alpha):
    if not mask_alpha:
        return True
    if pixel_format == "rgba32f":
        alpha = struct.unpack_from("<f", data, offset + 12)[0]
        if alpha not in (0.0, 1.0):
            raise ValueError("Debug view mask contains non-binary alpha")
        return alpha == 1.0
    alpha = data[offset + 3]
    if alpha not in (0, 255):
        raise ValueError("Debug view mask contains non-binary alpha")
    return alpha == 255


def _ssim_score(data_a, data_b, width, height, pixel_format, pixel_size,
                row_pitch_a, row_pitch_b, mask_alpha, sample_count, peak_value=1.0,
                window_size=8):
    c1 = (0.01 * peak_value) ** 2
    c2 = (0.03 * peak_value) ** 2
    weighted_score = 0.0
    window_count = 0
    for top in range(0, height, window_size):
        for left in range(0, width, window_size):
            sums_a = [0.0, 0.0, 0.0]
            sums_b = [0.0, 0.0, 0.0]
            squares_a = [0.0, 0.0, 0.0]
            squares_b = [0.0, 0.0, 0.0]
            products = [0.0, 0.0, 0.0]
            count = 0
            for y in range(top, min(height, top + window_size)):
                for x in range(left, min(width, left + window_size)):
                    offset_a = y * row_pitch_a + x * pixel_size
                    offset_b = y * row_pitch_b + x * pixel_size
                    if mask_alpha and not (
                            _capture_pixel_valid(data_a, offset_a, pixel_format, True) and
                            _capture_pixel_valid(data_b, offset_b, pixel_format, True)):
                        continue
                    ca = _decode_rgb(data_a, offset_a, pixel_format)
                    cb = _decode_rgb(data_b, offset_b, pixel_format)
                    if not all(math.isfinite(value) for value in ca + cb):
                        raise ValueError("Cannot compare non-finite pixel values")
                    for channel in range(3):
                        sums_a[channel] += ca[channel]
                        sums_b[channel] += cb[channel]
                        squares_a[channel] += ca[channel] * ca[channel]
                        squares_b[channel] += cb[channel] * cb[channel]
                        products[channel] += ca[channel] * cb[channel]
                    count += 1
            if not count:
                continue
            score = 0.0
            for channel in range(3):
                mean_a = sums_a[channel] / count
                mean_b = sums_b[channel] / count
                variance_a = max(0.0, squares_a[channel] / count - mean_a * mean_a)
                variance_b = max(0.0, squares_b[channel] / count - mean_b * mean_b)
                covariance = products[channel] / count - mean_a * mean_b
                numerator = (2.0 * mean_a * mean_b + c1) * (2.0 * covariance + c2)
                denominator = (mean_a * mean_a + mean_b * mean_b + c1) * \
                    (variance_a + variance_b + c2)
                score += numerator / denominator
            weighted_score += (score / 3.0) * count
            window_count += 1
    if not sample_count or not window_count:
        raise ValueError("SSIM comparison has no valid intersection-mask samples")
    return weighted_score / sample_count, window_count


def _connected_regions(changed_pixels, width, height, limit=64):
    parent = {}
    regions = {}
    previous_runs = []
    next_label = 0
    component_count = 0
    largest_area = 0
    largest_regions = []

    def find(label):
        root = label
        while parent[root] != root:
            root = parent[root]
        while parent[label] != label:
            next_parent = parent[label]
            parent[label] = root
            label = next_parent
        return root

    def merge(first, second):
        first = find(first)
        second = find(second)
        if first == second:
            return first
        parent[second] = first
        a = regions[first]
        b = regions.pop(second)
        a["area"] += b["area"]
        a["x_min"] = min(a["x_min"], b["x_min"])
        a["y_min"] = min(a["y_min"], b["y_min"])
        a["x_max"] = max(a["x_max"], b["x_max"])
        a["y_max"] = max(a["y_max"], b["y_max"])
        return first

    def record(label):
        nonlocal component_count, largest_area, largest_regions
        region = regions.pop(label)
        component_count += 1
        largest_area = max(largest_area, region["area"])
        item = {
            "area_pixels": region["area"],
            "bounds": {
                "x": region["x_min"], "y": region["y_min"],
                "width": region["x_max"] - region["x_min"] + 1,
                "height": region["y_max"] - region["y_min"] + 1,
            },
        }
        largest_regions.append(item)
        largest_regions.sort(key=lambda value: (-value["area_pixels"],
                                                 value["bounds"]["y"],
                                                 value["bounds"]["x"]))
        del largest_regions[limit:]

    for y in range(height):
        runs = []
        x = 0
        previous_index = 0
        while x < width:
            if not changed_pixels[y * width + x]:
                x += 1
                continue
            start = x
            while x + 1 < width and changed_pixels[y * width + x + 1]:
                x += 1
            end = x
            while previous_index < len(previous_runs) and \
                    previous_runs[previous_index][1] < start:
                previous_index += 1
            overlapping = []
            old_index = previous_index
            while old_index < len(previous_runs) and previous_runs[old_index][0] <= end:
                old_start, old_end, old_label = previous_runs[old_index]
                if old_end >= start:
                    overlapping.append(find(old_label))
                old_index += 1
            if overlapping:
                label = overlapping[0]
                for other in overlapping[1:]:
                    label = merge(label, other)
            else:
                label = next_label
                next_label += 1
                parent[label] = label
                regions[label] = {"area": 0, "x_min": start, "y_min": y,
                                  "x_max": end, "y_max": y}
            label = find(label)
            region = regions[label]
            region["area"] += end - start + 1
            region["x_min"] = min(region["x_min"], start)
            region["x_max"] = max(region["x_max"], end)
            region["y_max"] = y
            runs.append((start, end, label))
            x += 1

        current_labels = {find(label) for _, _, label in runs}
        previous_labels = {find(label) for _, _, label in previous_runs}
        for label in previous_labels - current_labels:
            record(label)
        canonical_runs = [(start, end, find(label)) for start, end, label in runs]
        regions = {label: regions[label] for label in current_labels}
        parent = {label: label for label in current_labels}
        previous_runs = canonical_runs

    for label in {find(label) for _, _, label in previous_runs}:
        record(label)
    return {
        "connectivity": 4,
        "component_count": component_count,
        "largest_area_pixels": largest_area,
        "regions": largest_regions,
        "regions_returned": len(largest_regions),
        "regions_truncated": component_count > limit,
        "region_limit": limit,
    }


def compare_frames(path_a, path_b, width, height, pixel_format="rgba8", threshold=0.01,
                   heatmap_dir=".ulre-analysis", row_pitch_a=None, row_pitch_b=None,
                   mask_alpha=False, peak_value=1.0):
    a = _read_raw(path_a)
    b = _read_raw(path_b)
    pixel_size, row_pitch_a = _validate_color_capture(
        a, width, height, pixel_format, row_pitch_a)
    other_size, row_pitch_b = _validate_color_capture(
        b, width, height, pixel_format, row_pitch_b)
    if pixel_size != other_size:
        raise ValueError("Frame captures have different pixel formats")
    if mask_alpha and pixel_format not in ("rgba8", "bgra8", "rgba32f"):
        raise ValueError("Alpha mask requires a supported four-channel image")
    if isinstance(threshold, bool) or not isinstance(threshold, (int, float)) or \
            not math.isfinite(threshold) or threshold < 0:
        raise ValueError("threshold must be finite and non-negative")
    if isinstance(peak_value, bool) or not isinstance(peak_value, (int, float)) or \
            not math.isfinite(peak_value) or peak_value <= 0 or peak_value > MAX_SIGNAL_PEAK:
        raise ValueError("peak_value must be finite, positive, and within the supported float range")

    channel_abs = [0.0, 0.0, 0.0]
    channel_sq = [0.0, 0.0, 0.0]
    changed = 0
    mask_a_count = 0
    mask_b_count = 0
    mask_intersection_count = 0
    mask_union_count = 0
    mask_changed = 0
    heatmap = bytearray(width * height)
    changed_pixels = bytearray(width * height)
    for y in range(height):
        for x in range(width):
            index = y * width + x
            offset_a = y * row_pitch_a + x * pixel_size
            offset_b = y * row_pitch_b + x * pixel_size
            valid_a = _capture_pixel_valid(a, offset_a, pixel_format, mask_alpha)
            valid_b = _capture_pixel_valid(b, offset_b, pixel_format, mask_alpha)
            mask_a_count += valid_a
            mask_b_count += valid_b
            mask_intersection_count += valid_a and valid_b
            mask_union_count += valid_a or valid_b
            if valid_a != valid_b:
                mask_changed += 1
                heatmap[index] = 255
            if not (valid_a and valid_b):
                continue
            ca = _decode_rgb(a, offset_a, pixel_format)
            cb = _decode_rgb(b, offset_b, pixel_format)
            if not all(math.isfinite(value) for value in ca + cb):
                raise ValueError("Cannot compare non-finite pixel values")
            errors = [abs(ca[channel] - cb[channel]) for channel in range(3)]
            for channel, error in enumerate(errors):
                channel_abs[channel] += error
                channel_sq[channel] += error * error
            max_error = max(errors)
            if max_error > threshold:
                changed += 1
                changed_pixels[index] = 1
                heatmap[index] = min(254, max(1, int(max_error / peak_value * 254.0)))
            elif max_error > 0:
                heatmap[index] = min(254, max(1, int(max_error / peak_value * 254.0)))

    if not mask_intersection_count:
        raise ValueError("Capture comparison has no valid intersection-mask pixels")
    channel_metrics = []
    for name, absolute_sum, square_sum in zip(("r", "g", "b"), channel_abs, channel_sq):
        mae = absolute_sum / mask_intersection_count
        rmse = math.sqrt(square_sum / mask_intersection_count)
        channel_metrics.append({
            "channel": name,
            "mae": mae,
            "rmse": rmse,
            "psnr": (20.0 * (math.log10(peak_value) - math.log10(rmse))
                     if rmse > 0 else None),
            "psnr_infinite": rmse == 0,
        })
    channel_sample_count = mask_intersection_count * 3
    mae = sum(channel_abs) / channel_sample_count
    rmse = math.sqrt(sum(channel_sq) / channel_sample_count)
    psnr = 20.0 * (math.log10(peak_value) - math.log10(rmse)) if rmse > 0 else None
    ssim, ssim_window_count = _ssim_score(
        a, b, width, height, pixel_format, pixel_size, row_pitch_a, row_pitch_b,
        mask_alpha, mask_intersection_count, peak_value)

    Path(heatmap_dir).mkdir(parents=True, exist_ok=True)
    header = f"P5\n{width} {height}\n255\n".encode("ascii")
    with tempfile.NamedTemporaryFile(prefix="frame_diff_", suffix=".pgm",
                                     dir=heatmap_dir, delete=False) as output:
        output.write(header + heatmap)
        heatmap_path = output.name

    if width * height <= MAX_COMPONENT_ANALYSIS_PIXELS:
        regions = _connected_regions(changed_pixels, width, height)
        regions["status"] = "available"
    else:
        regions = {
            "status": "unavailable",
            "reason": "component_analysis_pixel_limit",
            "pixel_count": width * height,
            "max_pixel_count": MAX_COMPONENT_ANALYSIS_PIXELS,
        }
    comparison_space = "float_channel_values" if pixel_format == "rgba32f" else "encoded_channels"
    result = {
        "valid": True,
        "mae": mae,
        "rmse": rmse,
        "psnr": psnr,
        "psnr_infinite": rmse == 0,
        "channel_metrics": channel_metrics,
        "changed_pixels": changed,
        "changed_ratio": changed / mask_intersection_count,
        "threshold": threshold,
        "threshold_comparison": "strictly_greater_than_max_absolute_channel_error",
        "threshold_units": comparison_space,
        "comparison_space": comparison_space,
        "signal_peak": peak_value,
        "channel_sample_count": channel_sample_count,
        "sample_count": mask_intersection_count,
        "mask": {
            "policy": "intersection",
            "sample_count_a": mask_a_count,
            "sample_count_b": mask_b_count,
            "intersection_sample_count": mask_intersection_count,
            "union_sample_count": mask_union_count,
            "coverage_changed_pixels": mask_changed,
            "coverage_changed_ratio": mask_changed / (width * height),
            "intersection_over_union": (mask_intersection_count / mask_union_count
                                        if mask_union_count else None),
        },
        "mask_changed_pixels": mask_changed,
        "mask_changed_ratio": mask_changed / (width * height),
        "ssim": {
            "value": ssim,
            "algorithm": "uniform non-overlapping local windows; per-channel SSIM averaged",
            "comparison_space": comparison_space,
            "window_size": 8,
            "window_count": ssim_window_count,
            "c1": (0.01 * peak_value) ** 2,
            "c2": (0.03 * peak_value) ** 2,
            "variance": "population",
        },
        "changed_regions": regions,
        "heatmap_path": heatmap_path,
        "heatmap": {
            "path": heatmap_path,
            "format": "PGM P5",
            "coverage_change_value": 255,
            "difference_values": "1..254 proportional to max channel error",
        },
        "limits": {
            "max_analysis_bytes_per_capture": MAX_ANALYSIS_BYTES,
            "max_analysis_pixels": MAX_ANALYSIS_PIXELS,
            "max_component_analysis_pixels": MAX_COMPONENT_ANALYSIS_PIXELS,
            "max_returned_regions": 64,
        },
    }
    return result
