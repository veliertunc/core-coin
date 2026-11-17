"""Build with GCC/Clang and always refresh the compilation database first."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "g++"))
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build-local")
    parser.add_argument("--configure-only", action="store_true")
    parser.add_argument("--test", action="store_true")
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error(f"Compiler {args.compiler!r} not found. Run in Ubuntu WSL or select --compiler.")
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    library = sorted(p for p in (ROOT / "src").rglob("*.cpp") if "cli" not in p.relative_to(ROOT / "src").parts)
    app = sorted((ROOT / "src/cli").glob("*.cpp"))
    tests = sorted((ROOT / "tests").glob("*.cpp"))
    gtest_root = ROOT / "third_party/googletest/googletest"
    gtest = [gtest_root / "src/gtest-all.cc", gtest_root / "src/gtest_main.cc"]
    if any(not source.is_file() for source in gtest):
        parser.error("GoogleTest submodule missing; run git submodule update --init --recursive")
    commands = []
    objects = {}
    for source in [*library, *app, *tests, *gtest]:
        obj = build / (source.relative_to(ROOT).as_posix().replace("/", "-") + ".o")
        flags = [compiler, "-std=c++23", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-pthread",
                 "-I", str(ROOT / "include"), "-I", str(ROOT / "src"),
                 "-I", str(gtest_root / "include"), "-I", str(gtest_root)]
        command = flags + ["-c", str(source), "-o", str(obj)]
        commands.append({"directory": str(ROOT), "file": str(source),
                         "arguments": command, "output": str(obj)})
        objects[source] = obj
    # Atomic replacement prevents editors from reading a partially written database.
    content = json.dumps(commands, indent=2) + "\n"
    for path in [ROOT / "compile_commands.json", build / "compile_commands.json"]:
        temporary = path.with_suffix(".json.tmp")
        temporary.write_text(content, encoding="utf-8")
        temporary.replace(path)
    print(f"Updated compile_commands.json: {len(commands)} translation units", flush=True)
    if args.configure_only:
        return
    headers = [*(ROOT / "include").rglob("*.hpp"), *(ROOT / "src").rglob("*.hpp"),
               *(ROOT / "tests").rglob("*.hpp"), Path(__file__)]
    for entry in commands:
        obj = Path(entry["output"])
        signature = obj.with_suffix(".command.json")
        command_text = json.dumps(entry["arguments"])
        dependencies = [Path(entry["file"]), *headers]
        if (not obj.exists() or not signature.exists() or signature.read_text() != command_text
                or any(p.stat().st_mtime > obj.stat().st_mtime for p in dependencies)):
            subprocess.run(entry["arguments"], cwd=ROOT, check=True)
            signature.write_text(command_text)
    suffix = ".exe" if os.name == "nt" else ""
    for name, sources in [("corecoin-cli", [*library, *app]), ("corecoin-tests", [*library, *tests, *gtest])]:
        subprocess.run([compiler, *[str(objects[p]) for p in sources], "-o", str(build / (name + suffix)), "-lcrypto", "-pthread"],
                       cwd=ROOT, check=True)
    if args.test:
        subprocess.run([str(build / ("corecoin-tests" + suffix))], cwd=ROOT, check=True)
        subprocess.run([sys.executable, str(ROOT / "tests/cli_tests.py"),
                        str(build / ("corecoin-cli" + suffix))], cwd=ROOT, check=True)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
