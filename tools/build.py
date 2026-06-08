#!/usr/bin/env python3

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build"

CC = "cc"
COMMON_FLAGS = ["-std=c17", "-Wall", "-Wextra", "-g"]
SANITIZER_FLAGS = ["-fsanitize=address,undefined"]

APP_SOURCES = [
    "main.c",
    "chunk.c",
    "debug.c",
    "lines.c",
    "memory.c",
    "value.c",
]

TEST_SOURCES = [
    "memory.c",
    "tests/test_main.c",
    "tests/memory_arena_test.c",
    "tests/memory_heap_test.c",
]


def source_paths(sources):
    return [ROOT / source for source in sources]


def run(command):
    printable = " ".join(str(part) for part in command)
    print(f"+ {printable}", flush=True)
    return subprocess.run(command, cwd=ROOT).returncode


def build(output, sources, sanitize):
    BUILD_DIR.mkdir(exist_ok=True)

    command = [CC, *COMMON_FLAGS]
    if sanitize:
        command.extend(SANITIZER_FLAGS)
    command.extend(source_paths(sources))
    command.extend(["-o", output])

    return run(command)


def build_app(args):
    return build(BUILD_DIR / "clox", APP_SOURCES, args.sanitize)


def build_tests(args):
    output = BUILD_DIR / "clox_tests"
    sanitize = not args.no_sanitize

    if args.verbose:
        print("target: test", flush=True)
        print(f"output: {output.relative_to(ROOT)}", flush=True)
        print(f"compiler: {CC}", flush=True)
        print(f"sanitize: {'enabled' if sanitize else 'disabled'}", flush=True)

    result = build(output, TEST_SOURCES, sanitize)
    if result != 0:
        return result

    command = [output]
    if args.verbose:
        command.append("--verbose")
    return run(command)


def clean(_args):
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    return 0


def parse_args():
    parser = argparse.ArgumentParser(description="Build clox targets.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    app = subparsers.add_parser("app", help="Build the clox executable.")
    app.add_argument(
        "--sanitize",
        action="store_true",
        help="Build the app with address and undefined behavior sanitizers.",
    )
    app.set_defaults(func=build_app)

    test = subparsers.add_parser("test", help="Build and run tests.")
    test.add_argument(
        "--no-sanitize",
        action="store_true",
        help="Build tests without address and undefined behavior sanitizers.",
    )
    test.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Print test build configuration and per-test run lines.",
    )
    test.set_defaults(func=build_tests)

    clean_cmd = subparsers.add_parser("clean", help="Remove build outputs.")
    clean_cmd.set_defaults(func=clean)

    return parser.parse_args()


def main():
    args = parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
