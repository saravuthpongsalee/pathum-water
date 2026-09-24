"""Merge board-tested Arduino binaries for the HTTPS installer on GitHub Pages.

This script cannot compile the sketch and never creates dummy firmware.
"""
import argparse
import pathlib
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description="Prepare ESP32-CYD ST7789 merged.bin")
    parser.add_argument("--bootloader", required=True, type=pathlib.Path)
    parser.add_argument("--partitions", required=True, type=pathlib.Path)
    parser.add_argument("--boot-app0", required=True, type=pathlib.Path)
    parser.add_argument("--application", required=True, type=pathlib.Path)
    args = parser.parse_args()
    for label, source in vars(args).items():
        if not source.is_file() or source.stat().st_size < 32:
            parser.error(f"{label}: binary is missing or empty: {source}")
    output = pathlib.Path(__file__).resolve().parent / "docs" / "firmware" / "merged.bin"
    output.parent.mkdir(parents=True, exist_ok=True)
    cmd = [sys.executable, "-m", "esptool", "--chip", "esp32", "merge-bin",
           "-o", str(output), "--flash-mode", "dio", "--flash-freq", "40m",
           "--flash-size", "4MB", "0x1000", str(args.bootloader),
           "0x8000", str(args.partitions), "0xe000", str(args.boot_app0),
           "0x10000", str(args.application)]
    subprocess.run(cmd, check=True)
    if output.stat().st_size < 65536:
        raise SystemExit("Merged binary is too small; check the supplied binaries.")
    print("Merged ESP32 firmware:", output)
    print("Test this merged binary on the same ST7789 board before public upload.")


if __name__ == "__main__":
    main()
