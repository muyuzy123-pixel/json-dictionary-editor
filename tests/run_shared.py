#!/usr/bin/env python3
"""Run shared fixtures against the actual, separately compiled JSON implementations."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--swift-adapter", type=Path)
    parser.add_argument("--cpp-adapter", type=Path)
    parser.add_argument("--require-both", action="store_true")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    adapters = {name: path.resolve() for name, path in
                (("swift", args.swift_adapter), ("cpp", args.cpp_adapter)) if path}
    if not adapters or (args.require_both and len(adapters) != 2):
        parser.error("provide adapter(s); --require-both requires both implementations")
    fixtures_path = Path(__file__).resolve().parent / "fixtures" / "cases.json"
    fixtures_bytes = fixtures_path.read_bytes()
    cases = json.loads(fixtures_bytes)["cases"]
    results = []
    for name, adapter in adapters.items():
        for case in cases:
            if name not in case.get("platforms", ["swift", "cpp"]):
                continue
            payload = bytes.fromhex(case["input_hex"]) if "input_hex" in case else case["input"].encode("utf-8")
            expected = case.get("expect_" + name, case["expect"])
            command = [str(adapter), "-", case.get("format", "compact"),
                       case.get("root", "object"), case.get("newline", "false")]
            result = {"platform": name, "case": case["id"], "expected_accept": expected["accept"]}
            try:
                process = subprocess.run(command, input=payload, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
                expected_code = 0 if expected["accept"] else 1
                passed = process.returncode == expected_code
                if expected["accept"]:
                    passed = passed and process.stdout == expected["output"].encode("utf-8")
                else:
                    passed = passed and not process.stdout and bool(process.stderr)
                result.update(passed=passed, exit_code=process.returncode,
                              output_sha256=hashlib.sha256(process.stdout).hexdigest(),
                              diagnostic=process.stderr.decode("utf-8", errors="replace")[:1000])
            except (OSError, subprocess.TimeoutExpired) as error:
                result.update(passed=False, diagnostic=str(error))
            results.append(result)
            print(("PASS" if result["passed"] else "FAIL") + " " + name + "/" + case["id"])
    report = {
        "scope": "parser, dictionary-root validation and writer only; no GUI or file-layer acceptance",
        "fixture_sha256": hashlib.sha256(fixtures_bytes).hexdigest(),
        "adapters": {name: {"sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                     for name, path in adapters.items() if path.is_file()},
        "passed": sum(result["passed"] for result in results),
        "total": len(results), "results": results,
    }
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f'SHARED_FIXTURES: {report["passed"]}/{report["total"]} passed; implementations={",".join(adapters)}')
    return 0 if report["passed"] == report["total"] else 1


if __name__ == "__main__":
    sys.exit(main())
