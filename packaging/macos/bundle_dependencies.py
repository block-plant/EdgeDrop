#!/usr/bin/env python3
"""
EdgeDrop macOS Dependency Bundler & Homebrew Stripper.

1. Strips all /opt/homebrew LC_RPATH entries from all Mach-O binaries in the bundle.
2. Ensures @executable_path/../Frameworks is in edgedrop's LC_RPATH.
3. Recursively bundles missing @rpath and /opt/homebrew dylibs until convergence.
4. Normalizes install IDs (LC_ID_DYLIB) so none point to /opt/homebrew.
5. Enforces a strict Zero-Homebrew verification gate.
"""

import os
import re
import shutil
import subprocess
import sys

# Priority search directories for Homebrew dylibs
HOMEBREW_SEARCH_DIRS = [
    "/opt/homebrew/lib",
    "/opt/homebrew/opt/opencv/lib",
    "/opt/homebrew/opt/gcc/lib/gcc/current",
    "/opt/homebrew/opt/libomp/lib",
    "/opt/homebrew/opt/tbb/lib",
    "/opt/homebrew/opt/openblas/lib",
    "/opt/homebrew/opt/qpdf/lib",
    "/opt/homebrew/opt/openssl@4/lib",
    "/opt/homebrew/opt/jpeg-turbo/lib",
    "/opt/homebrew/opt/webp/lib",
    "/opt/homebrew/opt/libpng/lib",
    "/opt/homebrew/opt/libtiff/lib",
    "/opt/homebrew/opt/openjpeg/lib",
    "/opt/homebrew/opt/openexr/lib",
    "/opt/homebrew/opt/imath/lib",
    "/opt/homebrew/opt/zstd/lib",
    "/opt/homebrew/opt/xz/lib",
]


def is_macho(path):
    """Check if a file is a Mach-O binary (and not a symlink)."""
    if not os.path.isfile(path) or os.path.islink(path):
        return False
    try:
        with open(path, "rb") as fp:
            magic = fp.read(4)
            return magic in (
                b"\xfe\xed\xfa\xce",
                b"\xce\xfa\xed\xfe",
                b"\xfe\xed\xfa\xcf",
                b"\xcf\xfa\xed\xfe",
                b"\xca\xfe\xba\xbe",
                b"\xbe\xba\xfe\xca",
            )
    except Exception:
        return False


def find_all_machos(root_dir):
    """Find all regular Mach-O files under root_dir."""
    machos = []
    for root, dirs, files in os.walk(root_dir):
        for f in files:
            p = os.path.join(root, f)
            if is_macho(p):
                machos.append(p)
    return sorted(machos)


def get_rpaths(path):
    """Extract all LC_RPATH entries from a Mach-O file."""
    try:
        out = subprocess.check_output(
            ["otool", "-l", path], text=True, stderr=subprocess.DEVNULL
        )
    except Exception:
        return []
    rpaths = []
    lines = out.splitlines()
    for i, line in enumerate(lines):
        if line.strip() == "cmd LC_RPATH":
            for j in range(i + 1, min(i + 5, len(lines))):
                m = re.match(r"^\s*path\s+(.*?)\s*\(offset \d+\)$", lines[j])
                if m:
                    rpaths.append(m.group(1))
                    break
    return rpaths


def get_install_id(path):
    """Get LC_ID_DYLIB install ID if present, or None."""
    try:
        out = subprocess.check_output(
            ["otool", "-D", path], text=True, stderr=subprocess.DEVNULL
        )
        lines = [
            l.strip()
            for l in out.splitlines()[1:]
            if l.strip() and not l.strip().endswith(":")
        ]
        if lines:
            return lines[0]
    except Exception:
        pass
    return None


def get_deps(path):
    """Get list of dependency paths from otool -L, excluding header."""
    try:
        out = subprocess.check_output(
            ["otool", "-L", path], text=True, stderr=subprocess.DEVNULL
        )
    except Exception:
        return []
    lines = out.splitlines()[1:]
    deps = []
    for line in lines:
        line = line.strip()
        if not line:
            continue
        dep_path = line.split()[0]
        deps.append(dep_path)
    return deps


def find_in_homebrew(basename):
    """Locate a library by basename across Homebrew search paths."""
    for d in HOMEBREW_SEARCH_DIRS:
        cand = os.path.join(d, basename)
        if os.path.isfile(cand):
            return os.path.realpath(cand)

    for root_dir in ["/opt/homebrew/opt", "/opt/homebrew/lib"]:
        if os.path.exists(root_dir):
            for root, _, files in os.walk(root_dir):
                if basename in files:
                    cand = os.path.join(root, basename)
                    if os.path.isfile(cand):
                        return os.path.realpath(cand)
    return None


def copy_and_normalize(source_path, dest_path, install_id, frameworks_dir):
    """Copy source to dest if missing, make writable, set install ID, and create alias symlink if needed."""
    if not os.path.exists(dest_path):
        print(f"  [+] Bundling: {os.path.basename(dest_path)}")
        shutil.copyfile(source_path, dest_path)
        os.chmod(dest_path, 0o755)

    if os.path.exists(dest_path) and not os.path.islink(dest_path):
        os.chmod(dest_path, 0o755)
        current_id = get_install_id(dest_path)
        if current_id != install_id:
            subprocess.run(
                ["install_name_tool", "-id", install_id, dest_path], check=True
            )

    # If the real file has an alternative basename (e.g. versioned .5.0.0.dylib vs .500.dylib), create symlink
    source_base = os.path.basename(source_path)
    dest_base = os.path.basename(dest_path)
    if source_base != dest_base:
        alt_dest = os.path.join(frameworks_dir, source_base)
        if not os.path.exists(alt_dest):
            try:
                os.symlink(dest_base, alt_dest)
            except Exception:
                pass


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <path_to_app_bundle>")
        sys.exit(1)

    app_bundle = os.path.abspath(sys.argv[1])
    frameworks_dir = os.path.join(app_bundle, "Contents", "Frameworks")
    macos_dir = os.path.join(app_bundle, "Contents", "MacOS")
    edgedrop_bin = os.path.join(macos_dir, "edgedrop")

    if not os.path.isdir(app_bundle):
        print(f"Error: Bundle directory not found: {app_bundle}", file=sys.stderr)
        sys.exit(1)

    os.makedirs(frameworks_dir, exist_ok=True)

    print("=== Step 1: Strip Homebrew LC_RPATHs & Ensure Frameworks RPATH ===")
    machos = find_all_machos(app_bundle)
    print(f"Found {len(machos)} Mach-O binaries in bundle.")

    for m in machos:
        rpaths = get_rpaths(m)
        for rp in rpaths:
            if "/opt/homebrew" in rp:
                print(f"  [-] Deleting Homebrew LC_RPATH '{rp}' from {os.path.basename(m)}")
                os.chmod(m, 0o755)
                subprocess.run(
                    ["install_name_tool", "-delete_rpath", rp, m], check=True
                )

    if os.path.isfile(edgedrop_bin):
        rpaths = get_rpaths(edgedrop_bin)
        if "@executable_path/../Frameworks" not in rpaths:
            print("  [+] Adding @executable_path/../Frameworks to edgedrop LC_RPATH")
            os.chmod(edgedrop_bin, 0o755)
            subprocess.run(
                [
                    "install_name_tool",
                    "-add_rpath",
                    "@executable_path/../Frameworks",
                    edgedrop_bin,
                ],
                check=True,
            )

    print("\n=== Step 2: Recursive Dependency Bundling & RPATH Normalization ===")
    iteration = 0
    max_iterations = 25

    while iteration < max_iterations:
        iteration += 1
        print(f"--- Convergence Pass {iteration} ---")
        changed = False
        machos = find_all_machos(app_bundle)

        for target in machos:
            target_name = os.path.basename(target)
            target_id = get_install_id(target)

            # 2a. Normalize target install ID if it contains /opt/homebrew
            if target_id and "/opt/homebrew" in target_id:
                os.chmod(target, 0o755)
                m = re.search(r"([^/]+\.framework/Versions/[^/]+/[^/]+)$", target)
                if m:
                    new_id = f"@rpath/{m.group(1)}"
                else:
                    new_id = f"@executable_path/../Frameworks/{target_name}"
                print(f"  [*] Normalizing install ID of {target_name}: {new_id}")
                subprocess.run(
                    ["install_name_tool", "-id", new_id, target], check=True
                )
                changed = True
                target_id = new_id

            # 2b. Check dependencies
            deps = get_deps(target)
            for dep in deps:
                # Skip self-ID
                if target_id and dep == target_id:
                    continue

                # Case 1: Dependency has /opt/homebrew/ and is inside a .framework
                if "/opt/homebrew" in dep and ".framework" in dep:
                    m = re.search(r"([^/]+\.framework/Versions/[^/]+/[^/]+)$", dep)
                    if m:
                        new_dep = f"@rpath/{m.group(1)}"
                        print(f"  [*] Rewriting {target_name}: {dep} -> {new_dep}")
                        os.chmod(target, 0o755)
                        subprocess.run(
                            ["install_name_tool", "-change", dep, new_dep, target],
                            check=True,
                        )
                        changed = True
                        continue

                # Case 2: Dependency starts with /opt/homebrew/ (standalone dylib)
                if dep.startswith("/opt/homebrew/"):
                    base = os.path.basename(dep)
                    dest = os.path.join(frameworks_dir, base)
                    real_source = None
                    if os.path.isfile(dep):
                        real_source = os.path.realpath(dep)
                    else:
                        real_source = find_in_homebrew(base)

                    if not real_source:
                        print(
                            f"ERROR: Cannot locate Homebrew dependency: {dep}",
                            file=sys.stderr,
                        )
                        sys.exit(1)

                    new_id = f"@executable_path/../Frameworks/{base}"
                    copy_and_normalize(real_source, dest, new_id, frameworks_dir)

                    new_dep = f"@executable_path/../Frameworks/{base}"
                    print(f"  [*] Rewriting {target_name}: {dep} -> {new_dep}")
                    os.chmod(target, 0o755)
                    subprocess.run(
                        ["install_name_tool", "-change", dep, new_dep, target],
                        check=True,
                    )
                    changed = True
                    continue

                # Case 3: Dependency is @rpath/*.dylib
                if dep.startswith("@rpath/") and dep.endswith(".dylib"):
                    base = os.path.basename(dep)
                    dest = os.path.join(frameworks_dir, base)
                    if not os.path.exists(dest):
                        real_source = find_in_homebrew(base)
                        if not real_source:
                            print(
                                f"ERROR: Cannot locate @rpath dylib: {dep}",
                                file=sys.stderr,
                            )
                            sys.exit(1)
                        new_id = f"@executable_path/../Frameworks/{base}"
                        copy_and_normalize(
                            real_source, dest, new_id, frameworks_dir
                        )

                    new_dep = f"@executable_path/../Frameworks/{base}"
                    if dep != new_dep:
                        print(f"  [*] Rewriting {target_name}: {dep} -> {new_dep}")
                        os.chmod(target, 0o755)
                        subprocess.run(
                            ["install_name_tool", "-change", dep, new_dep, target],
                            check=True,
                        )
                        changed = True
                        continue

        if not changed:
            print(f"Convergence achieved after {iteration} pass(es)!")
            break

    if iteration >= max_iterations:
        print(
            "WARNING: Max iterations reached without clean convergence.",
            file=sys.stderr,
        )

    print("\n=== Step 3: Zero-Homebrew Verification Gate ===")
    machos = find_all_machos(app_bundle)
    gate_errors = []

    for target in machos:
        rel_path = os.path.relpath(target, app_bundle)

        # 1. Check LC_RPATH
        for rp in get_rpaths(target):
            if "/opt/homebrew" in rp:
                gate_errors.append(f"{rel_path}: LC_RPATH contains /opt/homebrew: {rp}")

        # 2. Check install ID
        target_id = get_install_id(target)
        if target_id and "/opt/homebrew" in target_id:
            gate_errors.append(
                f"{rel_path}: install ID contains /opt/homebrew: {target_id}"
            )

        # 3. Check dependencies
        deps = get_deps(target)
        for dep in deps:
            if target_id and dep == target_id:
                continue
            if "/opt/homebrew" in dep:
                gate_errors.append(
                    f"{rel_path}: dependency contains /opt/homebrew: {dep}"
                )
            elif dep.startswith("@rpath/") and dep.endswith(".dylib"):
                base = os.path.basename(dep)
                dest = os.path.join(frameworks_dir, base)
                if not os.path.exists(dest):
                    gate_errors.append(
                        f"{rel_path}: @rpath dylib missing from Frameworks: {dep}"
                    )

    if gate_errors:
        print("VERIFICATION GATE FAILED:", file=sys.stderr)
        for err in gate_errors:
            print(f"  [!] {err}", file=sys.stderr)
        sys.exit(1)

    print("VERIFICATION GATE PASSED:")
    print(f"  - Verified {len(machos)} Mach-O binaries in deployed bundle.")
    print("  - Zero /opt/homebrew LC_RPATH entries.")
    print("  - Zero /opt/homebrew dependency load commands.")
    print("  - All transitive @rpath dylibs present in Contents/Frameworks.")


if __name__ == "__main__":
    main()
