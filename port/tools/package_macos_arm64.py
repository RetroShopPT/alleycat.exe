#!/usr/bin/env python3
"""Package a locally built ARM64 executable and SDL2 into a signed .app."""
import argparse
from pathlib import Path
import plistlib
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("framework", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--version", default="1.3")
    parser.add_argument("--minimum-os", default="11.0")
    parser.add_argument("--icon", type=Path, help="Optional locally supplied .icns")
    args = parser.parse_args()
    if args.output.suffix != ".app" or not args.binary.is_file() or not args.framework.is_dir():
        parser.error("Expected an executable, SDL2.framework and an output .app path")
    subprocess.run(["lipo", "-verify_arch", "arm64", str(args.binary)], check=True)
    subprocess.run(["lipo", "-verify_arch", "arm64", str(args.framework / "SDL2")], check=True)
    if args.output.exists():
        parser.error("Output already exists; remove the generated .app before packaging again")
    contents = args.output / "Contents"
    macos = contents / "MacOS"
    resources = contents / "Resources"
    macos.mkdir(parents=True)
    resources.mkdir()
    shutil.copyfile(args.binary, macos / "alleycat")
    (macos / "alleycat").chmod(0o755)
    subprocess.run(["ditto", "--norsrc", "--noextattr", str(args.framework),
                    str(macos / "SDL2.framework")], check=True)
    info = {
        "CFBundleDisplayName": "Alley Cat", "CFBundleName": "Alley Cat",
        "CFBundleExecutable": "alleycat", "CFBundleIdentifier": "com.local.alleycat",
        "CFBundlePackageType": "APPL", "CFBundleShortVersionString": args.version,
        "CFBundleVersion": args.version + ".1", "LSMinimumSystemVersion": args.minimum_os,
        "NSHighResolutionCapable": True,
    }
    if args.icon:
        shutil.copyfile(args.icon, resources / "AlleyCat.icns")
        info["CFBundleIconFile"] = "AlleyCat"
    (contents / "Info.plist").write_bytes(plistlib.dumps(info))
    license_file = Path(__file__).resolve().parents[2] / "LICENSE"
    shutil.copyfile(license_file, resources / "LICENSE-port.txt")
    (resources / "SOURCE.txt").write_text(
        "Alley Cat ARM64 port: https://github.com/RetroShopPT/alleycat.exe\n"
        "Based on https://github.com/petarov/alleycat.exe (version 1.3).\n"
        "Original game: Bill Williams, SynSoft/IBM, 1984.\n"
        "The port's Unlicense does not cover the original game data.\n")
    subprocess.run(["xattr", "-cr", str(args.output)], check=True)
    subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(args.output)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(args.output)], check=True)
    print(args.output)


if __name__ == "__main__":
    main()
