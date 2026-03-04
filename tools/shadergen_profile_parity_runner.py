#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
import sys
from datetime import datetime

DEFAULT_SMOKE_TESTS = [
    "test_ShaderGenPhysicalDeviceProfileJsonPath.exe",
    "test_ShaderGenContractValidatorProfile.exe",
]

EXTENDED_TESTS = [
    "test_RendererShaderGenAdapterProfileCategory.exe",
    "test_MaterialPresetExhaustiveCompile.exe",
]


def run_test(exe_path: str, timeout_sec: int) -> dict:
    start = datetime.utcnow()
    result = {
        "name": os.path.basename(exe_path),
        "path": exe_path,
        "started_utc": start.isoformat() + "Z",
        "exit_code": None,
        "passed": False,
        "stdout": "",
        "stderr": "",
        "duration_ms": 0,
    }

    try:
        proc = subprocess.run(
            [exe_path],
            capture_output=True,
            text=True,
            timeout=timeout_sec,
            check=False,
        )
        end = datetime.utcnow()
        result["duration_ms"] = int((end - start).total_seconds() * 1000)
        result["exit_code"] = proc.returncode
        result["stdout"] = proc.stdout.strip()
        result["stderr"] = proc.stderr.strip()
        result["passed"] = proc.returncode == 0
    except subprocess.TimeoutExpired as ex:
        end = datetime.utcnow()
        result["duration_ms"] = int((end - start).total_seconds() * 1000)
        result["exit_code"] = -1
        result["stderr"] = f"TIMEOUT after {timeout_sec}s: {ex}"
    except Exception as ex:
        end = datetime.utcnow()
        result["duration_ms"] = int((end - start).total_seconds() * 1000)
        result["exit_code"] = -2
        result["stderr"] = f"RUN_ERROR: {ex}"

    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Run ShaderGen profile parity baseline tests")
    parser.add_argument(
        "--bin-dir",
        default=os.path.join("build", "windows-msvc-debug", "out", "Windows_64_Debug"),
        help="Directory containing built test executables",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=120,
        help="Per-test timeout in seconds",
    )
    parser.add_argument(
        "--output",
        default=os.path.join("doc", "shader-system", "baseline", "shadergen_profile_parity_latest.json"),
        help="Output JSON summary path",
    )
    parser.add_argument(
        "--tests",
        nargs="*",
        default=None,
        help="Optional explicit executable file names",
    )
    parser.add_argument(
        "--extended",
        action="store_true",
        help="Include integration-heavy tests that may require full runtime context",
    )
    args = parser.parse_args()

    bin_dir = os.path.abspath(args.bin_dir)
    output_path = os.path.abspath(args.output)

    summary = {
        "generated_utc": datetime.utcnow().isoformat() + "Z",
        "bin_dir": bin_dir,
        "tests": [],
        "all_passed": True,
    }

    selected_tests = list(args.tests) if args.tests else list(DEFAULT_SMOKE_TESTS)
    if args.extended and not args.tests:
        selected_tests.extend(EXTENDED_TESTS)

    for test_name in selected_tests:
        exe_path = os.path.join(bin_dir, test_name)
        if not os.path.exists(exe_path):
            summary["tests"].append(
                {
                    "name": test_name,
                    "path": exe_path,
                    "exit_code": -3,
                    "passed": False,
                    "stdout": "",
                    "stderr": "NOT_FOUND",
                    "duration_ms": 0,
                }
            )
            summary["all_passed"] = False
            continue

        test_result = run_test(exe_path, args.timeout)
        summary["tests"].append(test_result)
        if not test_result["passed"]:
            summary["all_passed"] = False

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)

    print(f"[ShaderGenParity] output: {output_path}")
    for t in summary["tests"]:
        status = "PASS" if t["passed"] else "FAIL"
        print(f"[{status}] {t['name']} (exit={t['exit_code']}, {t['duration_ms']}ms)")

    return 0 if summary["all_passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
