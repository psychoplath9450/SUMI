"""
Post-build script: merge bootloader + partitions + firmware into a single
flashable image, and copy it to the firmware project root for release.

WHY THIS EXISTS:
v0.5.0 was initially shipped by uploading `.pio/build/gh_release/firmware.bin`
directly to the web flasher. That file is ONLY the application — no bootloader,
no partition table. The web flasher wrote it to offset 0x0, clobbering the
bootloader region, and bricked every device that ran the update.

This script eliminates that failure mode: after every build it produces a
properly merged `sumi-v<VERSION>-full.bin` sitting right next to platformio.ini.
That is the ONLY file that should ever be shipped to the web flasher.

The raw `firmware.bin` stays buried in `.pio/build/<env>/` where nobody reaches
for it by accident.
"""
import os
import shutil
import subprocess
import sys

Import("env")  # noqa: F821 - provided by PlatformIO


def _extract_version(scons_env) -> str:
    """Pull SUMI_VERSION out of CPPDEFINES (it is set per-env in platformio.ini)."""
    for define in scons_env.get("CPPDEFINES", []):
        if isinstance(define, (list, tuple)) and len(define) == 2 and define[0] == "SUMI_VERSION":
            # Value arrives with escaped quotes: '\\"0.5.0\\"' — scrub both.
            # str.strip() only peels from the ends and can leave a stray
            # backslash when the pattern is \"X\"; replace handles it cleanly.
            return str(define[1]).replace('\\', '').replace('"', '').strip()
    return "unknown"


# SCons post-action callbacks are invoked with (target, source, env) by name,
# so the parameter MUST be called `env` even though the module-level `env`
# is also in scope from `Import("env")`. The local parameter shadows it.
def merge_firmware(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    python_exe = env.subst("$PYTHONEXE")
    version = _extract_version(env)

    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    firmware = os.path.join(build_dir, "firmware.bin")

    for required in (bootloader, partitions, firmware):
        if not os.path.isfile(required):
            print(f"[merge_firmware] SKIP — missing {required}")
            return

    merged_in_build = os.path.join(build_dir, "sumi-full.bin")
    merged_in_root = os.path.join(project_dir, f"sumi-v{version}-full.bin")

    cmd = [
        python_exe, "-m", "esptool",
        "--chip", "esp32c3",
        "merge-bin",
        "-o", merged_in_build,
        "--flash-mode", "dio",
        "--flash-freq", "80m",
        "--flash-size", "16MB",
        "0x0", bootloader,
        "0x8000", partitions,
        "0x10000", firmware,
    ]

    print(f"[merge_firmware] Merging bootloader + partitions + firmware -> {merged_in_build}")
    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as exc:
        print(f"[merge_firmware] esptool merge-bin FAILED: {exc}", file=sys.stderr)
        env.Exit(1)  # fail the build — do not let a broken release slip through
        return

    shutil.copy2(merged_in_build, merged_in_root)

    size_kb = os.path.getsize(merged_in_root) // 1024
    banner = "=" * 72
    print()
    print(banner)
    print(f" MERGED FIRMWARE READY  ({size_kb} KB)")
    print(f"   {merged_in_root}")
    print()
    print(" SHIP THIS FILE to the web flasher.")
    print(" DO NOT SHIP .pio/build/*/firmware.bin — that will brick devices.")
    print(banner)
    print()


# Hook into the final program build step.
env.AddPostAction("buildprog", merge_firmware)  # noqa: F821
