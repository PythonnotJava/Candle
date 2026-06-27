"""
Candle Test Runner — 自动化回归测试
用法: python test/run_tests.py
"""
import subprocess, os, sys

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CANDLEC = os.path.join(BASE, "candlec.exe")

tests = [
    # (name, path, expected_output_contains)
    ("hello",           "grammar/examples/hello.candle",           "hello from interp"),
    ("basic",           "grammar/examples/basic.candle",           "item: 5"),
    ("class",           "grammar/examples/class.candle",           "[INFO] Application started"),
    ("class_test",      "grammar/examples/class_test.candle",      "49"),
    ("control",         "grammar/examples/control.candle",         "Error: division by zero"),
    ("fn_test",         "grammar/examples/fn_test.candle",         "1"),
    ("inherit_test",    "grammar/examples/inherit_test.candle",    "16"),
    ("loop_test",       "grammar/examples/loop_test.candle",       "9"),
    ("parallel",        "grammar/examples/parallel.candle",        "Final: 449985000"),
    ("reflect_test",    "grammar/examples/reflect_test.candle",    "red"),
    ("stdlib_v2",       "test/stdlib_test_v2.candle",              "ALL STDLIB TESTS PASSED"),
    ("double",          "test/double/double_test.candle",          "DOUBLE STDLIB TESTS PASSED"),
    ("double_builtin",  "test/double/double_builtin_test.candle",  "BUILTIN FALLBACK TESTS PASSED"),
    ("union_types",     "test/union_test.candle",                  "UNION TYPE TESTS PASSED"),
    ("generic_types",   "test/generic_test.candle",                "GENERIC TYPE TESTS PASSED"),
    ("json",            "test/json_test.candle",                    "JSON TESTS PASSED"),
    ("http",            "test/http_test.candle",                    "HTTP TESTS PASSED"),
    ("test_all",        "test/test_all/test_all.candle",             "ALL KEYWORD TESTS PASSED"),
    ("time",            "test/time/time_test.candle",                 "STD.TIME TESTS PASSED"),
    ("parallel_bench",  "test/parallel_bench/parallel_bench.candle",   "PARALLEL BENCH PASSED"),
]

passed = 0
failed = 0

for name, path, expected in tests:
    full_path = os.path.join(BASE, path)
    try:
        result = subprocess.run(
            [CANDLEC, "--run", full_path],
            capture_output=True, timeout=30,
            cwd=BASE,
            encoding="utf-8", errors="replace"
        )
        combined = result.stdout + result.stderr
        if expected in combined and result.returncode == 0:
            print(f"  OK  {name}")
            passed += 1
        else:
            print(f"  FAIL  {name}  (rc={result.returncode})")
            for line in combined.split('\n'):
                if 'error' in line.lower() or 'Error' in line:
                    print(f"        {line.strip()[:120]}")
                    break
            failed += 1
    except subprocess.TimeoutExpired:
        print(f"  TIMEOUT  {name}")
        failed += 1
    except Exception as e:
        print(f"  ERROR  {name}: {e}")
        failed += 1

print(f"\n{'='*50}")
print(f"  Passed: {passed}/{passed+failed}")
if failed:
    print(f"  Failed: {failed}")
print(f"{'='*50}")
sys.exit(0 if failed == 0 else 1)
