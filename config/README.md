# Build configuration

These files describe the H8 build and the tools used around it. `configure.py`
reads them to generate `build.ninja`; the build utilities also record their
contents with the produced firmware.

| File | Purpose |
| --- | --- |
| `build.json` | Shared compiler options, per-module flag overrides, linker sections, and the retail image's size and SHA-256 |
| `artwork.json` | Resident asset names, ROM extraction ranges, image dimensions, packed pixel formats, and bytes shared with audio timing |
| `toolchains.json` | Fingerprints of the qualified 6.02.01 and 6.02.02 suites and known updater packages |
| `host-tools.json` | Managed Wibo releases, download locations, and integrity hashes |
| `h8.xml` | Cppcheck's target widths and alignment for the H8 C implementation |
| `lint-suppressions.txt` | Two narrow Cppcheck exceptions for preserved target arithmetic |

Compiler fingerprints describe tested inputs. Other complete installations can
be imported and built with a warning. Each build records the actual compiler
files selected on that machine. Downloaded Wibo executables must pass their
recorded integrity hash before use.

`tools/modules.py` discovers C files recursively under `src/` and sorts their
unique lowercase basenames. Configuration, compilation, linking, and receipt
validation use the resulting module list. Directory paths serve navigation;
basename order determines object order.

`build.json` supplies extra compiler options through `source_flags`, keyed by
the full lowercase basename including `.c`:

```json
{
  "source_flags": {
    "h8_rominfo.c": ["-section=const=M"],
    "pw_fourier.c": ["-speed=shift,loop=2"],
    "pw_pedometer.c": ["-speed"]
  }
}
```

Other modules use the common compiler options. An override must name an
existing source. Project `#include` directives name headers relative to
`include/`, such as `application/pw_pedometer.h` or `support/lib_common.h`.

Source order, integer widths, compiler flags, and linker placement can affect
retail identity. After changing them, build under both qualified suites and run
`uv run ninja verify` for each. The [build guide](../docs/build.md) explains the
commands, and the [artwork guide](../assets/README.md) explains the image formats.
